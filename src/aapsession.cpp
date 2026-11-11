// SPDX-License-Identifier: MIT
#include "aapsession.h"

#include "aapprotocol.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QStringList>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <optional>

namespace
{
// "38:C4:3A:7B:06:85" -> bdaddr_t, which BlueZ stores in reverse byte order.
std::optional<bdaddr_t> toBdaddr(const QString &address)
{
    const QStringList bytes = address.split(QLatin1Char(':'));
    if (bytes.size() != 6) {
        return std::nullopt;
    }
    bdaddr_t result{};
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        result.b[5 - i] = static_cast<uint8_t>(bytes[i].toUInt(&ok, 16));
        if (!ok) {
            return std::nullopt;
        }
    }
    return result;
}

// The default Options::connector: a non-blocking L2CAP socket connecting to the AAP channel.
int connectL2cap(const QString &address)
{
    const auto bdaddr = toBdaddr(address);
    if (!bdaddr) {
        errno = EINVAL;
        return -1;
    }
    // Fails e.g. while the adapter is being reset, or when we ran out of descriptors.
    const int socket = ::socket(AF_BLUETOOTH, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, BTPROTO_L2CAP);
    if (socket < 0) {
        return -1;
    }

    sockaddr_l2 target{};
    target.l2_family = AF_BLUETOOTH;
    target.l2_psm = htobs(Aap::Psm);
    target.l2_bdaddr = *bdaddr;

    if (::connect(socket, reinterpret_cast<sockaddr *>(&target), sizeof(target)) < 0 && errno != EINPROGRESS) {
        const int error = errno;
        ::close(socket);
        errno = error;
        return -1;
    }
    return socket;
}

// Closes and frees a notifier safely, even from inside its own activated() signal.
void discard(QSocketNotifier *&notifier)
{
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
        notifier = nullptr;
    }
}
}

AapSession::AapSession(const QString &address, QObject *parent)
    : AapSession(address, Options{}, parent)
{
}

AapSession::AapSession(const QString &address, const Options &options, QObject *parent)
    : QObject(parent)
    , m_address(address)
    , m_options(options)
{
    if (!m_options.connector) {
        m_options.connector = connectL2cap;
    }

    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &AapSession::connectToAccessory);

    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        qCDebug(DEVICEBATTERY) << "AAP session with" << m_address << (m_reported ? "went silent" : "sent no battery report") << "- reconnecting";
        closeSocket();
        retryLater();
    });

    m_retryTimer.start(m_options.settleDelay);
}

AapSession::~AapSession()
{
    closeSocket();
}

void AapSession::connectToAccessory()
{
    m_socket = m_options.connector(m_address);
    if (m_socket < 0) {
        qCDebug(DEVICEBATTERY) << "AAP connect to" << m_address << "failed:" << std::strerror(errno);
        m_socket = -1;
        retryLater();
        return;
    }

    m_reported = false;
    m_watchdog.start(m_options.responseTimeout);

    // A non-blocking connect completes when the socket becomes writable.
    m_writeNotifier = new QSocketNotifier(m_socket, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &AapSession::onConnectFinished);
}

void AapSession::onConnectFinished()
{
    discard(m_writeNotifier);

    int error = 0;
    socklen_t length = sizeof(error);
    if (::getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
        error = errno;
    }
    if (error != 0) {
        qCDebug(DEVICEBATTERY) << "AAP connect to" << m_address << "failed:" << std::strerror(error);
        closeSocket();
        retryLater();
        return;
    }

    qCDebug(DEVICEBATTERY) << "AAP connected to" << m_address;
    m_readNotifier = new QSocketNotifier(m_socket, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &AapSession::onReadable);

    send(Aap::handshakePacket());
    send(Aap::requestNotificationsPacket());
}

void AapSession::onReadable()
{
    // SOCK_SEQPACKET keeps message boundaries: each recv() returns exactly one packet.
    // Packets larger than the buffer (firmware info, HID descriptors) get truncated,
    // which is fine: battery packets are about 20 bytes.
    char buffer[1024];
    while (true) {
        const ssize_t size = ::recv(m_socket, buffer, sizeof(buffer), 0);
        if (size > 0) {
            if (auto parts = Aap::parseBatteryPacket(QByteArrayView(buffer, size))) {
                m_failedAttempts = 0; // only a real battery report proves the session works
                m_reported = true;
                // The raw bytes let users of other AirPods/Beats models report what their
                // accessory sends, e.g. components or status values we do not know yet.
                qCDebug(DEVICEBATTERY).noquote() << "Battery report from" << m_address << "with" << parts->size()
                                                 << "components:" << QByteArray(buffer, size).toHex(' ');
                Q_EMIT batteryChanged(*parts);
            } else if (const auto placements = Aap::parseEarDetectionPacket(QByteArrayView(buffer, size))) {
                qCDebug(DEVICEBATTERY).noquote() << "Ear detection report from" << m_address << ":" << QByteArray(buffer, size).toHex(' ');
                Q_EMIT placementsChanged(placements->first, placements->second);
            } else if (const auto mode = Aap::parseNoiseControlPacket(QByteArrayView(buffer, size))) {
                qCDebug(DEVICEBATTERY).noquote() << "Noise control report from" << m_address << ":" << QByteArray(buffer, size).toHex(' ');
                Q_EMIT noiseControlChanged(*mode);
            }
            if (m_reported) {
                m_watchdog.start(m_options.idleTimeout); // any traffic proves the session is alive
            }
            continue;
        }
        if (size < 0 && errno == EAGAIN) { // EAGAIN == EWOULDBLOCK on Linux
            return; // drained
        }
        if (size < 0 && errno == EINTR) {
            continue;
        }
        qCDebug(DEVICEBATTERY) << "AAP connection to" << m_address << "closed";
        closeSocket();
        retryLater();
        return;
    }
}

void AapSession::send(const QByteArray &packet)
{
    if (::send(m_socket, packet.constData(), packet.size(), MSG_NOSIGNAL) < 0) {
        qCDebug(DEVICEBATTERY) << "AAP send to" << m_address << "failed:" << std::strerror(errno);
    }
}

void AapSession::closeSocket()
{
    m_watchdog.stop();
    discard(m_readNotifier);
    discard(m_writeNotifier);
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
}

void AapSession::retryLater()
{
    ++m_failedAttempts;
    if (m_failedAttempts == m_options.failuresUntilLost) {
        qCDebug(DEVICEBATTERY) << "AAP for" << m_address << "failed" << m_failedAttempts << "times in a row, dropping its levels";
        Q_EMIT batteryLost();
    }
    const auto delay = std::min(m_options.retryStep * m_failedAttempts, m_options.maxRetryDelay);
    qCDebug(DEVICEBATTERY) << "Retrying AAP for" << m_address << "in" << delay.count() << "ms";
    m_retryTimer.start(delay);
}

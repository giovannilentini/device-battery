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
// Retry delays grow by RetryStep (3 s, 6 s, 9 s, ...) up to MaxRetryDelay, and retries never
// stop: a session only exists while BlueZ says the device is connected, so it is worth
// waiting for, and an idle retry costs one connect attempt every couple of minutes.
// After FailuresUntilLost failures in a row the last levels are withdrawn as stale.
constexpr auto RetryStep = std::chrono::seconds(3);
constexpr auto MaxRetryDelay = std::chrono::minutes(2);
constexpr int FailuresUntilLost = 5;

// Right after the Bluetooth link comes up, AirPods accept the L2CAP channel but ignore the
// handshake, so wait for them to settle before the first attempt.
constexpr auto SettleDelay = std::chrono::seconds(2);

// The watchdog reconnects a session that is not delivering data:
// - an attempt with no battery report after ResponseTimeout failed (the connect hung, or the
//   accessory accepted the channel but ignores us, e.g. while another app holds the session);
// - a working session with no packet at all for IdleTimeout went stale. AirPods in use send
//   something every minute or two, so this only triggers when the session really died.
constexpr auto ResponseTimeout = std::chrono::seconds(8);
constexpr auto IdleTimeout = std::chrono::minutes(5);

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
    : QObject(parent)
    , m_address(address)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &AapSession::connectToAccessory);

    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        qCDebug(DEVICEBATTERY) << "AAP session with" << m_address << (m_reported ? "went silent" : "sent no battery report") << "- reconnecting";
        closeSocket();
        retryLater();
    });

    m_retryTimer.start(SettleDelay);
}

AapSession::~AapSession()
{
    closeSocket();
}

void AapSession::connectToAccessory()
{
    const auto bdaddr = toBdaddr(m_address);
    if (!bdaddr) {
        qCWarning(DEVICEBATTERY) << "Invalid Bluetooth address" << m_address;
        return;
    }

    m_socket = ::socket(AF_BLUETOOTH, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, BTPROTO_L2CAP);
    if (m_socket < 0) {
        qCWarning(DEVICEBATTERY) << "Cannot create L2CAP socket:" << std::strerror(errno);
        retryLater(); // e.g. the adapter is being reset, or we ran out of descriptors
        return;
    }

    m_reported = false;
    m_watchdog.start(ResponseTimeout);

    sockaddr_l2 target{};
    target.l2_family = AF_BLUETOOTH;
    target.l2_psm = htobs(Aap::Psm);
    target.l2_bdaddr = *bdaddr;

    if (::connect(m_socket, reinterpret_cast<sockaddr *>(&target), sizeof(target)) < 0 && errno != EINPROGRESS) {
        qCDebug(DEVICEBATTERY) << "AAP connect to" << m_address << "failed:" << std::strerror(errno);
        closeSocket();
        retryLater();
        return;
    }

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
                qCDebug(DEVICEBATTERY) << "Battery report from" << m_address << "with" << parts->size() << "components";
                Q_EMIT batteryChanged(*parts);
            }
            if (m_reported) {
                m_watchdog.start(IdleTimeout); // any traffic proves the session is alive
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
    if (m_failedAttempts == FailuresUntilLost) {
        qCDebug(DEVICEBATTERY) << "AAP for" << m_address << "failed" << m_failedAttempts << "times in a row, dropping its levels";
        Q_EMIT batteryLost();
    }
    const auto delay = std::min<std::chrono::milliseconds>(RetryStep * m_failedAttempts, MaxRetryDelay);
    qCDebug(DEVICEBATTERY) << "Retrying AAP for" << m_address << "in" << delay.count() / 1000 << "s";
    m_retryTimer.start(delay);
}

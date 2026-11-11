// SPDX-License-Identifier: MIT
//
// Runs AapSession against a local socket pair instead of an accessory: the test plays the
// AirPods on the other end. SOCK_SEQPACKET keeps packet boundaries, as L2CAP does.
#include "aapsession.h"

#include "aapprotocol.h"

#include <QSignalSpy>
#include <QTest>

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>

using namespace std::chrono_literals;

namespace
{
// Right earbud 100 %, left earbud 90 % and charging, case 50 %.
const QByteArray BatteryPacket = QByteArray::fromHex(
    "040004000400"
    "03"
    "0201640201"
    "04015a0101"
    "0801320201");
// Primary earbud in the ear, secondary in the case.
const QByteArray EarDetectionPacket = QByteArray::fromHex(
    "040004000600"
    "0002");
// Noise control (setting 0x0D): transparency.
const QByteArray NoiseControlPacket = QByteArray::fromHex(
    "040004000900"
    "0d03000000");
// Some other packet AirPods send, which the session must ignore.
const QByteArray OtherPacket = QByteArray::fromHex(
    "040004001d00"
    "0102");
}

class AapSessionTest : public QObject
{
    Q_OBJECT

private:
    // Delays short enough for a test, but far enough apart to tell them from one another.
    AapSession::Options options()
    {
        AapSession::Options options;
        options.settleDelay = 1ms;
        options.retryStep = 5ms;
        options.maxRetryDelay = 20ms;
        options.responseTimeout = 300ms;
        options.idleTimeout = 600ms;
        options.connector = [this](const QString &) {
            ++m_attempts;
            if (m_refuse) {
                errno = ECONNREFUSED;
                return -1;
            }
            int sockets[2];
            if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets) < 0) {
                return -1;
            }
            closeAccessory(); // the end of the previous attempt, if the test left it open
            m_accessory = sockets[1];
            return sockets[0];
        };
        return options;
    }

    void closeAccessory()
    {
        if (m_accessory >= 0) {
            ::close(m_accessory);
            m_accessory = -1;
        }
    }

    // The next packet the session sent, waiting for it; an empty one if none came.
    QByteArray received()
    {
        QByteArray packet;
        (void)QTest::qWaitFor([&] {
            char buffer[256];
            const ssize_t size = m_accessory >= 0 ? ::recv(m_accessory, buffer, sizeof(buffer), 0) : -1;
            if (size > 0) {
                packet = QByteArray(buffer, size);
            }
            return size > 0;
        });
        return packet;
    }

    void sendFromAccessory(const QByteArray &packet)
    {
        QCOMPARE(::send(m_accessory, packet.constData(), packet.size(), MSG_NOSIGNAL), packet.size());
    }

    // Whether the session connected and said hello.
    bool saidHello()
    {
        return received() == Aap::handshakePacket() && received() == Aap::requestNotificationsPacket();
    }

    int m_accessory = -1;
    int m_attempts = 0;
    bool m_refuse = false;

private Q_SLOTS:
    void init()
    {
        m_attempts = 0;
        m_refuse = false;
    }

    void cleanup()
    {
        closeAccessory();
    }

    void reportsWhatTheAccessorySends()
    {
        AapSession session(QStringLiteral("test"), options());
        QSignalSpy battery(&session, &AapSession::batteryChanged);
        QSignalSpy placements(&session, &AapSession::placementsChanged);
        QSignalSpy noiseControl(&session, &AapSession::noiseControlChanged);
        QVERIFY(saidHello());

        sendFromAccessory(OtherPacket);
        sendFromAccessory(BatteryPacket);
        sendFromAccessory(EarDetectionPacket);
        sendFromAccessory(NoiseControlPacket);
        QTRY_COMPARE(noiseControl.size(), 1);

        QCOMPARE(battery.size(), 1);
        const auto parts = battery.first().first().value<QList<BatteryPart>>();
        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts.at(0), (BatteryPart{BatteryPart::Component::Right, 100, false}));
        QCOMPARE(parts.at(1), (BatteryPart{BatteryPart::Component::Left, 90, true}));
        QCOMPARE(parts.at(2), (BatteryPart{BatteryPart::Component::Case, 50, false}));

        QCOMPARE(placements.size(), 1);
        QCOMPARE(placements.first().at(0).value<Placement>(), Placement::InEar);
        QCOMPARE(placements.first().at(1).value<Placement>(), Placement::InCase);
        QCOMPARE(noiseControl.first().first().value<NoiseControl>(), NoiseControl::Transparency);
        QCOMPARE(m_attempts, 1);
    }

    void reconnectsWhenTheAccessoryHangsUp()
    {
        AapSession session(QStringLiteral("test"), options());
        QSignalSpy battery(&session, &AapSession::batteryChanged);
        QVERIFY(saidHello());
        sendFromAccessory(BatteryPacket);
        QTRY_COMPARE(battery.size(), 1);

        closeAccessory();
        QTRY_COMPARE(m_attempts, 2);
        QVERIFY(saidHello());
        sendFromAccessory(BatteryPacket);
        QTRY_COMPARE(battery.size(), 2);
    }

    // The accessory accepts the channel but never answers, e.g. another app holds its session.
    void reconnectsWhenNoBatteryReportArrives()
    {
        AapSession session(QStringLiteral("test"), options());
        QSignalSpy battery(&session, &AapSession::batteryChanged);
        QVERIFY(saidHello());
        sendFromAccessory(OtherPacket); // traffic, but not a battery report

        QTRY_COMPARE(m_attempts, 2);
        QCOMPARE(battery.size(), 0);
    }

    // A session that worked and then went silent is replaced, without losing the levels: one
    // silent session is not five failures in a row.
    void reconnectsASilentSession()
    {
        AapSession session(QStringLiteral("test"), options());
        QSignalSpy battery(&session, &AapSession::batteryChanged);
        QSignalSpy lost(&session, &AapSession::batteryLost);
        QVERIFY(saidHello());
        sendFromAccessory(BatteryPacket);
        QTRY_COMPARE(battery.size(), 1);

        QTRY_COMPARE(m_attempts, 2); // after idleTimeout, with the accessory end still open
        QCOMPARE(lost.size(), 0);
    }

    void withdrawsTheLevelsAfterRepeatedFailures()
    {
        AapSession session(QStringLiteral("test"), options());
        QSignalSpy battery(&session, &AapSession::batteryChanged);
        QSignalSpy lost(&session, &AapSession::batteryLost);
        QVERIFY(saidHello());
        sendFromAccessory(BatteryPacket);
        QTRY_COMPARE(battery.size(), 1);

        int attemptsWhenLost = 0;
        connect(&session, &AapSession::batteryLost, this, [&] {
            attemptsWhenLost = m_attempts;
        });
        m_refuse = true;
        closeAccessory();
        QTRY_COMPARE(lost.size(), 1);
        QCOMPARE(attemptsWhenLost, 1 + 4); // the hang-up was the first failure of five

        // It keeps trying, without announcing the loss again, and recovers.
        QTRY_VERIFY(m_attempts >= 1 + 6);
        QCOMPARE(lost.size(), 1);
        m_refuse = false;
        QVERIFY(saidHello());
        sendFromAccessory(BatteryPacket);
        QTRY_COMPARE(battery.size(), 2);
        QCOMPARE(lost.size(), 1);
    }

    void stopsWhenDestroyed()
    {
        {
            AapSession session(QStringLiteral("test"), options());
            QVERIFY(saidHello());
        }
        // The session closed its end: reading ours reports the end of the stream.
        char byte;
        QTRY_COMPARE(::recv(m_accessory, &byte, 1, 0), 0);
        const int attempts = m_attempts;
        QTest::qWait(50);
        QCOMPARE(m_attempts, attempts);
    }
};

QTEST_GUILESS_MAIN(AapSessionTest)
#include "aapsessiontest.moc"

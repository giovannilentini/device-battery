// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QObject>
#include <QTimer>

#include <chrono>
#include <functional>

class QSocketNotifier;

// One Apple Accessory Protocol connection to a connected AirPods/Beats device.
//
// Opens a non-blocking L2CAP socket, performs the handshake, subscribes to
// notifications and emits batteryChanged() whenever the accessory pushes new levels.
// The socket is watched by the Qt event loop, so an idle session costs nothing.
// Failed, silent or dropped connections are retried with a growing delay, for as long as
// the session exists; after a few failures in a row the last levels are withdrawn.
class AapSession : public QObject
{
    Q_OBJECT

public:
    // The delays of a session and how it opens its socket. The defaults are the ones the widget
    // uses; tests shorten the delays and connect to a local socket instead of an accessory.
    struct Options {
        // Right after the Bluetooth link comes up, AirPods accept the L2CAP channel but ignore
        // the handshake, so wait for them to settle before the first attempt.
        std::chrono::milliseconds settleDelay = std::chrono::seconds(2);

        // Retry delays grow by retryStep (3 s, 6 s, 9 s, ...) up to maxRetryDelay, and retries
        // never stop: a session only exists while BlueZ says the device is connected, so it is
        // worth waiting for, and an idle retry costs one connect attempt every couple of
        // minutes. After failuresUntilLost failures in a row the last levels are withdrawn as
        // stale.
        std::chrono::milliseconds retryStep = std::chrono::seconds(3);
        std::chrono::milliseconds maxRetryDelay = std::chrono::minutes(2);
        int failuresUntilLost = 5;

        // The watchdog reconnects a session that is not delivering data:
        // - an attempt with no battery report after responseTimeout failed (the connect hung,
        //   or the accessory accepted the channel but ignores us, e.g. while another app holds
        //   the session);
        // - a working session with no packet at all for idleTimeout went stale. AirPods in use
        //   send something every minute or two, so this only triggers when the session really
        //   died.
        std::chrono::milliseconds responseTimeout = std::chrono::seconds(8);
        std::chrono::milliseconds idleTimeout = std::chrono::minutes(5);

        // Returns a non-blocking socket that is connecting, or already connected, to the
        // accessory with this Bluetooth address; -1 with errno set if that failed. The session
        // owns and closes the socket. Empty: an L2CAP socket to the AAP channel.
        std::function<int(const QString &address)> connector;
    };

    explicit AapSession(const QString &address, QObject *parent = nullptr);
    AapSession(const QString &address, const Options &options, QObject *parent = nullptr);
    ~AapSession() override;

Q_SIGNALS:
    void batteryChanged(const QList<BatteryPart> &parts);
    // The accessory stopped answering, so the last reported levels can no longer be trusted.
    void batteryLost();
    // In-ear detection: where the primary and the secondary earbud are.
    void placementsChanged(Placement primary, Placement secondary);
    void noiseControlChanged(NoiseControl mode);

private:
    void connectToAccessory();
    void onConnectFinished();
    void onReadable();
    void send(const QByteArray &packet);
    void closeSocket();
    void retryLater();

    QString m_address;
    Options m_options;
    int m_socket = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer m_retryTimer;
    QTimer m_watchdog; // reconnects a session that delivers no data, see aapsession.cpp
    bool m_reported = false; // a battery report arrived on the current connection
    int m_failedAttempts = 0;
};

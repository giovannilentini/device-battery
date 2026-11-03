// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QObject>
#include <QTimer>

class QSocketNotifier;

// One Apple Accessory Protocol connection to a connected AirPods/Beats device.
//
// Opens a non-blocking L2CAP socket, performs the handshake, subscribes to
// notifications and emits batteryChanged() whenever the accessory pushes new levels.
// The socket is watched by the Qt event loop, so an idle session costs nothing.
// Failed, silent or dropped connections are retried a few times with a growing delay.
class AapSession : public QObject
{
    Q_OBJECT

public:
    explicit AapSession(const QString &address, QObject *parent = nullptr);
    ~AapSession() override;

Q_SIGNALS:
    void batteryChanged(const QList<BatteryPart> &parts);

private:
    void connectToAccessory();
    void onConnectFinished();
    void onReadable();
    void send(const QByteArray &packet);
    void closeSocket();
    void retryLater();

    QString m_address;
    int m_socket = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer m_retryTimer;
    QTimer m_responseTimer;
    int m_failedAttempts = 0;
};

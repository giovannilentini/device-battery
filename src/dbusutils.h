// SPDX-License-Identifier: MIT
#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "logging.h"

namespace DBus
{
inline const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

// Sends `message` without blocking and calls `onReply` with the reply message.
// Errors are logged and swallowed: a missing service simply means "no devices".
// The callback is dropped if `context` is destroyed first.
template<typename Callback>
void callAsync(const QDBusConnection &bus, const QDBusMessage &message, QObject *context, Callback onReply)
{
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, onReply = std::move(onReply)] {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCDebug(DEVICEBATTERY) << "D-Bus call failed:" << reply.errorName() << reply.errorMessage();
            return;
        }
        onReply(reply);
    });
}

// Typed access to the n-th argument of a reply or signal, e.g. argument<QVariantMap>(msg, 1).
template<typename T>
T argument(const QDBusMessage &message, int index)
{
    return qdbus_cast<T>(message.arguments().value(index));
}
}

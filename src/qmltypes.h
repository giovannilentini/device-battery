// SPDX-License-Identifier: MIT
#pragma once

#include "devicefilter.h"

#include <QQmlEngine>

// Makes the core library's DeviceFilter, which does not depend on QML, creatable from QML.
struct DeviceFilterForeign {
    Q_GADGET
    QML_FOREIGN(DeviceFilter)
    QML_NAMED_ELEMENT(DeviceFilter)
};

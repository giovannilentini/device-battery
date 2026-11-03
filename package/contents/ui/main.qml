import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import io.github.giovannilentini.devicebattery

PlasmoidItem {
    id: root

    // [{name, iconName, percentage, charging}, ...] from the shared C++ backend.
    readonly property var devices: DeviceModel.summary

    // The device closest to running out; the panel icon follows it.
    readonly property var lowestDevice: devices.reduce((lowest, device) => {
        if (device.percentage < 0) {
            return lowest;
        }
        return lowest === null || device.percentage < lowest.percentage ? device : lowest;
    }, null)

    readonly property bool batteryLow: lowestDevice !== null
        && !lowestDevice.charging
        && lowestDevice.percentage <= Plasmoid.configuration.lowThreshold

    // In the system tray: hidden with no devices, promoted when a battery is low.
    Plasmoid.status: {
        if (devices.length === 0) {
            return PlasmaCore.Types.PassiveStatus;
        }
        return batteryLow ? PlasmaCore.Types.NeedsAttentionStatus : PlasmaCore.Types.ActiveStatus;
    }

    toolTipMainText: Plasmoid.title
    toolTipSubText: {
        if (devices.length === 0) {
            return i18nc("@info:tooltip", "No devices with battery information");
        }
        return devices.map(device => {
            const level = device.percentage >= 0
                ? i18nc("battery level in percent", "%1%", device.percentage)
                : i18nc("@info:tooltip battery level", "unknown");
            return device.charging
                ? i18nc("@info:tooltip device name: battery level", "%1: %2, charging", device.name, level)
                : i18nc("@info:tooltip device name: battery level", "%1: %2", device.name, level);
        }).join("\n");
    }

    compactRepresentation: CompactRepresentation {
        plasmoidItem: root
        device: root.lowestDevice
    }

    fullRepresentation: FullRepresentation {}
}

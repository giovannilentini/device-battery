import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import io.github.giovannilentini.devicebattery

import "levels.js" as Levels

PlasmoidItem {
    id: root

    // The shared backend runs only while at least one instance of the widget exists.
    Component.onCompleted: DeviceModel.acquire()
    Component.onDestruction: DeviceModel.release()

    // [{name, iconName, percentage, coarseLevel, charging, parts, model, noiseControl, connected,
    // lastSeen}, ...] from the shared C++ backend.
    readonly property var devices: DeviceModel.summary
    // Without the recently disconnected ones, which the popup lists with their last levels.
    readonly property var connectedDevices: devices.filter(device => device.connected)

    // The device closest to running out; the panel icon follows it. Chosen in C++ (unit
    // tested): a charging device only counts when every device is charging.
    readonly property var lowestDevice: devices[DeviceModel.lowestIndex] ?? null

    function levelText(percentage, coarseLevel) {
        switch (coarseLevel) {
        case "critical":
            return i18nc("@info:tooltip rough battery level", "critical");
        case "low":
            return i18nc("@info:tooltip rough battery level", "low");
        case "normal":
            return i18nc("@info:tooltip rough battery level", "normal");
        case "high":
            return i18nc("@info:tooltip rough battery level", "high");
        case "full":
            return i18nc("@info:tooltip rough battery level", "full");
        }
        return percentage >= 0
            ? i18nc("battery level in percent", "%1%", percentage)
            : i18nc("@info:tooltip battery level", "unknown");
    }

    function partText(part) {
        let name = "";
        switch (part.component) {
        case "left":
            name = i18nc("@info:tooltip left earbud", "Left");
            break;
        case "right":
            name = i18nc("@info:tooltip right earbud", "Right");
            break;
        case "case":
            name = i18nc("@info:tooltip charging case", "Case");
            break;
        }
        let text = part.charging
            ? i18nc("@info:tooltip earbud or case: battery level", "%1 %2, charging", name, levelText(part.percentage, ""))
            : i18nc("@info:tooltip earbud or case: battery level", "%1 %2", name, levelText(part.percentage, ""));
        switch (part.placement) {
        case "ear":
            return i18nc("@info:tooltip earbud: battery level, where it is", "%1, in ear", text);
        case "out":
            return i18nc("@info:tooltip earbud: battery level, where it is", "%1, out of ear", text);
        }
        return text; // "case": the earbud charges, which the text already says
    }

    readonly property bool batteryLow: lowestDevice !== null
        && Levels.isLow(lowestDevice.percentage, lowestDevice.charging, lowestDevice.coarseLevel, Plasmoid.configuration.lowThreshold)

    // In the system tray: hidden with no devices, promoted when a battery is low.
    Plasmoid.status: {
        if (connectedDevices.length === 0) {
            return PlasmaCore.Types.PassiveStatus;
        }
        return batteryLow ? PlasmaCore.Types.NeedsAttentionStatus : PlasmaCore.Types.ActiveStatus;
    }

    toolTipMainText: Plasmoid.title
    toolTipSubText: {
        if (connectedDevices.length === 0) {
            return i18nc("@info:tooltip", "No devices with battery information");
        }
        return connectedDevices.map(device => {
            // Earbuds list each part ("AirPods: Left 40%, Right 80%, Case 10%, charging"),
            // other devices their single level.
            if (device.parts.length > 0) {
                return i18nc("@info:tooltip device name: levels of its parts", "%1: %2",
                             device.name, device.parts.map(part => partText(part)).join(", "));
            }
            const level = levelText(device.percentage, device.coarseLevel);
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

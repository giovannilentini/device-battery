pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import io.github.giovannilentini.devicebattery

// Per-device settings: show or hide each device, and give it another name.
// Devices are identified by their key (see deviceKey() in device.h), which stays the same
// when they reconnect.
KCM.ScrollViewKCM {
    id: page

    property var cfg_hiddenDevices: []
    property string cfg_deviceNames: "{}"

    // Set by Plasma for the "Defaults" button; declared to avoid warnings.
    property var cfg_hiddenDevicesDefault
    property string cfg_deviceNamesDefault

    readonly property var names: {
        try {
            const parsed = JSON.parse(cfg_deviceNames);
            return parsed !== null && typeof parsed === "object" ? parsed : {};
        } catch (error) {
            return {};
        }
    }

    // Every device the backend knows (connected or recently disconnected), plus the ones that
    // only appear in the settings, so a hidden device that is gone can still be shown again.
    // Rebuilt only when the set of devices changes, not on every level update or keystroke,
    // which would recreate the rows and take the focus away from the name being typed.
    property var entries: []

    function rebuildEntries() {
        const list = DeviceModel.summary.map(device => ({
            key: device.key,
            name: device.name,
            iconName: device.iconName,
        }));
        const known = new Set(list.map(entry => entry.key));
        for (const key of cfg_hiddenDevices.concat(Object.keys(names))) {
            if (known.has(key)) {
                continue;
            }
            known.add(key);
            list.push({
                key: key,
                // "name:Mouse" -> "Mouse", "bt:38:C4:..." -> its address
                name: key.startsWith("name:") ? key.slice(5) : key.slice(3),
                iconName: "preferences-system-bluetooth-battery-symbolic",
            });
        }
        if (JSON.stringify(list) !== JSON.stringify(entries)) {
            entries = list;
        }
    }

    Component.onCompleted: rebuildEntries()

    Connections {
        target: DeviceModel
        function onSummaryChanged() {
            page.rebuildEntries();
        }
    }

    function setHidden(key, hidden) {
        const keys = cfg_hiddenDevices.filter(entry => entry !== key);
        if (hidden) {
            keys.push(key);
        }
        cfg_hiddenDevices = keys;
    }

    function setName(key, name) {
        const updated = Object.assign({}, names);
        if (name.trim().length > 0) {
            updated[key] = name; // the widget trims it; trimming here would eat typed spaces
        } else {
            delete updated[key];
        }
        cfg_deviceNames = JSON.stringify(updated);
    }

    view: ListView {
        model: page.entries
        reuseItems: false

        delegate: QQC2.ItemDelegate {
            id: row

            required property var modelData
            readonly property bool shown: !page.cfg_hiddenDevices.includes(modelData.key)

            width: ListView.view.width
            down: false
            highlighted: false
            hoverEnabled: false

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                QQC2.CheckBox {
                    checked: row.shown
                    onToggled: page.setHidden(row.modelData.key, !checked)
                    QQC2.ToolTip.text: i18nc("@info:tooltip", "Show this device")
                    QQC2.ToolTip.visible: hovered
                    Accessible.name: i18nc("@option:check device name", "Show %1", row.modelData.name)
                }

                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    source: row.modelData.iconName
                    opacity: row.shown ? 1 : 0.5
                }

                QQC2.TextField {
                    Layout.fillWidth: true
                    text: page.names[row.modelData.key] ?? ""
                    placeholderText: row.modelData.name
                    enabled: row.shown
                    onTextEdited: page.setName(row.modelData.key, text)
                    Accessible.name: i18nc("@label:textbox device name", "Name for %1", row.modelData.name)
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: page.entries.length === 0
            icon.name: "preferences-system-bluetooth-battery-symbolic"
            text: i18nc("@info", "No devices yet")
            explanation: i18nc("@info", "Devices appear here once they have been connected.")
        }
    }

    header: QQC2.Label {
        text: i18nc("@info", "Untick a device to hide it. Type a name to show it instead of the device's own.")
        wrapMode: Text.Wrap
        padding: Kirigami.Units.largeSpacing
    }
}

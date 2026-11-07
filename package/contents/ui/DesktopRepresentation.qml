pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import io.github.giovannilentini.devicebattery

// On the desktop: a grid of large rings, one per device, in the style of Apple's Batteries
// widget. Earbuds list each earbud and the case under their ring.
Item {
    id: desktop

    required property DeviceFilter model

    readonly property real tileWidth: Kirigami.Units.gridUnit * 6
    readonly property int columns: Math.max(1, Math.floor((width - Kirigami.Units.largeSpacing * 2) / tileWidth))

    Layout.minimumWidth: tileWidth + Kirigami.Units.largeSpacing * 2
    Layout.minimumHeight: Kirigami.Units.gridUnit * 8
    Layout.preferredWidth: tileWidth * Math.max(2, Math.min(4, model.count)) + Kirigami.Units.largeSpacing * 2
    Layout.preferredHeight: grid.implicitHeight + Kirigami.Units.largeSpacing * 2

    function partShortName(component) {
        switch (component) {
        case "left":
            return i18nc("@label short for left earbud", "L");
        case "right":
            return i18nc("@label short for right earbud", "R");
        case "case":
            return i18nc("@label short for charging case", "C");
        }
        return "";
    }

    GridLayout {
        id: grid
        anchors.centerIn: parent
        columns: desktop.columns
        rowSpacing: Kirigami.Units.largeSpacing
        columnSpacing: 0

        Repeater {
            model: desktop.model

            delegate: ColumnLayout {
                id: tile

                required property string name
                required property string iconName
                required property int percentage
                required property string coarseLevel
                required property bool charging
                required property var parts
                required property bool connected

                Layout.preferredWidth: desktop.tileWidth
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing
                opacity: connected ? 1 : 0.6

                BatteryRing {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 4
                    percentage: tile.percentage
                    coarseLevel: tile.coarseLevel
                    charging: tile.charging

                    Kirigami.Icon {
                        anchors.fill: parent
                        source: tile.iconName
                    }
                }

                PercentLabel {
                    Layout.alignment: Qt.AlignHCenter
                    percentage: tile.percentage
                    coarseLevel: tile.coarseLevel
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.3
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: tile.name || i18nc("@label", "Unknown device")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                // "L 64% · R 82% · C 40%"
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    visible: tile.parts.length > 0
                    horizontalAlignment: Text.AlignHCenter
                    text: tile.parts.map(part => i18nc("@label earbud or case: battery level", "%1 %2",
                        desktop.partShortName(part.component),
                        part.percentage >= 0 ? i18nc("battery level in percent", "%1%", part.percentage) : "–")).join(" · ")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                }
            }
        }
    }

    PlasmaExtras.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.largeSpacing * 4
        visible: desktop.model.count === 0
        iconName: "preferences-system-bluetooth-battery-symbolic"
        text: i18nc("@info", "No devices with battery information")
    }
}

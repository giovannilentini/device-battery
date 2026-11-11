pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import io.github.giovannilentini.devicebattery

// On the desktop: a grid of rings, one per device, in the style of Apple's Batteries widget.
// Earbuds show a small ring per earbud and the case under their ring.
//
// It adapts to the size the widget is given: the number of columns is the one that gives the
// largest rings, and the rings grow and shrink with the widget. When the widget is too small
// for readable rings, it shows the same list as the popup instead.
Item {
    id: desktop

    required property DeviceFilter model

    readonly property real margin: Kirigami.Units.largeSpacing
    readonly property real availableWidth: width - margin * 2
    readonly property real availableHeight: height - margin * 2
    readonly property int count: model.count

    // Height of the text under a ring (level and name), and of the earbud rings if any device
    // has them; constant, only the rings scale.
    readonly property real labelHeight: Kirigami.Units.gridUnit * 2.5
    readonly property bool anyParts: {
        for (const device of model.summary) {
            if (device.parts.length > 0) {
                return true;
            }
        }
        return false;
    }
    readonly property real partsHeight: anyParts ? Kirigami.Units.iconSizes.medium + Kirigami.Units.gridUnit : 0

    // Narrowest a tile can be: wide enough for the earbud rings, or for a short name.
    readonly property real minimumCellWidth: anyParts ? Kirigami.Units.iconSizes.medium * 3 + Kirigami.Units.smallSpacing * 4 : Kirigami.Units.gridUnit * 4

    // Smallest and largest ring diameter; below the smallest, the list is shown.
    readonly property real minimumRing: Kirigami.Units.iconSizes.large
    readonly property real maximumRing: Kirigami.Units.gridUnit * 7

    // {columns, ring}: the column count giving the largest rings for the available space.
    readonly property var fit: {
        let best = {
            columns: 1,
            ring: 0,
        };
        for (let columns = 1; columns <= Math.max(1, count); ++columns) {
            const rows = Math.ceil(Math.max(1, count) / columns);
            const cellWidth = availableWidth / columns;
            // Usually only one device has earbuds, so their rings take room in one row only.
            const cellHeight = (availableHeight - partsHeight) / rows;
            if (cellWidth < minimumCellWidth) {
                break; // more columns only get narrower
            }
            const ring = Math.min(cellWidth - Kirigami.Units.largeSpacing, cellHeight - labelHeight - Kirigami.Units.largeSpacing, maximumRing);
            // On a tie (rings at their largest size), more columns use a wide widget better.
            if (ring >= best.ring) {
                best = {
                    columns: columns,
                    ring: ring,
                };
            }
        }
        return best;
    }
    readonly property bool useList: count > 0 && fit.ring < minimumRing

    Layout.minimumWidth: Kirigami.Units.gridUnit * 8
    Layout.minimumHeight: Kirigami.Units.gridUnit * 6
    Layout.preferredWidth: Kirigami.Units.gridUnit * 6 * Math.max(2, Math.min(4, count)) + margin * 2
    Layout.preferredHeight: Kirigami.Units.gridUnit * 12

    // Nothing may draw outside the widget, also while it is being resized.
    clip: true

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
        width: Math.min(implicitWidth, desktop.availableWidth)
        visible: !desktop.useList && desktop.count > 0
        columns: desktop.fit.columns
        rowSpacing: Kirigami.Units.largeSpacing
        columnSpacing: 0

        Repeater {
            model: desktop.useList ? null : desktop.model

            delegate: ColumnLayout {
                id: tile

                required property string name
                required property string iconName
                required property int percentage
                required property string coarseLevel
                required property bool charging
                required property var parts
                required property bool connected

                Layout.preferredWidth: desktop.availableWidth / desktop.fit.columns
                Layout.maximumWidth: desktop.availableWidth / desktop.fit.columns
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing

                // For screen readers: the tile as one item, "Mouse", "54%".
                Accessible.role: Accessible.ListItem
                Accessible.name: tile.name || i18nc("@label", "Unknown device")
                Accessible.description: tile.charging
                    ? i18nc("@info:tooltip battery level", "%1, charging", tileLevel.accessibleText)
                    : tileLevel.accessibleText

                BatteryRing {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: desktop.fit.ring
                    Layout.preferredHeight: desktop.fit.ring
                    percentage: tile.percentage
                    coarseLevel: tile.coarseLevel
                    charging: tile.charging
                    stale: !tile.connected

                    Kirigami.Icon {
                        anchors.fill: parent
                        source: tile.iconName
                    }
                }

                PercentLabel {
                    id: tileLevel
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    percentage: tile.percentage
                    coarseLevel: tile.coarseLevel
                    font.bold: tile.connected
                    opacity: tile.connected ? 1 : 0.7
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: tile.name || i18nc("@label", "Unknown device")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                // Earbuds: a small ring per earbud and the case, with its letter inside.
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: false // nested layouts fill by default; keep the rings together
                    visible: tile.parts.length > 0
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: tile.parts

                        delegate: ColumnLayout {
                            id: part

                            required property var modelData

                            spacing: 0

                            BatteryRing {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                                percentage: part.modelData.percentage
                                charging: part.modelData.charging
                                stale: !tile.connected

                                PlasmaComponents3.Label {
                                    anchors.centerIn: parent
                                    text: desktop.partShortName(part.modelData.component)
                                    font: Kirigami.Theme.smallFont
                                }
                            }

                            PercentLabel {
                                Layout.alignment: Qt.AlignHCenter
                                percentage: part.modelData.percentage
                                font: Kirigami.Theme.smallFont
                                opacity: 0.7
                            }
                        }
                    }
                }
            }
        }
    }

    // Too small for rings: the list from the popup, scrolling if needed.
    Loader {
        anchors.fill: parent
        active: desktop.useList
        sourceComponent: PlasmaComponents3.ScrollView {
            ListView {
                model: desktop.model
                topMargin: Kirigami.Units.smallSpacing
                bottomMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing
                interactive: contentHeight > height

                delegate: DeviceDelegate {
                    width: ListView.view.width
                }
            }
        }
    }

    PlasmaExtras.PlaceholderMessage {
        anchors.centerIn: parent
        width: desktop.availableWidth
        visible: desktop.count === 0
        iconName: "preferences-system-bluetooth-battery-symbolic"
        text: i18nc("@info", "No devices with battery information")
    }
}

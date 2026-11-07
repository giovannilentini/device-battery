pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// One device in the popup:
//
//   ( icon )  AirPods                          97%
//             Noise cancellation
//             (L) 97%   (R) 100%   (C) 40%
//
// At most one short status line ("Charging", the noise control mode, or "3 h ago" for a
// disconnected device). The model is in the row's tooltip.
Item {
    id: root

    // Model roles
    required property string name
    required property string iconName
    required property int percentage
    required property string coarseLevel
    required property bool charging
    required property var parts
    required property string model
    required property string noiseControl
    required property bool connected
    required property var lastSeen // Date, for a disconnected device

    // The current time, for "3 h ago"; set by the list so all rows share one timer.
    property date now: new Date()

    readonly property string status: {
        if (!connected) {
            return lastSeenText();
        }
        if (charging && parts.length === 0) {
            return i18nc("@info:status", "Charging");
        }
        return noiseControlText(noiseControl);
    }

    implicitHeight: layout.implicitHeight

    function lastSeenText() {
        const minutes = Math.max(0, Math.floor((now - lastSeen) / 60000));
        if (minutes < 1) {
            return i18nc("@info:status disconnected device", "Just now");
        }
        if (minutes < 60) {
            return i18ncp("@info:status disconnected device", "%1 min ago", "%1 min ago", minutes);
        }
        const hours = Math.floor(minutes / 60);
        if (hours < 24) {
            return i18ncp("@info:status disconnected device", "%1 h ago", "%1 h ago", hours);
        }
        const days = Math.floor(hours / 24);
        return i18ncp("@info:status disconnected device", "Yesterday", "%1 days ago", days);
    }

    function noiseControlText(mode) {
        switch (mode) {
        case "off":
            return i18nc("@info:status noise control mode", "Noise control off");
        case "cancellation":
            return i18nc("@info:status noise control mode", "Noise cancellation");
        case "transparency":
            return i18nc("@info:status noise control mode", "Transparency");
        case "adaptive":
            return i18nc("@info:status noise control mode", "Adaptive");
        }
        return "";
    }

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

    HoverHandler {
        id: hover
    }

    PlasmaComponents3.ToolTip.text: root.model
    PlasmaComponents3.ToolTip.visible: hover.hovered && root.model.length > 0
    PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        BatteryRing {
            Layout.preferredWidth: Kirigami.Units.iconSizes.large
            Layout.preferredHeight: Kirigami.Units.iconSizes.large
            Layout.alignment: Qt.AlignTop
            percentage: root.percentage
            coarseLevel: root.coarseLevel
            charging: root.charging
            stale: !root.connected

            Kirigami.Icon {
                anchors.fill: parent
                source: root.iconName
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: root.name || i18nc("@label", "Unknown device")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                PercentLabel {
                    percentage: root.percentage
                    coarseLevel: root.coarseLevel
                    font.bold: root.connected
                    opacity: root.connected ? 1 : 0.7
                }
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.status
                textFormat: Text.PlainText
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
            }

            // Only earbuds have parts: other devices never build the slots below.
            Loader {
                Layout.fillWidth: true
                active: root.parts.length > 0
                visible: active

                // Three equal columns, so the row never wraps and the parts line up between devices.
                sourceComponent: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // One fixed slot per component instead of a Repeater over `parts`: `parts` is a
                    // new list on every update, which would recreate the rings each time and skip
                    // their animation. The slots stay alive and only their values change.
                    Repeater {
                        model: ["left", "right", "case"]

                        delegate: RowLayout {
                            id: part

                            required property string modelData
                            readonly property var info: root.parts.find(entry => entry.component === part.modelData) ?? null

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1 // equal shares
                            spacing: Kirigami.Units.smallSpacing
                            opacity: info !== null ? 1 : 0 // keep the column, so the others stay aligned

                            BatteryRing {
                                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                                percentage: part.info?.percentage ?? -1
                                charging: part.info?.charging ?? false
                                stale: !root.connected

                                PlasmaComponents3.Label {
                                    anchors.centerIn: parent
                                    text: root.partShortName(part.modelData)
                                    font: Kirigami.Theme.smallFont
                                }
                            }

                            PercentLabel {
                                Layout.fillWidth: true
                                percentage: part.info?.percentage ?? -1
                                font: Kirigami.Theme.smallFont
                                opacity: 0.8
                            }
                        }
                    }
                }
            }
        }
    }
}

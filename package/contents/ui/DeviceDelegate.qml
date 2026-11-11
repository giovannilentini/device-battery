pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// One device in the popup:
//
//   ( icon )  AirPods                          97%
//             Noise cancellation (or the model)
//             (L) 97%   (R) 100%   (C) 40%
//
// At most one short status line ("Charging", the noise control mode, or "3 h ago" for a
// disconnected device).
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
        // The model, e.g. "AirPods Pro", unless there is a noise control mode to show.
        return noiseControlText(noiseControl) || model;
    }

    implicitHeight: layout.implicitHeight

    // For screen readers: the row as one item, "AirPods", "97%, Noise cancellation, Left 97%,
    // Right 100%, Case 40%, charging".
    Accessible.role: Accessible.ListItem
    Accessible.name: name || i18nc("@label", "Unknown device")
    Accessible.description: {
        const lines = [levelLabel.accessibleText];
        if (status.length > 0) {
            lines.push(status);
        }
        for (const part of parts) {
            lines.push(partText(part));
        }
        return lines.join(", ");
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
        const level = part.percentage >= 0
            ? i18nc("battery level in percent", "%1%", part.percentage)
            : i18nc("@info:tooltip battery level", "unknown");
        return part.charging && connected
            ? i18nc("@info:tooltip earbud or case: battery level", "%1 %2, charging", name, level)
            : i18nc("@info:tooltip earbud or case: battery level", "%1 %2", name, level);
    }

    // Too narrow for the status line and the earbud rings (e.g. a small desktop widget): show
    // just the name and level.
    readonly property bool narrow: width < Kirigami.Units.gridUnit * 14

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
                    id: levelLabel
                    percentage: root.percentage
                    coarseLevel: root.coarseLevel
                    font.bold: root.connected
                    opacity: root.connected ? 1 : 0.7
                }
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                visible: text.length > 0 && !root.narrow
                text: root.status
                textFormat: Text.PlainText
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
            }

            // Only earbuds have parts: other devices never build the slots below.
            Loader {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 14 // a wide list keeps the rings together
                active: root.parts.length > 0 && !root.narrow
                visible: active

                // Three equal columns, so the row never wraps and the parts line up between devices.
                // The parts reported are packed into the left-most columns, in their order (left,
                // right, case: `parts` is sorted), so a missing part leaves no hole before or
                // between them: with only the right earbud and the case, they take the first two.
                sourceComponent: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Three fixed slots instead of a Repeater over `parts`: `parts` is a new list on
                    // every update, which would recreate the rings each time and skip their
                    // animation. The slots stay alive and only their values change.
                    Repeater {
                        model: 3

                        delegate: RowLayout {
                            id: part

                            required property int index
                            readonly property var info: root.parts[index] ?? null
                            readonly property string component: info?.component ?? ""

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
                                    text: root.partShortName(part.component)
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

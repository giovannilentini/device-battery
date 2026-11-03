pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// One device in the popup:
//
//   ( icon )  AirPods                          97%
//             (o) Left 97%  (o) Right 100%  (o) Case 40%
Item {
    id: root

    // Model roles
    required property string name
    required property string iconName
    required property int percentage
    required property string coarseLevel
    required property bool charging
    required property var parts

    implicitHeight: layout.implicitHeight

    function partName(component) {
        switch (component) {
        case "left":
            return i18nc("@label left earbud", "Left");
        case "right":
            return i18nc("@label right earbud", "Right");
        case "case":
            return i18nc("@label charging case", "Case");
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
                    font.bold: true
                }
            }

            PlasmaComponents3.Label {
                visible: root.charging && root.parts.length === 0
                text: i18nc("@info:status", "Charging")
                opacity: 0.7
                font: Kirigami.Theme.smallFont
            }

            // Only earbuds have parts: other devices never build the slots below (three rings
            // and six labels per row that would stay hidden).
            Loader {
                Layout.fillWidth: true
                active: root.parts.length > 0
                visible: active

                sourceComponent: Flow {
                    spacing: Kirigami.Units.largeSpacing

                    // One fixed slot per component instead of a Repeater over `parts`: `parts` is a
                    // new list on every update, which would recreate the rings each time and skip
                    // their animation. The slots stay alive and only their values change.
                    Repeater {
                        model: ["left", "right", "case"]

                        delegate: Row {
                            id: part

                            required property string modelData
                            readonly property var info: root.parts.find(entry => entry.component === part.modelData) ?? null

                            visible: info !== null
                            spacing: Kirigami.Units.smallSpacing

                            BatteryRing {
                                width: Kirigami.Units.iconSizes.smallMedium
                                height: width
                                anchors.verticalCenter: parent.verticalCenter
                                percentage: part.info?.percentage ?? -1
                                charging: part.info?.charging ?? false
                            }

                            PlasmaComponents3.Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.partName(part.modelData)
                                textFormat: Text.PlainText
                                font: Kirigami.Theme.smallFont
                            }

                            PercentLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                percentage: part.info?.percentage ?? -1
                                font: Kirigami.Theme.smallFont
                                opacity: 0.7
                            }
                        }
                    }
                }
            }
        }
    }
}

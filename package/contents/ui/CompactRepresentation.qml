import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// The panel / system tray icon: the emptiest device inside its battery ring,
// optionally followed by its percentage.
MouseArea {
    id: compact

    required property PlasmoidItem plasmoidItem
    required property var device // entry of DeviceModel.summary, or null

    readonly property bool inSquareSlot: (Plasmoid.containmentDisplayHints & PlasmaCore.Types.ContainmentForcesSquarePlasmoids)
        || Plasmoid.formFactor !== PlasmaCore.Types.Horizontal
    readonly property bool showLabel: device !== null && !inSquareSlot && Plasmoid.configuration.showPercentage

    property bool wasExpanded: false

    Layout.minimumWidth: showLabel ? row.implicitWidth : -1

    hoverEnabled: true
    onPressed: wasExpanded = plasmoidItem.expanded
    onClicked: plasmoidItem.expanded = !wasExpanded

    // Keyboard navigation in the panel and screen readers.
    activeFocusOnTab: true
    Accessible.name: Plasmoid.title
    Accessible.description: plasmoidItem.toolTipSubText
    Accessible.role: Accessible.Button
    Keys.onPressed: event => {
        switch (event.key) {
        case Qt.Key_Space:
        case Qt.Key_Enter:
        case Qt.Key_Return:
        case Qt.Key_Select:
            plasmoidItem.expanded = !plasmoidItem.expanded;
            event.accepted = true;
            break;
        }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        BatteryRing {
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Layout.alignment: Qt.AlignCenter
            percentage: compact.device ? compact.device.percentage : -1
            coarseLevel: compact.device ? compact.device.coarseLevel : ""
            charging: compact.device ? compact.device.charging : false

            Kirigami.Icon {
                anchors.fill: parent
                source: compact.device ? compact.device.iconName : Plasmoid.icon
                active: compact.containsMouse
            }
        }

        PercentLabel {
            visible: compact.showLabel
            percentage: compact.device ? compact.device.percentage : -1
            coarseLevel: compact.device ? compact.device.coarseLevel : ""
        }
    }
}

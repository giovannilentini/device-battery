import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import io.github.giovannilentini.devicebattery

// The popup: one row per device, with separate rings for each earbud and the case.
PlasmaExtras.Representation {
    id: full

    // Height of the rows plus a margin above and below: the popup hugs its content.
    readonly property real listHeight: deviceList.contentHeight + Kirigami.Units.largeSpacing * 2
    readonly property real placeholderHeight: Kirigami.Units.gridUnit * 8
    // Past this, the list scrolls instead of the popup growing further.
    readonly property real maximumListHeight: Kirigami.Units.gridUnit * 24
    readonly property bool empty: deviceList.count === 0
    readonly property real fittedHeight: empty ? placeholderHeight : Math.min(listHeight, maximumListHeight)

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
    // Plasma saves the popup size when it closes and from then on ignores preferredHeight,
    // so a popup sized for one device stayed that size with two. It keeps applying the
    // minimum and maximum, though, also to a saved size: pinning both to the content
    // makes the popup fit exactly the connected devices, and follow them as they change.
    Layout.minimumHeight: fittedHeight
    Layout.maximumHeight: fittedHeight
    Layout.preferredHeight: fittedHeight

    collapseMarginsHint: true

    contentItem: PlasmaComponents3.ScrollView {
        ListView {
            id: deviceList

            model: DeviceModel
            topMargin: Kirigami.Units.largeSpacing
            bottomMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing
            interactive: contentHeight > height

            delegate: DeviceDelegate {
                width: ListView.view.width
            }

            PlasmaExtras.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: deviceList.count === 0
                iconName: "preferences-system-bluetooth-battery-symbolic"
                text: i18nc("@info", "No devices with battery information")
                explanation: i18nc("@info", "Connect AirPods, headphones, a mouse or a keyboard to see their battery level here.")
            }
        }
    }
}

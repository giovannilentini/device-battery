import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import io.github.giovannilentini.devicebattery

// The popup: one row per device, with separate rings for each earbud and the case.
PlasmaExtras.Representation {
    id: full

    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
    Layout.minimumHeight: Kirigami.Units.gridUnit * 8
    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
    Layout.preferredHeight: Math.max(Layout.minimumHeight, deviceList.contentHeight + Kirigami.Units.largeSpacing * 2)

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
                iconName: "preferences-system-bluetooth-battery"
                text: i18nc("@info", "No devices with battery information")
                explanation: i18nc("@info", "Connect AirPods, headphones, a mouse or a keyboard to see their battery level here.")
            }
        }
    }
}

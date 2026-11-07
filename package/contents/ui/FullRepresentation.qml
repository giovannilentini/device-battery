import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import io.github.giovannilentini.devicebattery

// The popup: one row per device, with separate rings for each earbud and the case. On the
// desktop it shows the grid of rings instead (DesktopRepresentation).
//
// One representation for both, switching its content: Plasma creates the full representation
// once, possibly before it has told the widget where it is placed, and does not create it again
// when the placement becomes known. A widget in a panel then got the desktop grid.
PlasmaExtras.Representation {
    id: full

    required property DeviceFilter model
    property bool onDesktop: false

    readonly property Item desktopView: desktopLoader.item as Item

    // Height of the rows plus a margin above and below: the popup hugs its content.
    readonly property real listHeight: deviceList.contentHeight + Kirigami.Units.largeSpacing * 2
    // The empty-state message decides its own height: a fixed one cut off its text, more so
    // with large fonts or long translations.
    readonly property real placeholderHeight: placeholder.implicitHeight + Kirigami.Units.gridUnit * 2
    // Past this, the list scrolls instead of the popup growing further.
    readonly property real maximumListHeight: Kirigami.Units.gridUnit * 24
    readonly property bool empty: deviceList.count === 0
    readonly property real fittedHeight: empty ? placeholderHeight : Math.min(listHeight, maximumListHeight)

    // On the desktop: the grid's own hints, and the widget can be resized freely.
    Layout.minimumWidth: desktopView ? desktopView.Layout.minimumWidth : Kirigami.Units.gridUnit * 16
    Layout.preferredWidth: desktopView ? desktopView.Layout.preferredWidth : Kirigami.Units.gridUnit * 20
    // Plasma saves the popup size when it closes and from then on ignores preferredHeight,
    // so a popup sized for one device stayed that size with two. It keeps applying the
    // minimum and maximum, though, also to a saved size: pinning both to the content
    // makes the popup fit exactly the connected devices, and follow them as they change.
    Layout.minimumHeight: desktopView ? desktopView.Layout.minimumHeight : fittedHeight
    Layout.maximumHeight: desktopView ? Infinity : fittedHeight
    Layout.preferredHeight: desktopView ? desktopView.Layout.preferredHeight : fittedHeight

    collapseMarginsHint: !onDesktop

    // Connected devices that report no battery (see DeviceModel.silentDevices), except hidden ones.
    readonly property var silentDevices: DeviceModel.silentDevices.filter(device => !model.hiddenKeys.includes(device.key))
    readonly property string silentText: {
        if (silentDevices.length === 0) {
            return "";
        }
        const lines = [silentDevices.length === 1
            ? i18nc("@info %1 is a device name", "%1 is connected but doesn't report its battery to Linux.", silentDevices[0].name)
            : i18nc("@info %1 is a list of device names", "Connected without battery information: %1.", silentDevices.map(device => device.name).join(", "))];
        if (silentDevices.some(device => device.usb)) {
            lines.push(i18nc("@info", "Some keyboards and mice report their battery over Bluetooth, but not through their USB receiver."));
        }
        if (silentDevices.some(device => device.audio)) {
            lines.push(i18nc("@info", "Some headsets report it once BlueZ's experimental features are turned on."));
        }
        return lines.join(" ");
    }

    // For "Disconnected 2 h ago": refreshed once a minute, only while the popup is open.
    property date now: new Date()
    onVisibleChanged: {
        now = new Date();
        if (visible) {
            DeviceModel.refreshSilentDevices(); // USB receivers are looked for only now
        }
    }

    Timer {
        interval: 60000
        repeat: true
        running: full.visible
        onTriggered: full.now = new Date()
    }

    contentItem: Item {
        PlasmaComponents3.ScrollView {
            anchors.fill: parent
            visible: !full.onDesktop

            ListView {
                id: deviceList

                model: full.model
                topMargin: Kirigami.Units.largeSpacing
                bottomMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing
                interactive: contentHeight > height

                delegate: DeviceDelegate {
                    width: ListView.view.width
                    now: full.now
                }

                // Disconnected devices come last, under their own heading.
                section.property: "connected"
                section.delegate: Loader {
                    required property string section
                    width: ListView.view.width
                    active: section === "false" // no heading above the connected devices
                    sourceComponent: PlasmaExtras.ListSectionHeader {
                        leftPadding: Kirigami.Units.largeSpacing // line up with the rows
                        text: i18nc("@title:group", "Disconnected")
                    }
                }

                // Below the devices: the connected devices nothing reports a battery for.
                footer: RowLayout {
                    width: ListView.view.width
                    visible: full.silentText.length > 0 && !full.empty
                    height: visible ? implicitHeight + Kirigami.Units.largeSpacing : 0
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.topMargin: Kirigami.Units.largeSpacing
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        source: "dialog-information-symbolic"
                        opacity: 0.7
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        Layout.topMargin: Kirigami.Units.largeSpacing
                        text: full.silentText
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }

                PlasmaExtras.PlaceholderMessage {
                    id: placeholder
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: deviceList.count === 0
                    iconName: "preferences-system-bluetooth-battery-symbolic"
                    text: i18nc("@info", "No devices with battery information")
                    explanation: full.silentText || i18nc("@info", "Connect AirPods, headphones, a mouse or a keyboard to see their battery level here.")
                }
            }
        }

        Loader {
            id: desktopLoader
            anchors.fill: parent
            active: full.onDesktop
            sourceComponent: DesktopRepresentation {
                model: full.model
            }
        }
    }
}

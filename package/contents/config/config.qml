import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18nc("@title", "General")
        icon: "preferences-system-bluetooth-battery-symbolic"
        source: "configGeneral.qml"
    }

    ConfigCategory {
        name: i18nc("@title", "Devices")
        icon: "input-mouse"
        source: "configDevices.qml"
    }
}

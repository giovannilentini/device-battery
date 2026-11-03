import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18nc("@title", "General")
        icon: "preferences-system-bluetooth-battery"
        source: "configGeneral.qml"
    }
}

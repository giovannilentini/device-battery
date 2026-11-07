import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    property alias cfg_showPercentage: showPercentage.checked
    property alias cfg_lowThreshold: lowThreshold.value
    property alias cfg_showDisconnected: showDisconnected.checked

    // Set by Plasma for the "Defaults" button; declared to avoid warnings.
    property bool cfg_showPercentageDefault
    property int cfg_lowThresholdDefault
    property bool cfg_showDisconnectedDefault

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: showPercentage
            Kirigami.FormData.label: i18nc("@label", "Panel:")
            text: i18nc("@option:check", "Show percentage next to the icon")
        }

        QQC2.SpinBox {
            id: lowThreshold
            Kirigami.FormData.label: i18nc("@label:spinbox", "Low battery at:")
            from: 5
            to: 50
            stepSize: 5
            textFromValue: (value, locale) => i18nc("battery level in percent", "%1%", value)
            valueFromText: (text, locale) => parseInt(text)
        }

        QQC2.CheckBox {
            id: showDisconnected
            Kirigami.FormData.label: i18nc("@label", "Popup:")
            text: i18nc("@option:check", "List disconnected devices with their last levels")
        }
    }
}

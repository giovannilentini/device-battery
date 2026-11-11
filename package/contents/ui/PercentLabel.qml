import QtQuick
import org.kde.plasma.components as PlasmaComponents3

// "97%", a rough level ("Low") for devices that report no percentage, or a dash while the
// level is unknown.
PlasmaComponents3.Label {
    property int percentage: -1
    property string coarseLevel: ""

    text: {
        switch (coarseLevel) {
        case "critical":
            return i18nc("@info:status rough battery level", "Critical");
        case "low":
            return i18nc("@info:status rough battery level", "Low");
        case "normal":
            return i18nc("@info:status rough battery level", "Normal");
        case "high":
            return i18nc("@info:status rough battery level", "High");
        case "full":
            return i18nc("@info:status rough battery level", "Full");
        }
        return percentage >= 0 ? i18nc("battery level in percent", "%1%", percentage) : "–";
    }
    textFormat: Text.PlainText

    // What a screen reader should say: the dash of an unknown level means nothing when spoken.
    readonly property string accessibleText: percentage >= 0 || coarseLevel ? text : i18nc("@info:tooltip battery level", "unknown")
    Accessible.name: accessibleText
}

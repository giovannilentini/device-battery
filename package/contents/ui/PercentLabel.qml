import QtQuick
import org.kde.plasma.components as PlasmaComponents3

// "97%", or a dash while the level is unknown.
PlasmaComponents3.Label {
    property int percentage: -1

    text: percentage >= 0 ? i18nc("battery level in percent", "%1%", percentage) : "–"
    textFormat: Text.PlainText
}

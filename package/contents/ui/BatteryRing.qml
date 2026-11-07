import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

import "levels.js" as Levels

// A circular battery gauge, in the style of Apple's Batteries widget.
// Anything declared inside it (an icon, a label) is centred in the ring.
Item {
    id: ring

    property int percentage: -1 // 0-100, or -1 when unknown
    property string coarseLevel: "" // see PercentLabel
    property bool charging: false
    // A last known level (disconnected device): drawn in a neutral colour, never red or green.
    property bool stale: false
    property real thickness: Math.max(2, Math.round(width / 12))
    default property alias content: centre.data

    readonly property bool low: Levels.isLow(percentage, charging, coarseLevel, Plasmoid.configuration.lowThreshold)
    readonly property real radius: (Math.min(width, height) - thickness) / 2

    implicitWidth: Kirigami.Units.iconSizes.large
    implicitHeight: implicitWidth

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // Background track
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
            strokeWidth: ring.thickness

            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: 0
                sweepAngle: 360
            }
        }

        // Level, clockwise from twelve o'clock
        ShapePath {
            fillColor: "transparent"
            strokeColor: {
                if (ring.percentage <= 0) {
                    return "transparent";
                }
                if (ring.stale) {
                    return Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.45);
                }
                return ring.low ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor;
            }
            strokeWidth: ring.thickness
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: 360 * Math.max(0, ring.percentage) / 100

                Behavior on sweepAngle {
                    NumberAnimation {
                        duration: Kirigami.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }

    Item {
        id: centre
        anchors.centerIn: parent
        width: ring.width - 4 * ring.thickness
        height: width
    }

    // Charging badge: a lightning bolt on a disc, drawn as vectors so it looks the same
    // in every icon theme. Paths use a 0-1 unit square scaled to the badge size.
    Shape {
        id: bolt
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: Math.max(8, Math.round(ring.width * 0.4))
        height: width
        visible: ring.charging
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            scale: Qt.size(bolt.width, bolt.height)
            fillColor: Kirigami.Theme.backgroundColor
            strokeColor: "transparent"

            PathAngleArc {
                centerX: 0.5
                centerY: 0.5
                radiusX: 0.5
                radiusY: 0.5
                sweepAngle: 360
            }
        }

        ShapePath {
            scale: Qt.size(bolt.width, bolt.height)
            fillColor: Kirigami.Theme.positiveTextColor
            strokeColor: "transparent"

            PathPolyline {
                path: [
                    Qt.point(0.58, 0.10), Qt.point(0.24, 0.56), Qt.point(0.47, 0.56),
                    Qt.point(0.40, 0.90), Qt.point(0.76, 0.44), Qt.point(0.53, 0.44),
                    Qt.point(0.58, 0.10),
                ]
            }
        }
    }
}

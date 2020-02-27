import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.11

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property point profile: controller.profile
    property double zoom
    property double scalex
    property double scaley
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    property bool showAudiothumb: false
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property color overlayColor: 'cyan'
    property bool isClipMonitor: false

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    signal editCurrentMarker()
    signal toolBarChanged(bool doAccept)

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }

    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    function updatePalette() {
        clipMonitorRuler.forceRepaint()
    }

    function switchOverlay() {
        if (controller.overlayType >= 5) {
            controller.overlayType = 0
        } else {
            controller.overlayType = controller.overlayType + 1;
        }
        root.overlayType = controller.overlayType
    }
    MouseArea {
        id: barOverArea
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
    }
    SceneToolBar {
        id: sceneToolBar
        barContainsMouse: sceneToolBar.rightSide ? barOverArea.mouseX >= x - 10 : barOverArea.mouseX < x + width + 10
        onBarContainsMouseChanged: {
            sceneToolBar.opacity = 1
            sceneToolBar.visible = sceneToolBar.barContainsMouse
        }
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
    }

    Item {
        height: root.height - controller.rulerHeight
        width: root.width
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            anchors.centerIn: parent

            Loader {
                anchors.fill: parent
                source: {
                    switch(root.overlayType)
                    {
                        case 0:
                            return '';
                        case 1:
                            return "OverlayStandard.qml";
                        case 2:
                            return "OverlayMinimal.qml";
                        case 3:
                            return "OverlayCenter.qml";
                        case 4:
                            return "OverlayCenterDiagonal.qml";
                        case 5:
                            return "OverlayThirds.qml";
                    }
                }
            }
        }
        Item {
            id: monitorOverlay
            anchors.fill: parent

            Text {
                id: timecode
                font: fixedFont
                objectName: "timecode"
                color: "white"
                style: Text.Outline; 
                styleColor: "black"
                text: controller.toTimecode(controller.position)
                visible: root.showTimecode
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    rightMargin: 4
                }
            }
            Text {
                id: fpsdropped
                font: fixedFont
                objectName: "fpsdropped"
                color: root.dropped ? "red" : "white"
                style: Text.Outline;
                styleColor: "black"
                text: i18n("%1 fps", root.fps)
                visible: root.showFps
                anchors {
                    right: timecode.visible ? timecode.left : parent.right
                    bottom: parent.bottom
                    rightMargin: 10
                }
            }
            Label {
                id: inPoint
                font: fixedFont
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && controller.position == controller.zoneIn
                text: i18n("In Point")
                color: "white"
                background: Rectangle {
                    color: "#228b22"
                }
                height: marker.height
                width: textMetricsIn.width + 10
                leftPadding:0
                rightPadding:0
                horizontalAlignment: TextInput.AlignHCenter
                TextMetrics {
                    id: textMetricsIn
                    font: inPoint.font
                    text: inPoint.text
                }
            }
            Label {
                id: outPoint
                font: fixedFont
                anchors {
                    left: inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && controller.position + 1 == controller.zoneOut
                text: i18n("Out Point")
                color: "white"
                background: Rectangle {
                    color: "#ff4500"
                }
                width: textMetricsOut.width + 10
                height: marker.height
                leftPadding:0
                rightPadding:0
                horizontalAlignment: TextInput.AlignHCenter
                TextMetrics {
                    id: textMetricsOut
                    font: outPoint.font
                    text: outPoint.text
                }
            }
            TextField {
                id: marker
                font: fixedFont
                objectName: "markertext"
                activeFocusOnPress: true
                onEditingFinished: {
                    root.markerText = marker.displayText
                    marker.focus = false
                    root.editCurrentMarker()
                }
                anchors {
                    left: outPoint.visible ? outPoint.right : inPoint.visible ? inPoint.right : parent.left
                    bottom: parent.bottom
                }
                visible: root.showMarkers && text != ""
                text: controller.markerComment
                width: textMetrics.width + 10
                horizontalAlignment: TextInput.AlignHCenter
                background: Rectangle {
                        color: "#990000ff"
                }
                color: "white"
                padding:0

                TextMetrics {
                    id: textMetrics
                    font: marker.font
                    text: controller.markerComment
                }
                maximumLength: 25
            }
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: controller.rulerHeight
    }
}

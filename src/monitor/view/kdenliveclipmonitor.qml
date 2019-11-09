import QtQuick.Controls 2.2
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.6
import com.enums 1.0

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property int itemType: 0
    property point profile
    property double zoom
    property double scalex
    property double scaley
    property bool dropped
    property string fps
    property bool showMarkers
    property bool showTimecode
    property bool showFps
    property bool showSafezone
    property bool showAudiothumb
    property bool showToolbar: false
    property string clipName: controller.clipName
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property int overlayType: controller.overlayType
    property color overlayColor: 'cyan'
    property bool isClipMonitor: true
    property int dragType: 0

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
    onClipNameChanged: {
        // Animate clip name
        clipNameLabel.opacity = 1
        showAnimate.restart()
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

            Item {
                id: audioThumb
                property bool stateVisible: (clipMonitorRuler.containsMouse || (barOverArea.containsMouse && barOverArea.mouseY >= root.height * 0.7))
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                }
                height: controller.clipType == ProducerType.Audio ? parent.height : parent.height / 6
                //font.pixelSize * 3
                width: parent.width
                visible: root.showAudiothumb

                states: [
                    State { when: audioThumb.stateVisible || controller.clipType == ProducerType.Audio;
                        PropertyChanges {   target: audioThumb; opacity: 1.0    } },
                    State { when: !audioThumb.stateVisible && controller.clipType != ProducerType.Audio;
                        PropertyChanges {   target: audioThumb; opacity: 0.0    } }
                ]
                transitions: [ Transition {
                    NumberAnimation { property: "opacity"; duration: 500}
                } ]
                Rectangle {
                    color: "black"
                    opacity: 0.5
                    anchors.fill: parent
                }
                Rectangle {
                    color: "yellow"
                    opacity: 0.3
                    height: parent.height
                    x: controller.zoneIn * timeScale
                    width: (controller.zoneOut - controller.zoneIn) * timeScale
                    visible: controller.zoneIn > 0 || controller.zoneOut < duration - 1
                }
                Image {
                    anchors.fill: parent
                    source: controller.audioThumb
                    asynchronous: true
                }
                Rectangle {
                    color: "red"
                    width: 1
                    height: parent.height
                    x: controller.position * timeScale
                }
                MouseArea {
                    id: thumbMouseArea
                    anchors.fill: parent
                    onPressed: {
                        if (mouse.buttons === Qt.LeftButton) {
                            var pos = Math.max(mouseX, 0)
                            controller.requestSeekPosition(Math.min(pos / root.timeScale, root.duration));
                        }
                    }
                }
            }
            Label {
                id: clipNameLabel
                font: fixedFont
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                }
                color: "white"
                text: clipName
                background: Rectangle {
                    color: "#222277"
                }
                visible: clipName != ""
                padding :4
                SequentialAnimation {
                    id: showAnimate
                    running: false
                    NumberAnimation { target: clipNameLabel; duration: 3000 }
                    NumberAnimation { target: clipNameLabel; property: "opacity"; to: 0; duration: 1000 }
                }
            }

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
                maximumLength: 20
            }
        }

        Rectangle {
            // Audio or video only drag zone
            x: 5
            y: parent.height - height - 5
            width: childrenRect.width
            height: childrenRect.height
            color: Qt.rgba(activePalette.highlight.r, activePalette.highlight.g, activePalette.highlight.b, 0.7)
            radius: 4
            opacity: (dragAudioArea.containsMouse || dragVideoArea.containsMouse /*|| dragOverArea.pressed */|| (barOverArea.containsMouse && barOverArea.mouseY >= y)) ? 1 : 0
            visible: controller.clipHasAV
            Row {
                id: dragRow
                ToolButton {
                    id: videoDragButton
                    icon.name: "kdenlive-show-video"
                    MouseArea {
                        id: dragVideoArea
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        onPressed: {
                            parent.enabled = false
                            mouse.accepted = false
                            dragType = 2
                        }
                        onExited: {
                            parent.enabled = true
                            parent.clicked()
                        }
                    }
                }
                ToolButton {
                    id: audioDragButton
                    icon.name: "audio-volume-medium"
                    MouseArea {
                        id: dragAudioArea
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        anchors.fill: parent
                        propagateComposedEvents: true
                        cursorShape: Qt.PointingHand
                        onPressed: {
                            parent.enabled = false
                            mouse.accepted = false
                            dragType = 1
                        }
                        onExited: {
                            parent.enabled = true
                        }
                    }
                }
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

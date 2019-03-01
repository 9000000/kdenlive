import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import Kdenlive.Controls 1.0
import QtQuick 2.4

    // Monitor ruler
Rectangle {
    id: ruler
    color: activePalette.window

    Timer {
        id: zoneToolTipTimer
        interval: 3000; running: false;
    }
    function forceRepaint()
    {
        ruler.color = activePalette.window
        // Enforce repaint
        rulerTicks.model = 0
        rulerTicks.model = ruler.width / frameSize + 2
        playhead.fillColor = activePalette.windowText
    }

    function updateRuler()
    {
        root.timeScale = width / root.duration
        if (root.duration < 200) {
            root.frameSize = 5 * root.timeScale
        } else if (duration < 2500) {
            frameSize = 25 * root.timeScale
        } else if (duration < 10000) {
            root.frameSize = 50 * root.timeScale
        } else {
            root.frameSize = 100 * root.timeScale
        }
    }

    // frame ticks
    Repeater {
        id: rulerTicks
        model: ruler.width / frameSize + 2
        Rectangle {
            property int realPos: index
            x: realPos * frameSize
            anchors.bottom: ruler.bottom
            height: (realPos % 4)? ((realPos % 2) ? 3 : 7) : 12
            width: 1
            color: activePalette.windowText
            opacity: 0.5
        }
    }
    MouseArea {
        id: rulerMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onPressed: {
            if (mouse.buttons === Qt.LeftButton) {
                controller.requestSeekPosition(Math.min(mouseX / root.timeScale, root.duration));
            }
        }
        onPositionChanged: {
            if (mouse.buttons === Qt.LeftButton) {
                root.mouseRulerPos = mouseX
                if (pressed) {
                    controller.requestSeekPosition(Math.min(mouseX / root.timeScale, root.duration));
                }
            }
        }
    }
    // Zone duration indicator
    Rectangle {
        visible: zoneToolTipTimer.running || rulerMouseArea.containsMouse || trimInMouseArea.containsMouse || trimInMouseArea.pressed || trimOutMouseArea.containsMouse || trimOutMouseArea.pressed
        width: inLabel.contentWidth + 4
        height: inLabel.contentHeight
        property int centerPos: zone.x + zone.width / 2 - inLabel.contentWidth / 2
        x: centerPos < 0 ? 0 : centerPos > ruler.width - inLabel.contentWidth ? ruler.width - inLabel.contentWidth : centerPos
        color: activePalette.highlight
        anchors.bottom: ruler.top
        Label {
            id: inLabel
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: trimInMouseArea.containsMouse || trimInMouseArea.pressed ? controller.toTimecode(controller.zoneIn) + '>' + controller.toTimecode(controller.zoneOut - controller.zoneIn) : trimOutMouseArea.containsMouse || trimOutMouseArea.pressed ? controller.toTimecode(controller.zoneOut - controller.zoneIn) + '<' + controller.toTimecode(controller.zoneOut) : controller.toTimecode(controller.zoneOut - controller.zoneIn)
            font.pixelSize: root.baseUnit
            color: activePalette.highlightedText
        }
    }
    TimelinePlayhead {
        id: playhead
        visible: controller.position > -1
        height: ruler.height * 0.5
        width: ruler.height * 1
        opacity: 0.8
        anchors.top: ruler.top
        fillColor: activePalette.windowText
        x: controller.position * root.timeScale - (width / 2)
    }
    // monitor zone
    Rectangle {
        x: controller.zoneIn * root.timeScale
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        width: 1
        color: activePalette.highlight
        visible: rulerMouseArea.containsMouse || trimInMouseArea.containsMouse || trimInMouseArea.pressed || trimOutMouseArea.containsMouse
    }
    Rectangle {
        x: controller.zoneOut * root.timeScale
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        width: 1
        color: activePalette.highlight
        visible: rulerMouseArea.containsMouse || trimOutMouseArea.containsMouse || trimOutMouseArea.pressed || trimInMouseArea.containsMouse
    }
    Rectangle {
        id: zone
        visible: controller.zoneOut > controller.zoneIn
        color: activePalette.highlight
        x: controller.zoneIn * root.timeScale
        width: (controller.zoneOut - controller.zoneIn) * root.timeScale
        anchors.bottom: parent.bottom
        height: ruler.height / 2
        opacity: 0.4
        onXChanged: zoneToolTipTimer.start()
        onWidthChanged: zoneToolTipTimer.start()
    }
    Rectangle {
        id: trimIn
        x: zone.x - root.baseUnit / 3
        y: zone.y
        height: zone.height
        width: root.baseUnit * .8
        color: 'lawngreen'
        opacity: trimInMouseArea.containsMouse || trimInMouseArea.drag.active ? 0.5 : 0
        Drag.active: trimInMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        MouseArea {
            id: trimInMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false
            drag.minimumX: 0
            drag.maximumX: ruler.width
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    controller.zoneIn = Math.round(trimIn.x / root.timeScale)
                }
            }
        }
    }
    Rectangle {
        id: trimOut
        width: root.baseUnit * .8
        x: zone.x + zone.width - (width * .7)
        y: zone.y
        height: zone.height
        color: 'darkred'
        opacity: trimOutMouseArea.containsMouse || trimOutMouseArea.drag.active ? 0.5 : 0
        Drag.active: trimOutMouseArea.drag.active
        Drag.proposedAction: Qt.MoveAction
        MouseArea {
            id: trimOutMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.smoothed: false
            drag.minimumX: 0
            drag.maximumX: ruler.width - trimOut.width
            onPositionChanged: {
                if (mouse.buttons === Qt.LeftButton) {
                    controller.zoneOut = Math.round((trimOut.x + trimOut.width) / root.timeScale)
                }
            }
        }
    }

    // markers
    Repeater {
        model: markersModel
        delegate:
        Item {
            anchors.fill: parent
            Rectangle {
                id: markerBase
                width: 1
                height: parent.height
                x: (model.frame) * root.timeScale;
                color: model.color
            }
            Rectangle {
                visible: !rulerMouseArea.pressed && (guideArea.containsMouse || (rulerMouseArea.containsMouse && Math.abs(rulerMouseArea.mouseX - markerBase.x) < 4))
                opacity: 0.7
                property int guidePos: markerBase.x - mlabel.contentWidth / 2
                x: guidePos < 0 ? 0 : (guidePos > (parent.width - mlabel.contentWidth) ? parent.width - mlabel.contentWidth : guidePos)
                radius: 2
                width: mlabel.contentWidth
                height: mlabel.contentHeight * .8
                anchors {
                    bottom: parent.top
                }
                color: model.color
                Text {
                    id: mlabel
                    text: model.comment
                    font.pixelSize: root.baseUnit
                    verticalAlignment: Text.AlignVCenter
                    anchors {
                        fill: parent
                    }
                    color: 'white'
                }
                MouseArea {
                    z: 10
                    id: guideArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    //onDoubleClicked: timeline.editMarker(clipRoot.binId, model.frame)
                    onClicked: {
                        controller.requestSeekPosition(model.frame)
                    }
                }
            }
        }
    }

    Rectangle {
        id: seekCursor
        visible: controller.seekPosition > -1
        color: activePalette.highlight
        width: 4
        height: ruler.height
        opacity: 0.5
        x: controller.seekPosition * root.timeScale
        y: 0
    }
}

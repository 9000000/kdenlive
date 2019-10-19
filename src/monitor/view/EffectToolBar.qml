import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick 2.7


Rectangle {
    id: effecttoolbar
    objectName: "effecttoolbar"
    width: fullscreenButton.width
    property bool rightSide: true
    property bool barContainsMouse
    height: childrenRect.height
    color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.7)
    radius: 4
    border.color : Qt.rgba(0, 0, 0, 0.3)
    border.width: 1
    
    Timer {
        id: fadeTimer
        interval: 2000; running: false;
        onTriggered: {
            effecttoolbar.visible = false
            effecttoolbar.opacity = 1
        }
    }
    OpacityAnimator {
        id: animator
        target: effecttoolbar;
        from: 1;
        to: 0;
        duration: 2000
        running: false
    }

    function fadeBar()
    {
        effecttoolbar.visible = true
        animator.start()
        fadeTimer.start()
    }

    Column {
        ToolButton {
            id: fullscreenButton
            objectName: "fullScreen"
            iconName: "view-fullscreen"
            tooltip: i18n("Switch Full Screen")
            onClicked: controller.triggerAction('monitor_fullscreen')
        }
        ToolButton {
            objectName: "nextKeyframe"
            iconName: "go-next"
            tooltip: "Go to Next Keyframe"
            onClicked: controller.seekNextKeyframe()
        }
        ToolButton {
            objectName: "prevKeyframe"
            iconName: "go-previous"
            tooltip: i18n("Go to Previous Keyframe")
            onClicked: controller.seekPreviousKeyframe()
        }
        ToolButton {
            objectName: "addKeyframe"
            iconName: "list-add"
            tooltip: i18n("Add/Remove Keyframe")
            onClicked: controller.addRemoveKeyframe()
        }
        ToolButton {
            iconName: "zoom-in"
            tooltip: i18n("Zoom in")
            onClicked: controller.triggerAction('monitor_zoomin')
        }
        ToolButton {
            iconName: "zoom-out"
            tooltip: i18n("Zoom out")
            onClicked: controller.triggerAction('monitor_zoomout')
        }
        ToolButton {
            objectName: "moveBar"
            iconName: "transform-move-horizontal"
            tooltip: i18n("Move Toolbar")
            onClicked: {
                if (effecttoolbar.rightSide) {
                    effecttoolbar.anchors.right = undefined
                    effecttoolbar.anchors.left = effecttoolbar.parent.left
                    effecttoolbar.rightSide = false
                    fadeBar()
                } else {
                    effecttoolbar.anchors.left = undefined
                    effecttoolbar.anchors.right = effecttoolbar.parent.right
                    effecttoolbar.rightSide = true
                    fadeBar()
                }
            }
        }
    }
}

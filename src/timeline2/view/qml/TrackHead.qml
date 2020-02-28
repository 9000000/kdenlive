/*
 * Copyright (c) 2013-2016 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.11
import QtQuick.Controls 2.4

Rectangle {
    id: trackHeadRoot
    property string trackName
    property string effectNames
    property bool isStackEnabled
    property bool isDisabled
    property bool collapsed: false
    property int isComposite
    property bool isLocked: false
    property bool isActive: false
    property bool isAudio
    property bool showAudioRecord
    property bool current: false
    property int myTrackHeight
    property int trackId : -42
    property int buttonSize: root.baseUnit * 1.8
    property int iconSize: buttonSize - 4
    property string trackTag
    property int thumbsFormat: 0
    border.width: 1
    border.color: root.frameColor

    function pulseLockButton() {
        flashLock.restart();
    }

    color: getTrackColor(isAudio, true)
    //border.color: selected? 'red' : 'transparent'
    //border.width: selected? 1 : 0
    clip: true
    state: 'normal'
    states: [
        State {
            name: 'current'
            when: trackHeadRoot.current
            PropertyChanges {
                target: trackHeadRoot
                color: selectedTrackColor
            }
        },
        State {
            when: !trackHeadRoot.current
            name: 'normal'
            PropertyChanges {
                target: trackHeadRoot
                color: getTrackColor(isAudio, true)
            }
        }
    ]

    Keys.onDownPressed: {
        root.moveSelectedTrack(1)
    }
    Keys.onUpPressed: {
        root.moveSelectedTrack(-1)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            timeline.activeTrack = trackId
            if (mouse.button == Qt.RightButton) {
                root.showHeaderMenu()
            }
        }
        onClicked: {
            parent.forceActiveFocus()
            nameEdit.visible = false
            if (mouse.button == Qt.LeftButton) {
                timeline.showTrackAsset(trackId)
            }
        }
    }
    Item {
        id: targetColumn
        width: trackHeadRoot.buttonSize * .4
        height: trackHeadRoot.height
        Item {
            anchors.fill: parent
            anchors.margins: 1
            Rectangle {
                id: trackTarget
                color: 'grey'
                anchors.fill: parent
                width: height
                border.width: 0
                visible: trackHeadRoot.isAudio ? timeline.hasAudioTarget : timeline.hasVideoTarget
                MouseArea {
                    id: targetArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (trackHeadRoot.isAudio) {
                            if (trackHeadRoot.trackId == timeline.audioTarget) {
                                timeline.audioTarget = -1;
                            } else if (timeline.hasAudioTarget) {
                                timeline.audioTarget = trackHeadRoot.trackId;
                            }
                        } else {
                            if (trackHeadRoot.trackId == timeline.videoTarget) {
                                timeline.videoTarget = -1;
                            } else if (timeline.hasVideoTarget) {
                                timeline.videoTarget = trackHeadRoot.trackId;
                            }
                        }
                    }
                }
                ToolTip {
                        visible: targetArea.containsMouse
                        font.pixelSize: root.baseUnit
                        delay: 1500
                        timeout: 5000
                        background: Rectangle {
                            color: activePalette.alternateBase
                            border.color: activePalette.light
                        }
                        contentItem: Label {
                            color: activePalette.text
                            text: i18n("Click to toggle track as target. Target tracks will receive the inserted clips")
                        }
                    }
                state:  'normalTarget'
                states: [
                    State {
                        name: 'target'
                        when: (trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.audioTarget) || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.videoTarget)
                        PropertyChanges {
                            target: trackTarget
                            color: timeline.targetColor
                        }
                    },
                    State {
                        name: 'inactiveTarget'
                        when: (trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.lastAudioTarget) || (!trackHeadRoot.isAudio && trackHeadRoot.trackId == timeline.lastVideoTarget)
                        PropertyChanges {
                            target: trackTarget
                            opacity: 0.3
                            color: activePalette.text
                        }
                    },
                    State {
                        name: 'noTarget'
                        when: !trackHeadRoot.isLocked && !trackHeadRoot.isDisabled
                        PropertyChanges {
                            target: trackTarget
                            color: activePalette.base
                        }
                    }
                ]
                transitions: [
                    Transition {
                        to: '*'
                        ColorAnimation { target: trackTarget; duration: 300 }
                    }
                ]
            }
        }
    }
    Item {
        id: trackHeadColumn
        anchors.fill: parent
        anchors.leftMargin: targetColumn.width
        anchors.topMargin: 0

        ToolButton {
            id: expandButton
            anchors.left: trackHeadColumn.left
            height: trackHeadRoot.buttonSize
            width: trackHeadRoot.buttonSize
            focusPolicy: Qt.NoFocus
            //icon.width: trackHeadRoot.iconSize
            //icon.height: trackHeadRoot.iconSize
            icon.name: trackHeadRoot.collapsed ? 'arrow-right' : 'arrow-down'
            onClicked: {
                trackHeadRoot.myTrackHeight = trackHeadRoot.collapsed ? Math.max(root.collapsedHeight * 1.5, controller.getTrackProperty(trackId, "kdenlive:trackheight")) : root.collapsedHeight
            }
            ToolTip {
                visible: expandButton.hovered
                font.pixelSize: root.baseUnit
                delay: 1500
                timeout: 5000
                background: Rectangle {
                    color: activePalette.alternateBase
                    border.color: activePalette.light
                }
                contentItem: Label {
                    color: activePalette.text
                    text: trackLabel.visible? i18n("Minimize") : i18n("Expand")
                }
            }
        }
        Item {
            id: tagContainer
            anchors.left: expandButton.right
            width: trackHeadRoot.buttonSize
            height: trackHeadRoot.buttonSize
            //Layout.topMargin: 1
            Rectangle {
                id: trackLed
                color: Qt.darker(trackHeadRoot.color, 0.55)
                anchors.fill: parent
                anchors.margins: trackHeadRoot.buttonSize / 8
                border.width: 0
                Text {
                    id: trackTagLabel
                    text: trackHeadRoot.trackTag
                    anchors.fill: parent
                    font.pointSize: root.fontUnit
                    color: activePalette.text
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                MouseArea {
                    id: tagMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        timeline.switchTrackActive(trackHeadRoot.trackId)
                    }
                }
                ToolTip {
                    visible: tagMouseArea.containsMouse
                    font.pixelSize: root.baseUnit
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: i18n("Click to make track active/inactive. Active tracks will react to editing operations")
                    }
                    }
            state:  'normalled'
                states: [
                    State {
                        name: 'locked'
                        when: trackHeadRoot.isLocked
                        PropertyChanges {
                            target: trackLed
                            color: 'red'
                        }
                    },
                    State {
                        name: 'active'
                        when: trackHeadRoot.isActive
                        PropertyChanges {
                            target: trackLed
                            color: timeline.targetColor
                        }
                        PropertyChanges {
                            target: trackTagLabel
                            color: timeline.targetTextColor
                        }
                    },
                    State {
                        name: 'inactive'
                        when: !trackHeadRoot.isLocked && !trackHeadRoot.isActive
                        PropertyChanges {
                            target: trackLed
                            color: Qt.darker(trackHeadRoot.color, 0.55)
                        }
                    }
                ]
                transitions: [
                    Transition {
                        to: '*'
                        ColorAnimation { target: trackLed; duration: 300 }
                    }
                ]
            }
        }
        Label {
            anchors.left: tagContainer.right
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            text: trackHeadRoot.trackName
            font.pixelSize: root.baseUnit
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            visible: trackHeadRoot.collapsed && trackHeadRoot.width > trackTarget.width + expandButton.width + trackTagLabel.width + (4 * muteButton.width) + 4
        }
        Row {
            width: childrenRect.width
            x: Math.max(2 * trackHeadRoot.buttonSize + 2, parent.width - width - 4)
            spacing: 0
            ToolButton {
                id: effectButton
                icon.name: 'tools-wizard'
                checkable: true
                enabled: trackHeadRoot.effectNames != ''
                checked: enabled && trackHeadRoot.isStackEnabled
                height: trackHeadRoot.buttonSize
                width: trackHeadRoot.buttonSize
                focusPolicy: Qt.NoFocus
                //icon.width: trackHeadRoot.iconSize
                //icon.height: trackHeadRoot.iconSize
                onClicked: {
                    timeline.showTrackAsset(trackId)
                    controller.setTrackStackEnabled(trackId, !isStackEnabled)
                }
            }
            ToolButton {
                id: muteButton
                height: trackHeadRoot.buttonSize
                width: trackHeadRoot.buttonSize
                focusPolicy: Qt.NoFocus
                //icon.width: trackHeadRoot.iconSize
                //icon.height: trackHeadRoot.iconSize
                icon.name: isAudio ? (isDisabled ? 'kdenlive-hide-audio' : 'kdenlive-show-audio') : (isDisabled ? 'kdenlive-hide-video' : 'kdenlive-show-video')
                onClicked: controller.setTrackProperty(trackId, "hide", isDisabled ? (isAudio ? '1' : '2') : '3')
                ToolTip {
                    visible: muteButton.hovered
                    font.pixelSize: root.baseUnit
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: isAudio ? (isDisabled? i18n("Unmute") : i18n("Mute")) : (isDisabled? i18n("Show") : i18n("Hide"))
                    }
                }
            }

            ToolButton {
                id: lockButton
                height: trackHeadRoot.buttonSize
                width: trackHeadRoot.buttonSize
                focusPolicy: Qt.NoFocus
                //icon.width: trackHeadRoot.iconSize
                //icon.height: trackHeadRoot.iconSize
                icon.name: isLocked ? 'kdenlive-lock' : 'kdenlive-unlock'
                onClicked: controller.setTrackLockedState(trackId, !isLocked)
                ToolTip {
                    visible: lockButton.hovered
                    font.pixelSize: root.baseUnit
                    delay: 1500
                    timeout: 5000
                    background: Rectangle {
                        color: activePalette.alternateBase
                        border.color: activePalette.light
                    }
                    contentItem: Label {
                        color: activePalette.text
                        text: isLocked? i18n("Unlock track") : i18n("Lock track")
                    }
                }

                 SequentialAnimation {
                    id: flashLock
                    loops: 1
                    ScaleAnimator {
                        target: lockButton
                        from: 1
                        to: 2
                        duration: 500
                    }
                    ScaleAnimator {
                        target: lockButton
                        from: 2
                        to: 1
                        duration: 500
                    }
                 }
            }
        }
        Item {
            anchors.bottom: trackHeadColumn.bottom
            anchors.left: trackHeadColumn.left
            anchors.right: trackHeadColumn.right
            anchors.margins: 2
            height: nameEdit.height
            Rectangle {
                id: trackLabel
                color: 'transparent'
                radius: 2
                anchors.fill: parent
                border.color: trackNameMouseArea.containsMouse ? activePalette.highlight : 'transparent'
                visible: (trackHeadRoot.height >= trackLabel.height + muteButton.height + resizer.height)
                MouseArea {
                    id: trackNameMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onDoubleClicked: {
                        nameEdit.visible = true
                        nameEdit.focus = true
                        nameEdit.selectAll()
                    }
                    onClicked: {
                        timeline.showTrackAsset(trackId)
                        timeline.activeTrack = trackId
                        trackHeadRoot.focus = true
                    }
                    onEntered: {
                        if (nameEdit.visible == false && trackName == '') {
                            placeHolder.visible = true
                        }
                    }
                    onExited: {
                        if (placeHolder.visible == true) {
                            placeHolder.visible = false
                        }
                    }
                }
                Label {
                    text: trackName
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font.pointSize: root.fontUnit
                }
                Label {
                    id: placeHolder
                    visible: false
                    enabled: false
                    text: i18n("Edit track name")
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    elide: Qt.ElideRight
                    font.pointSize: root.fontUnit
                }
                TextField {
                    id: nameEdit
                    visible: false
                    width: parent.width
                    text: trackName
                    font.pointSize: root.fontUnit
                    background: Rectangle {
                        radius: 2
                        color: activePalette.window
                        anchors.fill: parent
                    }
                    /*style: TextFieldStyle {
                        padding.top:0
                        padding.bottom: 0
                        background: Rectangle {
                            radius: 2
                            color: activePalette.window
                            anchors.fill: parent
                        }
                    }*/
                    onEditingFinished: {
                        controller.setTrackProperty(trackId, "kdenlive:track_name", text)
                        visible = false
                    }
                }
            }
        }
    }
    Rectangle {
            id: resizer
            height: 4
            color: 'red'
            opacity: 0
            Drag.active: trimInMouseArea.drag.active
            Drag.proposedAction: Qt.MoveAction
            width: trackHeadRoot.width
            y: trackHeadRoot.height - height

            MouseArea {
                id: trimInMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                drag.target: parent
                drag.axis: Drag.YAxis
                drag.minimumY: root.collapsedHeight - resizer.height
                property double startY
                property double originalY
                drag.smoothed: false

                onPressed: {
                    root.autoScrolling = false
                    startY = mapToItem(null, x, y).y
                    originalY = trackHeadRoot.height // reusing originalX to accumulate delta for bubble help
                }
                onReleased: {
                    root.autoScrolling = timeline.autoScroll
                    if (!trimInMouseArea.containsMouse) {
                        parent.opacity = 0
                    }
                    if (mouse.modifiers & Qt.ShiftModifier) {
                        timeline.adjustAllTrackHeight(trackHeadRoot.trackId, trackHeadRoot.myTrackHeight)
                    }
                }
                onEntered: parent.opacity = 0.3
                onExited: parent.opacity = 0
                onPositionChanged: {
                    if (mouse.buttons === Qt.LeftButton) {
                        parent.opacity = 0.5
                        var newHeight = originalY + (mapToItem(null, x, y).y - startY)
                        newHeight =  Math.max(root.collapsedHeight, newHeight)
                        trackHeadRoot.myTrackHeight = newHeight
                    }
                }
            }
        }
    DropArea { //Drop area for tracks
        anchors.fill: trackHeadRoot
        keys: 'kdenlive/effect'
        property string dropData
        property string dropSource
        property int dropRow: -1
        onEntered: {
            dropData = drag.getDataAsString('kdenlive/effect')
            dropSource = drag.getDataAsString('kdenlive/effectsource')
        }
        onDropped: {
            console.log("Add effect: ", dropData)
            if (dropSource == '') {
                // drop from effects list
                controller.addTrackEffect(trackHeadRoot.trackId, dropData);
            } else {
                controller.copyTrackEffect(trackHeadRoot.trackId, dropSource);
            }
            dropSource = ''
            dropRow = -1
            drag.acceptProposedAction
        }
    }
}


/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15


ToolButton {
    property string iconName
    property string toolTipText
    contentItem: Item {
        Image {
            visible: iconName
            source: 'image://icon/' + iconName
            anchors.centerIn: parent
            width: parent.width - 4
            height: width
        }
    }
    width: parent.width
    height: width
    focusPolicy: Qt.NoFocus
    onPressed: {
        root.captureRightClick = true
    }
    onReleased: {
        root.captureRightClick = false
    }
    ToolTip {
        visible: parent.hovered
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: toolTipText
        timeout: 3000
    }
}

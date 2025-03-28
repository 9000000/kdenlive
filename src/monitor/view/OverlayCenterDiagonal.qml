/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2018 Willian Pessoa
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick 2.15

Item {
    id: overlay
    property color color
    property double diagonalLength: Math.sqrt(Math.pow(parent.height, 2) + Math.pow(parent.width, 2))

    function degreesRotation(width, height) {
        var a = height/width;
        var b = Math.sqrt(1 + Math.pow(a, 2));
        var angle = Math.acos(Math.pow(a,2) / (a * b));
        return angle * (180 / Math.PI);
    }

    Rectangle {
        color: overlay.color
        width: overlay.diagonalLength
        height: 1
        rotation: degreesRotation(parent.height, parent.width)
        anchors.centerIn: parent
        antialiasing: true
    }

    Rectangle {
        color: overlay.color
        height: overlay.diagonalLength
        width: 1
        rotation: degreesRotation(parent.width, parent.height)
        anchors.centerIn: parent
        antialiasing: true
    }
}

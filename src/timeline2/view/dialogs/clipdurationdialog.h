/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "widgets/timecodedisplay.h"
#include "ui_clipdurationdialog_ui.h"

/** @class ClipDurationDialog
    @brief A dialog for modifying an item's (clip or transition) duration.
    @author Jean-Baptiste Mardelle
 */
class ClipDurationDialog : public QDialog, public Ui::ClipDurationDialog_UI
{
    Q_OBJECT

public:
    explicit ClipDurationDialog(int clipId, int pos, int minpos, int in, int out, int length, int maxpos, QWidget *parent = nullptr);
    GenTime startPos() const;
    GenTime cropStart() const;
    GenTime duration() const;

private Q_SLOTS:
    void slotCheckDuration();
    void slotCheckStart();
    void slotCheckCrop();
    void slotCheckEnd();

private:
    int m_clipId;
    double m_fps;
    GenTime m_min;
    GenTime m_max;
    GenTime m_crop;
    GenTime m_length;
};

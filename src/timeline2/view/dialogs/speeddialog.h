/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QDialog>

namespace Ui {
class ClipSpeed_UI;
}

class TimecodeDisplay;
class KMessageWidget;

class SpeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeedDialog(QWidget *parent, double speed, int duration, double minSpeed, double maxSpeed, bool reversed, bool pitch_compensate,
                         ClipType::ProducerType clipType);
    ~SpeedDialog() override;

    double getValue() const;
    bool getPitchCompensate() const;

private:
    Ui::ClipSpeed_UI *ui;
    TimecodeDisplay *m_durationDisplay;
    int m_duration;
    void checkSpeed(double res);
};

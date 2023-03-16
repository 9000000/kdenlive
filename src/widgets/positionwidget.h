/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "utils/timecode.h"
#include <QString>
#include <QWidget>

class QSlider;
class TimecodeDisplay;

/** @brief This class is used to display a parameter with time value */
class PositionWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
     * @param name Name of the parameter
     * @param pos Initial position (in time)
     * @param min Minimum time value
     * @param max maximum time value
     * @param tc Timecode object to define time format
     * @param comment (optional) A comment explaining the parameter. Will be shown in a tooltip.
     * @param parent (optional) Parent Widget */
    explicit PositionWidget(const QString &name, int pos, int min, int max, const QString &comment = QString(), QWidget *parent = nullptr);
    ~PositionWidget() override;
    /** @brief get current position
     */
    int getPosition() const;
    /** @brief set position
     */
    void setPosition(int pos);
    /** @brief checks that the allowed time interval is valid
     */
    bool isValid() const;

public Q_SLOTS:
    /** @brief change the range of the widget
        @param min New minimum value
        @param max New maximum value
        @param absolute If true, the range is shifted to start in 0
     */
    void setRange(int min, int max, bool absolute = false);

    void slotShowComment(bool show);

private:
    TimecodeDisplay *m_display;
    QSlider *m_slider;

private Q_SLOTS:
    void slotUpdatePosition();

Q_SIGNALS:
    void valueChanged();
};

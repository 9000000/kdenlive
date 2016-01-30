/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "animkeyframeruler.h"

#include "kdenlivesettings.h"
#include "definitions.h"

#include "mlt++/MltAnimation.h"

#include <QDebug>
#include <QFontDatabase>
#include <KColorScheme>

#include <QMouseEvent>
#include <QStylePainter>
#include <QApplication>

const int margin = 5;

#define SEEK_INACTIVE (-1)

AnimKeyframeRuler::AnimKeyframeRuler(int min, int max, QWidget *parent) :
    QWidget(parent)
  , frameLength(max - min)
  , m_position(0)
  , m_scale(0)
  , m_movingKeyframe(false)
  , m_movingKeyframePos(0)
  , m_hoverKeyframe(-1)
  , m_seekPosition(SEEK_INACTIVE)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setMouseTracking(true);
    QPalette p = palette();
    m_size = QFontInfo(font()).pixelSize() * 1.8;
    m_lineHeight = m_size / 2;
    setMinimumHeight(m_size);
    setMaximumHeight(m_size);
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_selected = scheme.decoration(KColorScheme::HoverColor).color();
    m_keyframe = scheme.foreground(KColorScheme::LinkText).color();
}

void AnimKeyframeRuler::updateKeyframes(QVector<int> keyframes, QVector<int> types)
{
    m_keyframes = keyframes;
    m_keyframeTypes = types;
    update();
}

// virtual
void AnimKeyframeRuler::mousePressEvent(QMouseEvent * event)
{
    m_hoverKeyframe = -1;
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    int xPos = event->x() - margin;
    int headOffset = m_lineHeight / 1.5;
    if (event->y() < m_lineHeight) {
        // check if we want to move a keyframe
        int mousePos = qMax((int)(xPos / m_scale), 0);
        for (int i = 0; i < m_keyframes.count(); i++) {
            int kfrPos = m_keyframes.at(i);
            if (kfrPos * m_scale - xPos > headOffset) break;
            if (qAbs(kfrPos * m_scale - xPos) <  headOffset) {
                m_hoverKeyframe = kfrPos;
                setCursor(Qt::PointingHandCursor);
                event->accept();
                m_dragStart = event->pos();
                return;
            }
        }
    }
    int seekRequest = xPos / m_scale;
    if (seekRequest != m_position) {
        m_seekPosition = seekRequest;
        emit requestSeek(m_seekPosition);
        update();
    }
}

void AnimKeyframeRuler::leaveEvent( QEvent * event )
{
    Q_UNUSED(event)
    if (m_hoverKeyframe != -1) {
        m_hoverKeyframe = -1;
        update();
    }
}

// virtual
void AnimKeyframeRuler::mouseMoveEvent(QMouseEvent * event)
{
    int xPos = event->x() - margin;
    int headOffset = m_lineHeight / 1.5;
    if (event->buttons() == Qt::NoButton) {
        int mousePos = qMax((int)(xPos / m_scale), 0);
        if (qAbs(m_position * m_scale - xPos) < m_lineHeight && event->y() >= m_lineHeight) {
            // Mouse over time cursor
            if (m_hoverKeyframe != -2) {
                m_hoverKeyframe = -2;
                update();
            }
            event->accept();
            return;
        }
        if (event->y() < m_lineHeight) {
            // check if we want to move a keyframe
            for (int i = 0; i < m_keyframes.count(); i++) {
                int kfrPos = m_keyframes.at(i);
                if (kfrPos * m_scale - xPos > headOffset) break;
                if (qAbs(kfrPos * m_scale - xPos) <  headOffset) {
                    m_hoverKeyframe = kfrPos;
                    setCursor(Qt::PointingHandCursor);
                    update();
                    event->accept();
                    return;
                }
            }
        }
        if (m_hoverKeyframe != -1) {
            m_hoverKeyframe = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
        event->accept();
        return;
    }
    if (!m_dragStart.isNull() || m_movingKeyframe) {
        if (!m_movingKeyframe) {
            if ((QPoint(xPos, event->y()) - m_dragStart).manhattanLength() < QApplication::startDragDistance()) return;
            m_movingKeyframe = true;
            int index = m_keyframes.indexOf(m_hoverKeyframe);
            m_movingKeyframeType = m_keyframeTypes.value(index);
            m_keyframeTypes.removeAt(index);
            m_keyframes.removeAt(index);
            m_dragStart = QPoint();
        }
        m_movingKeyframePos = qBound(0, (int)(xPos / m_scale), frameLength);
        if (KdenliveSettings::snaptopoints() && qAbs(m_movingKeyframePos - m_position) < headOffset)
            m_movingKeyframePos = m_position;
        update();
        return;
    }
    m_seekPosition = (int) (xPos / m_scale);
    m_seekPosition = qMax(0, m_seekPosition);
    m_seekPosition = qMin(frameLength, m_seekPosition);
    m_hoverKeyframe = -2;
    emit requestSeek(m_seekPosition);
    update();
}

void AnimKeyframeRuler::mouseDoubleClickEvent(QMouseEvent * event)
{
    QWidget::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton) {
        // check if we want to move a keyframe
        int xPos = event->x() - margin;
        int headOffset = m_lineHeight / 1.5;
        for (int i = 0; i < m_keyframes.count(); i++) {
            int kfrPos = m_keyframes.at(i);
            if (kfrPos * m_scale - xPos > headOffset) break;
            if (qAbs(kfrPos * m_scale - xPos) <  headOffset) {
                // There is already a keyframe close to mouse click
                emit removeKeyframe(kfrPos);
                return;
            }
        }
        // add new keyframe
        emit addKeyframe((int)(xPos / m_scale));
    }
}

// virtual
void AnimKeyframeRuler::mouseReleaseEvent(QMouseEvent * event)
{
    setCursor(Qt::ArrowCursor);
    QWidget::mouseReleaseEvent(event);
    if (m_movingKeyframe) {
        emit moveKeyframe(m_hoverKeyframe, m_movingKeyframePos);
    } else if (!m_dragStart.isNull()) {
        // Seek to selected keyframe
        m_seekPosition = m_hoverKeyframe;
        m_dragStart = QPoint();
        emit requestSeek(m_seekPosition);
    }
    m_movingKeyframe = false;
    m_hoverKeyframe = -1;
}

// virtual
void AnimKeyframeRuler::wheelEvent(QWheelEvent * e)
{
    if (e->delta() < 0)
        --m_position;
    else
        ++m_position;
    m_position = qMax(0, m_position);
    m_position = qMin(frameLength, m_position);
    emit requestSeek(m_position);
    update();
    /*    int delta = 1;
        if (e->modifiers() == Qt::ControlModifier) delta = m_timecode.fps();
        if (e->delta() < 0) delta = 0 - delta;
        m_view->moveCursorPos(delta);
    */
}

// virtual
void AnimKeyframeRuler::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing);
    const QRectF clipRect = e->rect();
    p.setClipRect(clipRect);
    m_scale = (double) (width() - 2 * margin) / frameLength;
    int headOffset = m_lineHeight / 1.5;
    QPolygon polygon;
    for (int i = 0; i < m_keyframes.count(); i++) {
            int pos  = m_keyframes.at(i);
            int scaledPos = margin + pos * m_scale;
            // draw keyframes
            if (pos == m_position || pos == m_hoverKeyframe) {
                // active keyframe
                p.setBrush(m_selected);
                p.setPen(m_selected);
            }
            else {
                p.setPen(palette().text().color());
                p.setBrush(palette().text());
            }
            p.drawLine(scaledPos, headOffset, scaledPos, m_size);
            mlt_keyframe_type type = (mlt_keyframe_type) m_keyframeTypes.at(i);
            switch(type) {
                case mlt_keyframe_discrete:
                    p.drawRect(scaledPos - headOffset / 2, 0, headOffset, headOffset);
                    break;
                case mlt_keyframe_linear:
                    polygon.setPoints(3, scaledPos, 0, scaledPos + headOffset/2, headOffset, scaledPos - headOffset/2, headOffset);
                    p.drawPolygon(polygon);
                    break;
                default:
                    p.drawEllipse(scaledPos - headOffset / 2, 0, headOffset, headOffset);
                    break;
            }
        }
        if (m_movingKeyframe) {
            p.setBrush(m_selected);
            int scaledPos = margin + (int)(m_movingKeyframePos * m_scale);
            // draw keyframes
            p.drawLine(scaledPos, headOffset, scaledPos, m_size);
            switch((int) m_movingKeyframeType) {
                case mlt_keyframe_discrete:
                    p.drawRect(scaledPos - headOffset / 2, 0, headOffset, headOffset);
                    break;
                case mlt_keyframe_linear:
                    polygon.setPoints(3, scaledPos, 0, scaledPos + headOffset/2, headOffset, scaledPos - headOffset/2, headOffset);
                    p.drawPolygon(polygon);
                    break;
                default:
                    p.drawEllipse(scaledPos - headOffset / 2, 0, headOffset, headOffset);
                    break;
            }
            p.setBrush(m_keyframe);
        }
    p.setPen(palette().text().color());
    p.drawLine(margin, m_lineHeight + (headOffset / 2), width() - margin - 1, m_lineHeight + (headOffset / 2));
    p.drawLine(margin, m_lineHeight - headOffset, margin, m_lineHeight + headOffset);
    p.drawLine(width() - margin, m_lineHeight - headOffset, width() - margin, m_lineHeight + headOffset);
    p.setPen(Qt::NoPen);
    // draw pointer
    if (m_seekPosition != SEEK_INACTIVE) {
        p.fillRect(margin + m_seekPosition * m_scale - 1, 0, 3, height(), palette().dark());
    }
    QPolygon pa(3);
    const int cursor = margin + m_position * m_scale;
    int cursorwidth = (m_size - (m_lineHeight + headOffset / 2)) / 2 + 1;
    pa.setPoints(3, cursor - cursorwidth, m_size, cursor + cursorwidth, m_size, cursor, m_lineHeight + (headOffset / 2) + 1);
    if (m_hoverKeyframe == -2)
        p.setBrush(palette().highlight());
    else
        p.setBrush(palette().text());
    p.drawPolygon(pa);
}

int AnimKeyframeRuler::value() const
{
    if (m_seekPosition == SEEK_INACTIVE) {
        return m_position;
    }
    return m_seekPosition;
}

void AnimKeyframeRuler::setValue(const int pos)
{
    if (pos == m_position)
        return;
    if (pos == m_seekPosition) {
        m_seekPosition = SEEK_INACTIVE;
    }
    m_position = pos;
    update();
}



/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "selectmanager.h"
#include "../customtrackview.h"
#include "../clipitem.h"
#include "../abstractclipitem.h"
#include "../abstractgroupitem.h"
#include "../gentime.h"
#include "bin/projectclip.h"
#include "mltcontroller/clipcontroller.h"

#include <QMouseEvent>
#include <QGraphicsItem>

#include "klocalizedstring.h"

void SelectManager::checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, AbstractGroupItem *group, OperationType &operationMode, OperationType moveOperation)
{
    if (item && event->buttons() == Qt::NoButton && operationMode != ZoomTimeline) {
        AbstractClipItem *clip = static_cast <AbstractClipItem*>(item);

        if (group && clip->parentItem() == group) {
            // all other modes break the selection, so the user probably wants to move it
            operationMode = MoveOperation;
        } else {
            if (clip->rect().width() * view->transform().m11() < 15) {
                // If the item is very small, only allow move
                operationMode = MoveOperation;
            }
            else operationMode = clip->operationMode(clip->mapFromScene(view->mapToScene(event->pos())));
        }

        if (operationMode == moveOperation) {
            view->graphicsViewMouseEvent(event);
            return;
        }
        ClipItem *ci = NULL;
        if (item->type() == AVWidget)
            ci = static_cast <ClipItem *>(item);
        QString message;
        if (operationMode == MoveOperation) {
            view->setCursor(Qt::OpenHandCursor);
            if (ci) {
                message = ci->clipName() + i18n(":");
                message.append(i18n(" Position:") + view->getDisplayTimecode(ci->info().startPos));
                message.append(i18n(" Duration:") + view->getDisplayTimecode(ci->cropDuration()));
                if (clip->parentItem() && clip->parentItem()->type() == GroupWidget) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(clip->parentItem());
                    if (clip->parentItem() == group)
                        message.append(i18n(" Selection duration:"));
                    else
                        message.append(i18n(" Group duration:"));
                    message.append(view->getDisplayTimecode(parent->duration()));
                    if (parent->parentItem() && parent->parentItem()->type() == GroupWidget) {
                        AbstractGroupItem *parent2 = static_cast <AbstractGroupItem *>(parent->parentItem());
                        message.append(i18n(" Selection duration:") + view->getDisplayTimecode(parent2->duration()));
                    }
                }
            }
        } else if (operationMode == ResizeStart) {
            view->setCursor(QCursor(Qt::SizeHorCursor));
            if (ci)
                message = i18n("Crop from start: ") + view->getDisplayTimecode(ci->cropStart());
            if (item->type() == AVWidget && item->parentItem() && item->parentItem() != group)
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
        } else if (operationMode == ResizeEnd) {
            view->setCursor(QCursor(Qt::SizeHorCursor));
            if (ci)
                message = i18n("Duration: ") + view->getDisplayTimecode(ci->cropDuration());
            if (item->type() == AVWidget && item->parentItem() && item->parentItem() != group)
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
        } else if (operationMode == FadeIn || operationMode == FadeOut) {
            view->setCursor(Qt::PointingHandCursor);
            if (ci && operationMode == FadeIn && ci->fadeIn()) {
                message = i18n("Fade in duration: ");
                message.append(view->getDisplayTimecodeFromFrames(ci->fadeIn()));
            } else if (ci && operationMode == FadeOut && ci->fadeOut()) {
                message = i18n("Fade out duration: ");
                message.append(view->getDisplayTimecodeFromFrames(ci->fadeOut()));
            } else {
                message = i18n("Drag to add or resize a fade effect.");
            }
        } else if (operationMode == TransitionStart || operationMode == TransitionEnd) {
            view->setCursor(Qt::PointingHandCursor);
            message = i18n("Click to add a transition.");
        } else if (operationMode == KeyFrame) {
            view->setCursor(Qt::PointingHandCursor);
             QMetaObject::invokeMethod(view, "displayMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Move keyframe above or below clip to remove it, double click to add a new one.")), Q_ARG(MessageType, InformationMessage));
        }

        if (!message.isEmpty())
            QMetaObject::invokeMethod(view, "displayMessage", Qt::QueuedConnection, Q_ARG(QString, message), Q_ARG(MessageType, InformationMessage));
    } else if (event->buttons() == Qt::NoButton && (moveOperation == None || moveOperation == WaitingForConfirm)) {
        view->setCursor(Qt::ArrowCursor);
    }
}


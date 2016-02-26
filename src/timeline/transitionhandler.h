/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

/**
* @class TransitionHandler
* @brief Manages the transitions operations in timeline
* @author Jean-Baptiste Mardelle
*/

#ifndef TRANSITIONHANDLER_H
#define TRANSITIONHANDLER_H

#include "definitions.h"
#include <mlt++/Mlt.h>


class TransitionHandler : public QObject
{
    Q_OBJECT

public:
    explicit TransitionHandler(Mlt::Tractor *tractor);
    bool addTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool do_refresh = true);
    QMap<QString, QString> getTransitionParamsFromXml(const QDomElement &xml);
    void plantTransition(Mlt::Transition &tr, int a_track, int b_track);
    void plantTransition(Mlt::Field *field, Mlt::Transition &tr, int a_track, int b_track);
    void cloneProperties(Mlt::Properties &dest, Mlt::Properties &source);
    void updateTransition(QString oldTag, QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool force = false);
    void updateTransitionParams(QString type, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml);
    void deleteTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool refresh = true);
    void deleteTrackTransitions(int ix);
    bool moveTransition(QString type, int startTrack,  int newTrack, int newTransitionTrack, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut);
    QList <TransitionInfo> mltInsertTrack(int ix, const QString &name, bool videoTrack);
    void duplicateTransitionOnPlaylist(int in, int out, QString tag, QDomElement xml, int a_track, int b_track, Mlt::Field *field);
    /** @brief Get a transition with tag name. */
    Mlt::Transition *getTransition(const QString &name, int b_track, int a_track = -1, bool internalTransition = false) const;
    /** @brief Enable/disable multitrack split view. */
    void enableMultiTrack(bool enable);
    /** @brief Plug composite transitions depending on the en/disabled states. */
    void rebuildComposites(int lowestVideoTrack);

private:
    Mlt::Tractor *m_tractor;

signals:
    void refresh();
};

#endif

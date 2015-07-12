/*
 * Kdenlive timeline track handling MLT playlist
 * Copyright 2015 Kdenlive team <kdenlive@kde.org>
 * Author: Vincent Pinon <vpinon@kde.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef TRACK_H
#define TRACK_H

#include "definitions.h"

#include <QObject>

#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProducer.h>

/** @brief Kdenlive timeline track, to access MLT playlist operations
 * The track as seen in the video editor is actually a playlist
 * in the MLT framework behind the scene.
 * When one adds, deletes, moves and resizes clips on the track,
 * it corresponds to playlist operations, simple or elaborate.
 * The Track class handles the edit time to MLT frame & index
 * correspondance, and builds complex operations on top of basic
 * commands.
 */
class Track : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mlt::Playlist playlist READ playlist WRITE setPlaylist)
    Q_PROPERTY(qreal fps READ fps WRITE setFps)

public:
    /** @brief Track constructor
     * @param playlist is the MLT object used for monitor/render
     * @param fps is the read speed (frames per seconds) */
    explicit Track(Mlt::Playlist &playlist, TrackType type, qreal fps);
    ~Track();

    /// Property access function
    Mlt::Playlist & playlist();
    qreal fps();
    /** List containing track effects */
    EffectsList effectsList;
    /** Track type (audio / video) */
    TrackType type;

    /** @brief convertion utility function
     * @param time (in seconds)
     * @return frame number */
    int frame(qreal t);
    /** @brief get the playlist duration
     * @return play time in seconds */
    qreal length();

    /** @brief add a clip
     * @param t is the time position to start the cut (in seconds)
     * @param cut is a MLT Producer cut (resource + in/out timecodes)
     * @param duplicate when true, we will create a copy of the clip if necessary
     * @param mode allow insert in non-blanks by replacing (mode=1) or pushing (mode=2) content
     * The playlist must be locked / unlocked before and after calling doAdd
     * @return true if success */
    bool doAdd(qreal t, Mlt::Producer *cut, int mode);
    bool add(qreal t, Mlt::Producer *parent, bool duplicate, int mode = 0);
    bool add(qreal t, Mlt::Producer *parent, qreal tcut, qreal dtcut, PlaylistState::ClipState state, bool duplicate, int mode);
    /** @brief Move a clip in the track
     * @param start where clip is present (in seconds);
     * @param end wher the clip should be moved
     * @param mode allow insert in non-blanks by replacing (mode=1) or pushing (mode=2) content
     * @return true if success */
    bool move(qreal start, qreal end, int mode = 0);
    /** @brief delete a clip
     * @param time where clip is present (in seconds);
     * @return true if success */
    bool del(qreal t);
    /** delete a region
     * @param t is the start,
     * @param dt is the duration (in seconds)
     * @return true if success */
    bool del(qreal t, qreal dt);
    /** @brief change the clip length from start or end
     * @param told is the current edge position,
     * @param tnew is the target edge position (in seconds)
     * @param end precises if we move the end of the left clip (\em true)
     *  or the start of the right clip (\em false)
     * @return true if success */
    bool resize(qreal told, qreal tnew, bool end);
    /** @brief split the clip at given position
     * @param t is the cut time in playlist
     * @return true if success */
    bool cut(qreal t);
    /** @brief replace all occurences of a clip in the track with another resource
     * @param id is the clip id
     * @param original is the original replacement clip
     * @param videoOnlyProducer is the video only (without sound) replacement clip
     * @return true if success */
    bool replaceAll(const QString &id, Mlt::Producer *original, Mlt::Producer *videoOnlyProducer);
    /** @brief replace an instance of a clip with another resource
     * @param t is the clip time in playlist
     * @param prod is the replacement clip
     * @return true if success */
    bool replace(qreal t, Mlt::Producer *prod, PlaylistState::ClipState state = PlaylistState::Original);
    /** @brief look for a clip having a given property value
     * @param name is the property name
     * @param value is the searched value
     * @param startindex is a playlist index to start the search from
     * @return pointer to the first matching producer */
    Mlt::Producer *find(const QByteArray &name, const QByteArray &value, int startindex = 0);
    /** @brief get a producer clone for the track and pick an extract
     * MLT (libav*) can't mix audio of a clip with itself, so we duplicate the producer for each track
     * @param parent is the source media
     * @param state is for Normal, Audio only or Video only
     * @param forceCreation if true, we do not attempt to re-use existing track producer but recreate it
     * @return producer cut for this track */
    Mlt::Producer *clipProducer(Mlt::Producer *parent, PlaylistState::ClipState state, bool forceCreation = false);
    /** @brief Returns true if there is a clip with audio on this track */
    bool hasAudio();
    void setProperty(const QString &name, const QString &value);
    void setProperty(const QString &name, int value);
    const QString getProperty(const QString &name);
    int getIntProperty(const QString &name);
    TrackInfo info();
    void setInfo(TrackInfo info);
    int state();
    void setState(int state);
    /** @brief Check if we have a blank space at pos and its length. 
     *  Returns -1 if track is shorter, 0 if not blank and > 0 for blank length */
    int getBlankLength(int pos, bool fromBlankStart);

public Q_SLOTS:
    void setPlaylist(Mlt::Playlist &playlist);
    void setFps(qreal fps);

signals:
    /** @brief notify track length change to update background
     * @param duration is the new length */
    void newTrackDuration(int duration);

private:
    /** MLT playlist behind the scene */
    Mlt::Playlist m_playlist;
    /** frames per second (read speed) */
    qreal m_fps;
    /** @brief Returns true is this MLT service needs duplication to work on multiple tracks */
    bool needsDuplicate(const QString &service) const;
};

#endif // TRACK_H

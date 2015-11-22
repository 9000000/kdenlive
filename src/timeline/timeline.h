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

/**
* @class Timeline
* @brief Manages the timline
* @author Jean-Baptiste Mardelle
*/

#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include "timeline/customtrackscene.h"
#include "effectslist/effectslist.h"
#include "ui_timeline_ui.h"
#include "definitions.h"

#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QDomElement>

#include <mlt++/Mlt.h>

class Track;
class ClipItem;
class CustomTrackView;
class KdenliveDoc;
class TransitionHandler;
class CustomRuler;
class QUndoCommand;

class Timeline : public QWidget, public Ui::TimeLine_UI
{
    Q_OBJECT

public:
    explicit Timeline(KdenliveDoc *doc, const QList <QAction *>& actions, bool *ok, QWidget *parent = 0);
    virtual ~ Timeline();
    Track* track(int i);
    /** @brief Number of tracks in the MLT playlist. */
    int tracksCount() const;
    /** @brief Number of visible tracks (= tracksCount() - 1 ) because black trck is not visible to user. */
    int visibleTracksCount() const;
    void setEditMode(const QString & editMode);
    const QString & editMode() const;
    QGraphicsScene *projectScene();
    CustomTrackView *projectView();
    int duration() const;
    KdenliveDoc *document();
    void refresh() ;
    void updateProjectFps();
    int outPoint() const;
    int inPoint() const;
    int fitZoom() const;
    /** @brief This object handles all transition operation. */
    TransitionHandler *transitionHandler;
    void lockTrack(int ix, bool lock);
    bool isTrackLocked(int ix);
    /** @brief Dis / enable video for a track. */
    void switchTrackVideo(int ix, bool hide);
    /** @brief Dis / enable audio for a track. */
    void switchTrackAudio(int ix, bool mute);
    /** @brief find lowest track with audio in timeline. */
    int getLowestNonMutedAudioTrack();
    /** @brief Adjust audio transitions depending on tracks muted state. */
    void fixAudioMixing();

    /** @brief Updates (redraws) the ruler.
    *
    * Used to change from displaying frames to timecode or vice versa. */
    void updateRuler();

    /** @brief Parse tracks to see if project has audio in it.
    *
    * Parses all tracks to check if there is audio data. */
    bool checkProjectAudio();
    
    /** @brief Load guides from data */
    void loadGuides(QMap <double, QString> guidesData);

    void checkTrackHeight(bool force = false);
    void updateProfile();
    void updatePalette();
    void refreshIcons();
    /** @brief Returns a kdenlive effect xml description from an effect tag / id */
    static QDomElement getEffectByTag(const QString &effecttag, const QString &effectid);
    /** @brief Move a clip between tracks */
    bool moveClip(int startTrack, qreal startPos, int endTrack, qreal endPos, PlaylistState::ClipState state, int mode, bool duplicate);
    void renameTrack(int ix, const QString &name);
    void updateTrackState(int ix, int state);
    /** @brief Returns info about a track.
     *  @param ix The track number in MLT's coordinates (0 = black track, 1 = bottom audio, etc) 
     *  deprecated use string version with track name instead */
    TrackInfo getTrackInfo(int ix);
    int getTrackIndex(const QString &id);
    void setTrackInfo(int trackIndex, TrackInfo info);
    QList <TrackInfo> getTracksInfo();
    void addTrackEffect(int trackIndex, QDomElement effect);
    void removeTrackEffect(int trackIndex, const QDomElement &effect);
    void setTrackEffect(int trackIndex, int effectIndex, QDomElement effect);
    void enableTrackEffects(int trackIndex, const QList <int> &effectIndexes, bool disable);
    const EffectsList getTrackEffects(int trackIndex);
    QDomElement getTrackEffect(int trackIndex, int effectIndex);
    int hasTrackEffect(int trackIndex, const QString &tag, const QString &id);
    MltVideoProfile mltProfile() const;
    double fps() const;
    QPoint getTracksCount();
    /** @brief Check if we have a blank space on selected track. 
     *  Returns -1 if track is shorter, 0 if not blank and > 0 for blank length */
    int getTrackSpaceLength(int trackIndex, int pos, bool fromBlankStart);
    void updateClipProperties(const QString &id, QMap <QString, QString> properties);
    int changeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, PlaylistState::ClipState state, double speed, int strobe, Mlt::Producer *originalProd, bool removeEffect = false);
    /** @brief Set an effect's XML accordingly to MLT::filter values. */
    static void setParam(ProfileInfo info, QDomElement param, QString value);
    int getTracks();
    void getTransitions();
    void refreshTractor();
    void saveZone(const QUrl url, const QPoint &zone);
    void duplicateClipOnPlaylist(int tk, qreal startPos, int offset, Mlt::Producer *prod);
    int getSpaceLength(const GenTime &pos, int tk, bool fromBlankStart);
    void blockTrackSignals(bool block);
    /** @brief Load document */
    void loadTimeline();
    /** @brief Dis/enable all effects in timeline*/
    void disableTimelineEffects(bool disable);
    static bool isSlide(QString geometry);
    /** @brief Import amultitrack MLT playlist in timeline */
    void importPlaylist(ItemInfo info, QMap <QString, QString> processedUrl, QMap <QString, QString> idMaps, QDomDocument doc, QUndoCommand *command);

protected:
    void keyPressEvent(QKeyEvent * event);

public slots:
    void slotDeleteClip(const QString &clipId, QUndoCommand *deleteCommand);
    void slotChangeZoom(int horizontal, int vertical = -1);
    void setDuration(int dur);
    void slotSetZone(const QPoint &p, bool updateDocumentProperties = true);
    /** @brief Save a snapshot image of current timeline view */
    void slotSaveTimelinePreview(const QString &path);
    void checkDuration(int duration);
    void slotShowTrackEffects(int);

private:
    Mlt::Tractor *m_tractor;
    QList <Track*> m_tracks;
    CustomRuler *m_ruler;
    CustomTrackView *m_trackview;
    QList <QString> m_invalidProducers;
    double m_scale;
    QString m_editMode;
    CustomTrackScene *m_scene;
    /** @brief A list of producer ids to be replaced when opening a corrupted document*/
    QMap <QString, QString> m_replacementProducerIds;

    KdenliveDoc *m_doc;
    int m_verticalZoom;
    QString m_documentErrors;
    QList <QAction *> m_trackActions;

    void adjustTrackHeaders();

    void parseDocument(const QDomDocument &doc);
    int loadTrack(int ix, int offset, Mlt::Playlist &playlist);
    void getEffects(Mlt::Service &service, ClipItem *clip, int track = 0);
    QString getKeyframes(Mlt::Service service, int &ix, QDomElement e);
    void getSubfilters(Mlt::Filter *effect, QDomElement &currenteffect);
    void adjustDouble(QDomElement &e, double value);

    /** @brief Adjust kdenlive effect xml parameters to the MLT value*/
    void adjustparameterValue(QDomNodeList clipeffectparams, const QString &paramname, const QString &paramvalue);

private slots:
    void slotSwitchTrackComposite(int trackIndex, bool enable);
    void setCursorPos(int pos);
    void moveCursorPos(int pos);
    /** @brief The tracks count or a track name changed, rebuild and notify */
    void slotReloadTracks();
    void slotVerticalZoomDown();
    void slotVerticalZoomUp();
    /** @brief Changes the name of a track.
    * @param ix Number of the track
    * @param name New name */
    void slotRenameTrack(int ix, const QString &name);
    void slotRepaintTracks();

    /** @brief Adjusts the margins of the header area.
     *
     * Avoid a shift between header area and trackview if
     * the horizontal scrollbar is visible and the position
     * of the vertical scrollbar is maximal */
    void slotUpdateVerticalScroll(int min, int max);
    /** @brief Update the track label showing applied effects.*/
    void slotUpdateTrackEffectState(int);

signals:
    void mousePosition(int);
    void cursorMoved();
    void zoneMoved(int, int);
    void configTrack();
    void updateTracksInfo();
    void setZoom(int);
    void showTrackEffects(int, const TrackInfo&);
    /** @brief Indicate how many clips we are going to load */
    void startLoadingBin(int);
    /** @brief Indicate which clip we are currently loading */
    void loadingBin(int);

};

#endif

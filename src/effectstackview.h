/***************************************************************************
                          effecstackview.h  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @class EffectStackView
 * @brief View part of the EffectStack
 * @author Marco Gittler
 */

#ifndef EFFECTSTACKVIEW_H
#define EFFECTSTACKVIEW_H

#include "ui_effectstack_ui.h"
#include "effectstackedit.h"

class EffectsList;
class ClipItem;
class MltVideoProfile;
class Monitor;

class EffectStackView : public QWidget
{
    Q_OBJECT

public:
    EffectStackView(Monitor *monitor, QWidget *parent = 0);
    virtual ~EffectStackView();

    /** @brief Raises @param dock if a clip is loaded. */
    void raiseWindow(QWidget* dock);

    /** @brief Clears the list of effects and updates the buttons accordingly. */
    void clear();

    /** @brief Sets the add effect button's menu to @param menu. */
    void setMenu(QMenu *menu);

    /** @brief Passes updates on @param profile and @param t on to the effect editor. */
    void updateProjectFormat(MltVideoProfile profile, Timecode t);

    /** @brief Tells the effect editor to update its timecode format. */
    void updateTimecodeFormat();

    /** @brief return the index of the track displayed in effect stack
     ** @param ok set to true if we are looking at a track's effects, otherwise false. */
    int isTrackMode(bool *ok) const;

private:
    Ui::EffectStack_UI m_ui;
    Monitor *m_monitor;
    ClipItem* m_clipref;
    QMap<QString, EffectsList*> m_effectLists;
    EffectsList m_currentEffectList;
    EffectStackEdit* m_effectedit;

    /** @brief Effectstackview can show the effects of a clip or the effects of a track.
     * true if showing track effects. */
    bool m_trackMode;

    /** @brief The track index of currently edited track. */
    int m_trackindex;

    /** If in track mode: Info of the edited track to be able to access its duration. */
    TrackInfo m_trackInfo;

    /** @brief Sets the list of effects according to the clip's effect list.
    * @param ix Number of the effect to preselect */
    void setupListView(int ix);

public slots:
    /** @brief Sets the clip whose effect list should be managed.
    * @param c Clip whose effect list should be managed
    * @param ix Effect to preselect */
    void slotClipItemSelected(ClipItem* c, int ix);

    void slotTrackItemSelected(int ix, const TrackInfo info);

    /** @brief Emits updateClipEffect.
    * @param old Old effect information
    * @param e New effect information
    *
    * Connected to a parameter change in the editor */
    void slotUpdateEffectParams(const QDomElement &old, const QDomElement &e);

    /** @brief Removes the selected effect. */
    void slotItemDel();

private slots:
    /** @brief Updates buttons and the editor according to selected effect.
    * @param update (optional) Set the clip's selected effect (display keyframes in timeline) */
    void slotItemSelectionChanged(bool update = true);

    /** @brief Moves the selected effect upwards. */
    void slotItemUp();

    /** @brief Moves the selected effect downwards. */
    void slotItemDown();

    /** @brief Resets the selected effect to its default values. */
    void slotResetEffect();

    /** @brief Updates effect @param item if it was enabled or disabled. */
    void slotItemChanged(QListWidgetItem *item);

    /** @brief Saves the selected effect's values to a custom effect.
    *
    * TODO: save all effects into one custom effect */
    void slotSaveEffect();

    /** @brief Emits seekTimeline with position = clipstart + @param pos. */
    void slotSeekTimeline(int pos);

    /** @brief Makes the check all checkbox represent the check state of the effects. */
    void slotUpdateCheckAllButton();

    /** @brief Sets the check state of all effects according to @param state. */
    void slotCheckAll(int state);

    /* @brief Define the region filter for current effect.
    void slotRegionChanged();*/

    /** @brief Checks whether the monitor scene has to be displayed. */
    void slotCheckMonitorPosition(int renderPos);

    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotRenderPos(int pos);

    /** @brief Shows/Hides the comment box and emits showComments to notify the parameter widgets to do the same. */
    void slotShowComments();

signals:
    void removeEffect(ClipItem*, int, QDomElement);
    /**  Parameters for an effect changed, update the filter in playlist */
    void updateEffect(ClipItem*, int, QDomElement, QDomElement, int);
    /** An effect in stack was moved, we need to regenerate
        all effects for this clip in the playlist */
    void refreshEffectStack(ClipItem *);
    /** Enable or disable an effect */
    void changeEffectState(ClipItem*, int, int, bool);
    /** An effect in stack was moved */
    void changeEffectPosition(ClipItem*, int, int, int);
    /** an effect was saved, reload list */
    void reloadEffects();
    /** An effect with position parameter was changed, seek */
    void seekTimeline(int);
    /** The region effect for current effect was changed */
    void updateClipRegion(ClipItem*, int, QString);
    void displayMessage(const QString&, int);
    void showComments(bool show);
};

#endif

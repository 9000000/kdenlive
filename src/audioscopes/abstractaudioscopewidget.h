/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ABSTRACTAUDIOSCOPEWIDGET_H
#define ABSTRACTAUDIOSCOPEWIDGET_H


#include <QtCore>
#include <QWidget>

class QMenu;

class Monitor;
class Render;

class AbstractAudioScopeWidget : public QWidget
{
    Q_OBJECT
public:
    AbstractAudioScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, bool trackMouse = false, QWidget *parent = 0);
    virtual ~AbstractAudioScopeWidget(); // Must be virtual because of inheritance, to avoid memory leaks
    QPalette m_scopePalette;

    /** Initializes widget settings (reads configuration).
      Has to be called in the implementing object. */
    void init();

    /** Does this scope have auto-refresh enabled */
    bool autoRefreshEnabled();

    ///// Unimplemented /////

    virtual QString widgetName() const = 0;

    ///// Variables /////
    static const QPen penThick;
    static const QPen penThin;
    static const QPen penLight;
    static const QPen penDark;

protected:
    ///// Variables /////

    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;


    /** The context menu. Feel free to add new entries in your implementation. */
    QMenu *m_menu;

    /** Enables auto refreshing of the scope.
        This is when a new frame is shown on the active monitor.
        Resize events always force a recalculation. */
    QAction *m_aAutoRefresh;

    /** Realtime rendering. Should be disabled if it is not supported.
        Use the accelerationFactor variable passed to the render functions as a hint of
        how many times faster the scope should be calculated. */
    QAction *m_aRealtime;

    /** The mouse position; Updated when the mouse enters the widget
        AND mouse tracking has been enabled. */
    QPoint m_mousePos;
    /** Knows whether the mouse currently lies within the widget or not.
        Can e.g. be used for drawing a HUD only when the mouse is in the widget. */
    bool m_mouseWithinWidget;

    /** Offset from the widget's borders */
    const uchar offset;

    /** The rect on the widget we're painting in.
        Can be used by the implementing widget, e.g. in the render methods.
        Is updated when necessary (size changes). */
    QRect m_scopeRect;

    /** Images storing the calculated layers. Will be used on repaint events. */
    QImage m_imgHUD;
    QImage m_imgScope;
    QImage m_imgBackground;

    /** The acceleration factors can be accessed also by other renderer tasks,
        e.g. to display the scope's acceleration factor in the HUD renderer. */
    int m_accelFactorHUD;
    int m_accelFactorScope;
    int m_accelFactorBackground;

    /** Reads the widget's configuration.
        Can be extended in the implementing subclass (make sure to run readConfig as well). */
    virtual void readConfig();
    /** Writes the widget configuration.
        Implementing widgets have to implement an own method and run it in their destructor. */
    void writeConfig();
    /** Identifier for the widget's configuration. */
    QString configName();


    ///// Unimplemented Methods /////

    /** Where on the widget we can paint in.
        May also update other variables that depend on the widget's size.  */
    virtual QRect scopeRect() = 0;

    /** @brief HUD renderer. Must emit signalHUDRenderingFinished(). @see renderScope */
    virtual QImage renderHUD(uint accelerationFactor) = 0;
    /** @brief Scope renderer. Must emit signalScopeRenderingFinished()
        when calculation has finished, to allow multi-threading.
        accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible. */
    virtual QImage renderScope(uint accelerationFactor,
                               const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples) = 0;
    /** @brief Background renderer. Must emit signalBackgroundRenderingFinished(). @see renderScope */
    virtual QImage renderBackground(uint accelerationFactor) = 0;

    /** Must return true if the HUD layer depends on the input monitor.
        If it does not, then it does not need to be re-calculated when
        a new frame from the monitor is incoming. */
    virtual bool isHUDDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isScopeDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isBackgroundDependingOnInput() const = 0;

    ///// Can be reimplemented /////
    /** Calculates the acceleration factor to be used by the render thread.
        This method can be refined in the subclass if required. */
    virtual uint calculateAccelFactorHUD(uint oldMseconds, uint oldFactor);
    virtual uint calculateAccelFactorScope(uint oldMseconds, uint oldFactor);
    virtual uint calculateAccelFactorBackground(uint oldMseconds, uint oldFactor);

    ///// Reimplemented /////

    void mouseMoveEvent(QMouseEvent *);
    void leaveEvent(QEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);
    void showEvent(QShowEvent *); // Called when the widget is activated via the Menu entry
    //    void raise(); // Called only when  manually calling the event -> useless


protected slots:
    /** Forces an update of all layers. */
    void forceUpdate(bool doUpdate = true);
    void forceUpdateHUD();
    void forceUpdateScope();
    void forceUpdateBackground();
    void slotAutoRefreshToggled(bool);

signals:
    /** mseconds represent the time taken for the calculation,
        accelerationFactor is the acceleration factor that has been used. */
    void signalHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

    /** For the mouse position itself see m_mousePos.
        To check whether the mouse has leaved the widget, see m_mouseWithinWidget. */
    void signalMousePositionChanged();

    /** Do we need the renderer to send its frames to us? */
    void requestAutoRefresh(bool);

private:

    /** Counts the number of frames that have been rendered in the active monitor.
      The frame number will be reset when the calculation starts for the current frame. */
    QAtomicInt m_newHUDFrames;
    QAtomicInt m_newScopeFrames;
    QAtomicInt m_newBackgroundFrames;

    /** Counts the number of updates that, unlike new frames, force a recalculation
      of the scope, like for example a resize event. */
    QAtomicInt m_newHUDUpdates;
    QAtomicInt m_newScopeUpdates;
    QAtomicInt m_newBackgroundUpdates;

    /** The semaphores ensure that the QFutures for the HUD/Scope/Background threads cannot
      be assigned a new thread while it is still running. (Could cause deadlocks and other
      nasty things known from parallelism.) */
    QSemaphore m_semaphoreHUD;
    QSemaphore m_semaphoreScope;
    QSemaphore m_semaphoreBackground;

    QFuture<QImage> m_threadHUD;
    QFuture<QImage> m_threadScope;
    QFuture<QImage> m_threadBackground;

    bool initialDimensionUpdateDone;
    bool m_requestForcedUpdate;

//    QImage m_scopeImage;
    QVector<int16_t> m_audioFrame; //NEW
    int m_freq;
    int m_nChannels;
    int m_nSamples;

    QString m_widgetName;

    void prodHUDThread();
    void prodScopeThread();
    void prodBackgroundThread();

public slots:
    /** @brief Must be called when the active monitor has shown a new frame.
      This slot must be connected in the implementing class, it is *not*
      done in this abstract class. */
//    void slotActiveMonitorChanged(bool isClipMonitor);

private slots:
    void customContextMenuRequested(const QPoint &pos);
    /** To be called when a new frame has been received.
      The scope then decides whether and when it wants to recalculate the scope, depending
      on whether it is currently visible and whether a calculation thread is already running. */
    void slotRenderZoneUpdated();
//    void slotRenderZoneUpdated(QImage);//OLD
    void slotReceiveAudio(const QVector<int16_t>& sampleData, int freq, int num_channels, int num_samples); // NEW, TODO comment
    /** The following slots are called when rendering of a component has finished. They e.g. update
      the widget and decide whether to immediately restart the calculation thread. */
    void slotHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

    /** Resets the acceleration factors to 1 when realtime rendering is disabled. */
    void slotResetRealtimeFactor(bool realtimeChecked);

};

#endif // ABSTRACTAUDIOSCOPEWIDGET_H

/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2015 by Vincent Pinon (vpinon@kde.org)                  *
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


#include "timeline.h"
#include "track.h"
#include "clip.h"
#include "renderer.h"
#include "headertrack.h"
#include "clipitem.h"
#include "transition.h"
#include "transitionhandler.h"
#include "timelinecommands.h"
#include "customruler.h"
#include "customtrackview.h"
#include "dialogs/profilesdialog.h"
#include "mltcontroller/clipcontroller.h"
#include "bin/projectclip.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "doc/kdenlivedoc.h"
#include "utils/KoIconUtils.h"
#include "project/clipmanager.h"
#include "effectslist/initeffects.h"
#include "mltcontroller/effectscontroller.h"

#include <QScrollBar>
#include <QLocale>
#include <KDualAction>

#include <KMessageBox>
#include <KIO/FileCopyJob>
#include <klocalizedstring.h>


Timeline::Timeline(KdenliveDoc *doc, const QList<QAction *> &actions, bool *ok, QWidget *parent) :
    QWidget(parent),
    multitrackView(false)
    , videoTarget(-1)
    , audioTarget(-1)
    , m_hasOverlayTrack(false)
    , m_overlayTrack(NULL)
    , m_scale(1.0)
    , m_doc(doc)
    , m_verticalZoom(1)
{
    m_trackActions << actions;
    setupUi(this);
    //    ruler_frame->setMaximumHeight();
    //    size_frame->setMaximumHeight();
    m_scene = new CustomTrackScene(this);
    m_trackview = new CustomTrackView(doc, this, m_scene, parent);
    if (m_doc->setSceneList() == -1) *ok = false;
    else *ok = true;
    Mlt::Service s(m_doc->renderer()->getProducer()->parent().get_service());
    m_tractor = new Mlt::Tractor(s);
    m_ruler = new CustomRuler(doc->timecode(), m_trackview);
    connect(m_ruler, SIGNAL(zoneMoved(int,int)), this, SIGNAL(zoneMoved(int,int)));
    connect(m_ruler, SIGNAL(adjustZoom(int)), this, SIGNAL(setZoom(int)));
    connect(m_ruler, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_ruler, SIGNAL(seekCursorPos(int)), m_doc->renderer(), SLOT(seek(int)), Qt::QueuedConnection);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(m_trackview->frameWidth(), 0, 0, 0);
    layout->setSpacing(0);
    ruler_frame->setLayout(layout);
    ruler_frame->setMaximumHeight(m_ruler->height());
    layout->addWidget(m_ruler);

    QHBoxLayout *sizeLayout = new QHBoxLayout;
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(0);
    size_frame->setLayout(sizeLayout);
    size_frame->setMaximumHeight(m_ruler->height());

    QToolButton *butSmall = new QToolButton(this);
    butSmall->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-zoom-small")));
    butSmall->setToolTip(i18n("Smaller tracks"));
    butSmall->setAutoRaise(true);
    connect(butSmall, SIGNAL(clicked()), this, SLOT(slotVerticalZoomDown()));
    sizeLayout->addWidget(butSmall);

    QToolButton *butLarge = new QToolButton(this);
    butLarge->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-zoom-large")));
    butLarge->setToolTip(i18n("Bigger tracks"));
    butLarge->setAutoRaise(true);
    connect(butLarge, SIGNAL(clicked()), this, SLOT(slotVerticalZoomUp()));
    sizeLayout->addWidget(butLarge);

    QToolButton *enableZone = new QToolButton(this);
    QAction *ac = new QAction(KoIconUtils::themedIcon(QStringLiteral("measure")), i18n("Use Timeline Zone for Insert"), this);
    ac->setShortcut(Qt::Key_G);
    enableZone->setAutoRaise(true);
    ac->setCheckable(true);
    ac->setChecked(KdenliveSettings::useTimelineZoneToEdit());
    enableZone->setDefaultAction(ac);
    connect(ac, &QAction::toggled, this, &Timeline::slotEnableZone);
    sizeLayout->addWidget(enableZone);
    m_doc->doAddAction(QStringLiteral("use_timeline_zone_in_edit"), ac);

    QHBoxLayout *tracksLayout = new QHBoxLayout;
    tracksLayout->setContentsMargins(0, 0, 0, 0);
    tracksLayout->setSpacing(0);
    tracks_frame->setLayout(tracksLayout);

    headers_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headers_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout *headersLayout = new QVBoxLayout;
    headersLayout->setContentsMargins(0, m_trackview->frameWidth(), 0, 0);
    headersLayout->setSpacing(0);
    headers_container->setLayout(headersLayout);
    connect(headers_area->verticalScrollBar(), SIGNAL(valueChanged(int)), m_trackview->verticalScrollBar(), SLOT(setValue(int)));

    tracksLayout->addWidget(m_trackview);
    connect(m_trackview->verticalScrollBar(), SIGNAL(valueChanged(int)), headers_area->verticalScrollBar(), SLOT(setValue(int)));
    connect(m_trackview, SIGNAL(tracksChanged()), this, SLOT(slotReloadTracks()));
    connect(m_trackview, SIGNAL(updateTrackHeaders()), this, SLOT(slotRepaintTracks()));
    connect(m_trackview, SIGNAL(showTrackEffects(int,TrackInfo)), this, SIGNAL(showTrackEffects(int,TrackInfo)));
    connect(m_trackview, SIGNAL(updateTrackEffectState(int)), this, SLOT(slotUpdateTrackEffectState(int)));
    transitionHandler = new TransitionHandler(m_tractor);
    connect(transitionHandler, &TransitionHandler::refresh, m_doc->renderer(), &Render::doRefresh);
    connect(m_trackview, SIGNAL(cursorMoved(int,int)), m_ruler, SLOT(slotCursorMoved(int,int)));
    connect(m_trackview, SIGNAL(updateRuler(int)), m_ruler, SLOT(updateRuler(int)), Qt::DirectConnection);

    connect(m_trackview->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_ruler, SLOT(slotMoveRuler(int)));
    connect(m_trackview->horizontalScrollBar(), SIGNAL(rangeChanged(int,int)), this, SLOT(slotUpdateVerticalScroll(int,int)));
    connect(m_trackview, SIGNAL(mousePosition(int)), this, SIGNAL(mousePosition(int)));
    connect(m_doc->renderer(), &Render::previewRender, this, &Timeline::gotPreviewRender);
}

Timeline::~Timeline()
{
    delete m_ruler;
    delete m_trackview;
    delete m_scene;
    delete transitionHandler;
    delete m_tractor;
    qDeleteAll<>(m_tracks);
    m_tracks.clear();
}

void Timeline::loadTimeline()
{
    parseDocument(m_doc->toXml());
    m_trackview->slotUpdateAllThumbs();
    m_trackview->slotSelectTrack(m_trackview->getNextVideoTrack(1));
    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
}

QMap <QString, QString> Timeline::documentProperties()
{
    QMap <QString, QString> props = m_doc->documentProperties();
    props.insert(QStringLiteral("audiotargettrack"), QString::number(audioTarget));
    props.insert(QStringLiteral("videotargettrack"), QString::number(videoTarget));
    return props;
}

Track* Timeline::track(int i) 
{
    if (i < 0 || i >= m_tracks.count()) return NULL;
    return m_tracks.at(i);
}

int Timeline::tracksCount() const
{
    return m_tractor->count() - (m_hasOverlayTrack ? 1 : 0);
}

int Timeline::visibleTracksCount() const
{
    return m_tractor->count() - 1 - (m_hasOverlayTrack ? 1 : 0);
}

//virtual
void Timeline::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Up) {
        m_trackview->slotTrackUp();
        event->accept();
    } else if (event->key() == Qt::Key_Down) {
        m_trackview->slotTrackDown();
        event->accept();
    } else QWidget::keyPressEvent(event);
}

int Timeline::duration() const
{
    return m_trackview->duration();
}

bool Timeline::checkProjectAudio()
{
    bool hasAudio = false;
    int max = m_tracks.count();
    for (int i = 0; i < max; i++) {
        Track *sourceTrack = track(i);
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i + 1));
        int state = track->get_int("hide");
        if (sourceTrack && sourceTrack->hasAudio() && !(state & 2)) {
            hasAudio = true;
            break;
        }
    }
    return hasAudio;
}

int Timeline::inPoint() const
{
    return m_ruler->inPoint();
}

int Timeline::outPoint() const
{
    return m_ruler->outPoint();
}

void Timeline::slotSetZone(const QPoint &p, bool updateDocumentProperties)
{
    m_ruler->setZone(p);
    if (updateDocumentProperties) m_doc->setZone(p.x(), p.y());
}

void Timeline::setDuration(int dur)
{
    m_trackview->setDuration(dur);
    m_ruler->setDuration(dur);
}

int Timeline::getTracks() {
    int duration = 1;
    qDeleteAll<>(m_tracks);
    m_tracks.clear();
    QVBoxLayout *headerLayout = qobject_cast< QVBoxLayout* >(headers_container->layout());
    QLayoutItem *child;
    while ((child = headerLayout->takeAt(0)) != 0) {
        delete child;
    }
    int clipsCount = 0;
    for (int i = 0; i < m_tractor->count(); ++i) {
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        QString playlist_name = track->get("id");
        if (playlist_name == QLatin1String("black_track")) continue;
        clipsCount += track->count();
    }
    emit startLoadingBin(clipsCount);
    emit resetUsageCount();
    checkTrackHeight(false);
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    int headerWidth = 0;
    int offset = 0;
    for (int i = 0; i < m_tractor->count(); ++i) {
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        QString playlist_name = track->get("id");
        if (playlist_name == QLatin1String("playlistmain")) continue;
        bool isBackgroundBlackTrack = playlist_name == QLatin1String("black_track");
        // check track effects
        Mlt::Playlist playlist(*track);
        int trackduration = 0;
        int audio = 0;
        Track *tk = NULL;
        if (!isBackgroundBlackTrack) {
            audio = playlist.get_int("kdenlive:audio_track");
            tk = new Track(i, m_trackActions, playlist, audio == 1 ? AudioTrack : VideoTrack, this);
            m_tracks.append(tk);
            trackduration = loadTrack(i, offset, playlist);
            QFrame *frame = new QFrame(headers_container);
            frame->setFrameStyle(QFrame::HLine);
            frame->setFixedHeight(1);
            headerLayout->insertWidget(0, frame);
        } else {
            // Black track
            tk = new Track(i, m_trackActions, playlist, audio == 1 ? AudioTrack : VideoTrack, this);
            m_tracks.append(tk);
        }
        offset += track->count();
        if (audio == 0 && !isBackgroundBlackTrack) {
            // Check if we have a composite transition for this track
            QScopedPointer<Mlt::Transition> transition(transitionHandler->getTransition(KdenliveSettings::gpu_accel() ? "movit.overlay" : "frei0r.cairoblend", i, -1, true));
            if (!transition) {
                tk->trackHeader->disableComposite();
            }
        }
        if (!isBackgroundBlackTrack) {
            tk->trackHeader->setTrackHeight(height);
            int currentWidth = tk->trackHeader->minimumWidth();
            if (currentWidth > headerWidth) headerWidth = currentWidth;
            headerLayout->insertWidget(0, tk->trackHeader);
            if (trackduration > duration) duration = trackduration;
            tk->trackHeader->setSelectedIndex(m_trackview->selectedTrack());
            connect(tk->trackHeader, &HeaderTrack::switchTrackComposite, this, &Timeline::slotSwitchTrackComposite);
            connect(tk->trackHeader, SIGNAL(switchTrackVideo(int,bool)), m_trackview, SLOT(slotSwitchTrackVideo(int,bool)));
            connect(tk->trackHeader, SIGNAL(switchTrackAudio(int,bool)), m_trackview, SLOT(slotSwitchTrackAudio(int,bool)));
            connect(tk->trackHeader, SIGNAL(switchTrackLock(int,bool)), m_trackview, SLOT(slotSwitchTrackLock(int,bool)));
            connect(tk->trackHeader, SIGNAL(selectTrack(int,bool)), m_trackview, SLOT(slotSelectTrack(int,bool)));
            connect(tk->trackHeader, SIGNAL(renameTrack(int,QString)), this, SLOT(slotRenameTrack(int,QString)));
            connect(tk->trackHeader, SIGNAL(configTrack()), this, SIGNAL(configTrack()));
            connect(tk->trackHeader, SIGNAL(addTrackEffect(QDomElement,int)), m_trackview, SLOT(slotAddTrackEffect(QDomElement,int)));
            if (playlist.filter_count()) {
                getEffects(playlist, NULL, i);
                slotUpdateTrackEffectState(i);
            }
            connect(tk, &Track::newTrackDuration, this, &Timeline::checkDuration, Qt::DirectConnection);
            connect(tk, SIGNAL(storeSlowMotion(QString,Mlt::Producer *)), m_doc->renderer(), SLOT(storeSlowmotionProducer(QString,Mlt::Producer *)));
            connect(tk, &Track::invalidatePreview, this, &Timeline::invalidatePreview);
        }
    }
    headers_container->setFixedWidth(headerWidth);
    if (audioTarget > -1) {
        m_tracks.at(audioTarget)->trackHeader->switchTarget(true);
    }
    if (videoTarget > -1) {
        m_tracks.at(videoTarget)->trackHeader->switchTarget(true);
    }
    updatePalette();
    refreshTrackActions();
    return duration;
}

void Timeline::checkDuration(int duration) {
    Q_UNUSED(duration)
    m_doc->renderer()->mltCheckLength(m_tractor);
    return;
    /*FIXME
    for (int i = 1; i < m_tractor->count(); ++i) {
        QScopedPointer<Mlt::Producer> tk(m_tractor->track(i));
        int len = tk->get_playtime() - 1;
        if (len > duration) duration = len;
    }
    QScopedPointer<Mlt::Producer> tk1(m_tractor->track(0));
    Mlt::Service s(tk1->get_service());
    Mlt::Playlist blackTrack(s);
    if (blackTrack.get_playtime() - 1 != duration) {
        QScopedPointer<Mlt::Producer> blackClip(blackTrack.get_clip(0));
        if (blackClip->parent().get_length() <= duration) {
            blackClip->parent().set("length", duration + 1);
            blackClip->parent().set("out", duration);
            blackClip->set("length", duration + 1);
        }
        blackTrack.resize_clip(0, 0, duration);
    }
    //TODO: rewind consumer if beyond duration / emit durationChanged
    */
}

void Timeline::getTransitions() {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    mlt_service service = mlt_service_get_producer(m_tractor->get_service());
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    while (service) {
        Mlt::Properties prop(MLT_SERVICE_PROPERTIES(service));
        if (QString(prop.get("mlt_type")) != QLatin1String("transition"))
            break;
        //skip automatic mix
        if (prop.get_int("internal_added") == 237) {
            QString trans = prop.get("mlt_service");
            if (trans == QLatin1String("movit.overlay") || trans == QLatin1String("frei0r.cairoblend")) {
                int ix = prop.get_int("b_track");
                if (ix >= 0 && ix < m_tracks.count()) {
                    TrackInfo info = track(ix)->info();
                    info.composite = !prop.get_int("disable");
                    track(ix)->setInfo(info);
                } else qWarning() << "Wrong composite track index: " << ix;
            } else if(trans == QLatin1String("mix")) {
            }
            service = mlt_service_producer(service);
            continue;
        }

        int a_track = prop.get_int("a_track");
        int b_track = prop.get_int("b_track");
        ItemInfo transitionInfo;
        transitionInfo.startPos = GenTime(prop.get_int("in"), m_doc->fps());
        transitionInfo.endPos = GenTime(prop.get_int("out") + 1, m_doc->fps());
        transitionInfo.track = b_track;
        // When adding composite transition, check if it is a wipe transition
        if (prop.get("kdenlive_id") == NULL && QString(prop.get("mlt_service")) == QLatin1String("composite") && isSlide(prop.get("geometry")))
            prop.set("kdenlive_id", "slide");
        QDomElement base = MainWindow::transitions.getEffectByTag(prop.get("mlt_service"), prop.get("kdenlive_id")).cloneNode().toElement();
        //check invalid parameters
        if (a_track > m_tractor->count() - 1) {
            m_documentErrors.append(i18n("Transition %1 had an invalid track: %2 > %3", prop.get("id"), a_track, m_tractor->count() - 1) + '\n');
            prop.set("a_track", m_tractor->count() - 1);
        }
        if (b_track > m_tractor->count() - 1) {
            m_documentErrors.append(i18n("Transition %1 had an invalid track: %2 > %3", prop.get("id"), b_track, m_tractor->count() - 1) + '\n');
            prop.set("b_track", m_tractor->count() - 1);
        }
        if (a_track == b_track || b_track <= 0
            || transitionInfo.startPos >= transitionInfo.endPos
            || base.isNull()
            //|| !m_trackview->canBePastedTo(transitionInfo, TransitionWidget)
           ) {
            // invalid transition, remove it
            m_documentErrors.append(i18n("Removed invalid transition: %1", prop.get("id")) + '\n');
            mlt_service disconnect = service;
            service = mlt_service_producer(service);
            mlt_field_disconnect_service(field->get_field(), disconnect);
        } else {
            QDomNodeList params = base.elementsByTagName(QStringLiteral("parameter"));
            for (int i = 0; i < params.count(); ++i) {
                QDomElement e = params.item(i).toElement();
                QString paramName = e.hasAttribute(QStringLiteral("tag")) ? e.attribute(QStringLiteral("tag")) : e.attribute(QStringLiteral("name"));
                QString value = prop.get(paramName.toUtf8().constData());
                // if transition parameter has an "optional" attribute, it means that it can contain an empty value
                if (value.isEmpty() && !e.hasAttribute(QStringLiteral("optional"))) continue;
                if (e.hasAttribute("factor") || e.hasAttribute("offset"))
                    adjustDouble(e, value);
                else
                    e.setAttribute(QStringLiteral("value"), value);
            }
            Transition *tr = new Transition(transitionInfo, a_track, m_doc->fps(), base,
 QString(prop.get("automatic")) == QLatin1String("1"));
            connect(tr, &AbstractClipItem::selectItem, m_trackview, &CustomTrackView::slotSelectItem);
            tr->setPos(transitionInfo.startPos.frames(m_doc->fps()), KdenliveSettings::trackheight() * (visibleTracksCount() - transitionInfo.track) + 1 + tr->itemOffset());
            if (QString(prop.get("force_track")) == QLatin1String("1")) tr->setForcedTrack(true, a_track);
            if (isTrackLocked(b_track)) tr->setItemLocked(true);
            m_scene->addItem(tr);
            service = mlt_service_producer(service);
        }
    }
}

// static
bool Timeline::isSlide(QString geometry) {
    if (geometry.count(';') != 1) return false;

    geometry.remove(QChar('%'), Qt::CaseInsensitive);
    geometry.replace(QChar('x'), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar(','), QChar(':'), Qt::CaseInsensitive);
    geometry.replace(QChar('/'), QChar(':'), Qt::CaseInsensitive);

    QString start = geometry.section('=', 0, 0).section(':', 0, -2) + ':';
    start.append(geometry.section('=', 1, 1).section(':', 0, -2));
    QStringList numbers = start.split(':', QString::SkipEmptyParts);
    for (int i = 0; i < numbers.size(); ++i) {
        int checkNumber = qAbs(numbers.at(i).toInt());
        if (checkNumber != 0 && checkNumber != 100) {
            return false;
        }
    }
    return true;
}

void Timeline::adjustDouble(QDomElement &e, const QString &value) {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QString factor = e.attribute(QStringLiteral("factor"), QStringLiteral("1"));
    double offset = locale.toDouble(e.attribute(QStringLiteral("offset"), QStringLiteral("0")));
    double fact = 1;
    if (factor.contains('%')) fact = EffectsController::getStringEval(m_doc->getProfileInfo(), factor);
    else fact = locale.toDouble(factor);
    QString type = e.attribute(QStringLiteral("type"));
    if (type == QLatin1String("double") || type == QLatin1String("constant")) {
        double val = locale.toDouble(value);
        e.setAttribute(QStringLiteral("value"), locale.toString(offset + val * fact));
    } else if (type == QLatin1String("simplekeyframe")) {
        QStringList keys = value.split(QLatin1Char(';'));
        for (int j = 0; j < keys.count(); ++j) {
            QString pos = keys.at(j).section(QLatin1Char('='), 0, 0);
            double val = locale.toDouble(keys.at(j).section(QLatin1Char('='), 1, 1)) * fact + offset;
            keys[j] = pos + '=' + locale.toString(val);
        }
        e.setAttribute(QStringLiteral("value"), keys.join(QLatin1Char(';')));
    } else {
        e.setAttribute(QStringLiteral("value"), value);
    }
}

void Timeline::parseDocument(const QDomDocument &doc)
{
    //int cursorPos = 0;
    m_documentErrors.clear();
    m_replacementProducerIds.clear();

    // parse project tracks
    QDomElement mlt = doc.firstChildElement(QStringLiteral("mlt"));
    m_trackview->setDuration(getTracks());
    getTransitions();

    // Rebuild groups
    QDomDocument groupsDoc;
    groupsDoc.setContent(m_doc->renderer()->getBinProperty(QStringLiteral("kdenlive:clipgroups")));
    QDomNodeList groups = groupsDoc.elementsByTagName(QStringLiteral("group"));
    m_trackview->loadGroups(groups);

    // Load custom effects
    QDomDocument effectsDoc;
    effectsDoc.setContent(m_doc->renderer()->getBinProperty(QStringLiteral("kdenlive:customeffects")));
    QDomNodeList effects = effectsDoc.elementsByTagName(QStringLiteral("effect"));
    if (!effects.isEmpty()) {
        m_doc->saveCustomEffects(effects);
    }

    if (!m_documentErrors.isNull()) KMessageBox::sorry(this, m_documentErrors);
    if (mlt.hasAttribute(QStringLiteral("upgraded")) || mlt.hasAttribute(QStringLiteral("modified"))) {
        // Our document was upgraded, create a backup copy just in case
        QString baseFile = m_doc->url().path().section(QStringLiteral(".kdenlive"), 0, 0);
        int ct = 0;
        QString backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        while (QFile::exists(backupFile)) {
            ct++;
            backupFile = baseFile + "_backup" + QString::number(ct) + ".kdenlive";
        }
        QString message;
        if (mlt.hasAttribute(QStringLiteral("upgraded")))
            message = i18n("Your project file was upgraded to the latest Kdenlive document version.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile);
        else
            message = i18n("Your project file was modified by Kdenlive.\nTo make sure you don't lose data, a backup copy called %1 was created.", backupFile);

        KIO::FileCopyJob *copyjob = KIO::file_copy(m_doc->url(), QUrl::fromLocalFile(backupFile));
        if (copyjob->exec())
            KMessageBox::information(this, message);
        else
            KMessageBox::information(this, i18n("Your project file was upgraded to the latest Kdenlive document version, but it was not possible to create the backup copy %1.", backupFile));
    }
}

void Timeline::slotDeleteClip(const QString &clipId, QUndoCommand *deleteCommand)
{
    m_trackview->deleteClip(clipId, deleteCommand);
}

void Timeline::setCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void Timeline::moveCursorPos(int pos)
{
    m_trackview->setCursorPos(pos);
}

void Timeline::slotChangeZoom(int horizontal, int vertical)
{
    m_ruler->setPixelPerMark(horizontal);
    m_scale = (double) m_trackview->getFrameWidth() / m_ruler->comboScale[horizontal];

    if (vertical == -1) {
        // user called zoom
        m_doc->setZoom(horizontal, m_verticalZoom);
        m_trackview->setScale(m_scale, m_scene->scale().y());
    } else {
        m_verticalZoom = vertical;
        if (m_verticalZoom == 0)
            m_trackview->setScale(m_scale, 0.5);
        else
            m_trackview->setScale(m_scale, m_verticalZoom);
        adjustTrackHeaders();
    }
}

int Timeline::fitZoom() const
{
    int zoom = (int)((duration() + 20 / m_scale) * m_trackview->getFrameWidth() / m_trackview->width());
    int i;
    for (i = 0; i < 13; ++i)
        if (m_ruler->comboScale[i] > zoom) break;
    return i;
}

KdenliveDoc *Timeline::document()
{
    return m_doc;
}

void Timeline::refresh()
{
    m_trackview->viewport()->update();
}

void Timeline::slotRepaintTracks()
{
    for (int i = 1; i < m_tracks.count(); i++) {
        m_tracks.at(i)->trackHeader->setSelectedIndex(m_trackview->selectedTrack());
    }
}

void Timeline::blockTrackSignals(bool block)
{
    for (int i = 1; i < m_tracks.count(); i++) {
        m_tracks.at(i)->blockSignals(block);
    }
}

void Timeline::slotReloadTracks()
{
    emit updateTracksInfo();
}

TrackInfo Timeline::getTrackInfo(int ix)
{
    if (ix < 0 || ix > m_tracks.count()) {
	qWarning()<<"/// ARGH, requested info for track: "<<ix<<" - MAX is: "<<m_tracks.count();
	// Let it crash to find wrong calls
	TrackInfo info;
	return info;
    }
    Track *tk = track(ix);
    if (tk == NULL) {
	qWarning()<<"/// ARGH, requesting NULL track: "<<ix<<" - MAX is: "<<m_tracks.count();
	// Let it crash to find wrong calls
	TrackInfo info;
	return info;
    }
    return tk->info();
}

bool Timeline::isLastClip(ItemInfo info)
{
    Track *tk = track(info.track);
    if (tk == NULL) {
        return true;
    }
    return tk->isLastClip(info.endPos.seconds());
}

void Timeline::setTrackInfo(int ix, TrackInfo info)
{
    if (ix < 0 || ix > m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *tk = track(ix);
    tk->setInfo(info);
}


QList <TrackInfo> Timeline::getTracksInfo()
{
    QList <TrackInfo> tracks;
    for (int i = 0; i < tracksCount(); i++) {
        tracks << track(i)->info();
    }
    return tracks;
}

QStringList Timeline::getTrackNames()
{
    QStringList trackNames;
    for (int i = 0; i < tracksCount(); i++) {
        trackNames << track(i)->info().trackName;
    }
    return trackNames;
}

void Timeline::lockTrack(int ix, bool lock)
{
    Track *tk = track(ix);
    if (tk == NULL) {
        qWarning() << "Set Track effect outisde of range: "<<ix;
        return;
    }
    tk->lockTrack(lock);
}

bool Timeline::isTrackLocked(int ix)
{
    Track *tk = track(ix);
    if (tk == NULL) {
        qWarning() << "Set Track effect outisde of range: "<<ix;
        return false;
    }
    int locked = tk->getIntProperty(QStringLiteral("kdenlive:locked_track"));
    return locked == 1;
}

void Timeline::updateTrackState(int ix, int state)
{
    int currentState = 0;
    QScopedPointer<Mlt::Producer> track(m_tractor->track(ix));
    currentState = track->get_int("hide");
    if (state == currentState) return;
    if (state == 0) {
        // Show all
        if (currentState & 1) {
            switchTrackVideo(ix, false);
        }
        if (currentState & 2) {
            switchTrackAudio(ix, false);
        }
    }
    else if (state == 1) {
        // Mute video
        if (currentState & 2) {
            switchTrackAudio(ix, false);
        }
        switchTrackVideo(ix, true);
    }
    else if (state == 2) {
        // Mute audio
        if (currentState & 1) {
            switchTrackVideo(ix, false);
        }
        switchTrackAudio(ix, true);
    }
    else {
        switchTrackVideo(ix, true);
        switchTrackAudio(ix, true);
    }
}

void Timeline::switchTrackVideo(int ix, bool hide)
{
    Track* tk = track(ix);
    if (tk == NULL) {
        qWarning() << "Set Track effect outisde of range: "<<ix;
        return;
    }
    int state = tk->state();
    if (hide && (state & 1)) {
        // Video is already muted
        return;
    }
    int newstate = 0;
    if (hide) {
        if (state & 2) {
            newstate = 3;
        }
        else {
            newstate = 1;
        }
    }
    else {
        if (state & 2) {
            newstate = 2;
        }
        else {
            newstate = 0;
        }
    }
    tk->setState(newstate);
    refreshTractor();
}

void Timeline::slotSwitchTrackComposite(int trackIndex, bool enable)
{
    if (trackIndex < 1 || trackIndex > m_tracks.count()) return;
    QScopedPointer<Mlt::Transition> transition(transitionHandler->getTransition(KdenliveSettings::gpu_accel() ? "movit.overlay" : "frei0r.cairoblend", trackIndex, -1, true));
    if (transition) {
        transition->set("disable", enable);
        // When turning a track composite on/off, we need to re-plug transitions correctly
        updateComposites();
        m_doc->renderer()->doRefresh();
        m_doc->setModified();
        //TODO: create undo/redo command for this
    } else {
        Track* tk = track(trackIndex);
        tk->trackHeader->setComposite(false);
        qWarning() << "Composite transition not found";
    }
}

void Timeline::updateComposites()
{
    int lowest = getLowestVideoTrack();
    if (lowest >= 0) {
        transitionHandler->rebuildComposites(lowest);
    }
}

void Timeline::refreshTractor()
{
    m_tractor->multitrack()->refresh();
    m_tractor->refresh();
}

void Timeline::switchTrackAudio(int ix, bool mute)
{
    Track* tk = track(ix);
    if (tk == NULL) {
        qWarning() << "Set Track effect outisde of range: "<<ix;
        return;
    }
    int state = tk->state();
    if (mute && (state & 2)) {
        // audio is already muted
        return;
    }
    if (mute && state < 2 ) {
        // We mute a track with sound
        /*if (ix == getLowestNonMutedAudioTrack())*/
    }
    else if (!mute && state > 1 ) {
        // We un-mute a previously muted track
        /*if (ix < getLowestNonMutedAudioTrack())*/
    }
    int newstate;
    if (mute) {
        if (state & 1) newstate = 3;
        else newstate = 2;
    } else if (state & 1) {
        newstate = 1;
    } else {
        newstate = 0;
    }
    tk->setState(newstate);
    //if (audioMixingBroken) fixAudioMixing();
    m_tractor->multitrack()->refresh();
    m_tractor->refresh();
}

int Timeline::getLowestVideoTrack()
{
    for (int i = 1; i < m_tractor->count(); ++i) {
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        Mlt::Playlist playlist(*track);
        if (playlist.get_int("kdenlive:audio_track") != 1)
            return i;
    }
    return -1;
}

void Timeline::fixAudioMixing()
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    QString resource = mlt_properties_get(properties, "mlt_service");
    // Delete all audio mixing transitions
    while (mlt_type == QLatin1String("transition")) {
        if (resource == QLatin1String("mix")) {
            Mlt::Transition transition((mlt_transition) nextservice);
            nextservice = mlt_service_producer(nextservice);
            field->disconnect_service(transition);
        }
        else nextservice = mlt_service_producer(nextservice);
        if (nextservice == NULL) break;
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_properties_get(properties, "mlt_type");
        resource = mlt_properties_get(properties, "mlt_service");
    }

    // Re-add correct audio transitions
    for (int i = 1; i < m_tractor->count(); i++) {
        //bool muted = getTrackInfo(i).isMute;
        //if (muted) continue;
        /*int a_track = qMax(lowestTrack, i - 1);
        bool a_muted = getTrackInfo(a_track).isMute;
        while (a_muted && a_track > lowestTrack) {
            a_track = qMax(lowestTrack, a_track - 1);
            a_muted = getTrackInfo(a_track).isMute;
        }
        if (a_muted) continue;*/
        Mlt::Transition *transition = new Mlt::Transition(*m_tractor->profile(), "mix");
        transition->set("always_active", 1);
        transition->set("combine", 1);
        transition->set("a_track", 0);
        transition->set("b_track", i);
        transition->set("internal_added", 237);
        field->plant_transition(*transition, 0, i);
    }
    field->unlock();
}

void Timeline::updatePalette()
{
    headers_container->setStyleSheet(QLatin1String(""));
    setPalette(qApp->palette());
    QPalette p = qApp->palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor col = scheme.background().color();
    QColor col2 = scheme.foreground().color();
    headers_container->setStyleSheet(QStringLiteral("QLineEdit { background-color: transparent;color: %1;} QLineEdit:hover{ background-color: %2;} QLineEdit:focus { background-color: %2;} ").arg(col2.name(), col.name()));
    m_trackview->updatePalette();
    if (!m_tracks.isEmpty()) {
        int ix = m_trackview->selectedTrack();
        for (int i = 0; i < m_tracks.count(); i++) {
            if (m_tracks.at(i)->trackHeader) {
                m_tracks.at(i)->trackHeader->refreshPalette();
                if (i == ix)
                    m_tracks.at(ix)->trackHeader->setSelectedIndex(ix);
            }
        }
    }
    m_ruler->activateZone();
}

void Timeline::updateHeaders()
{
    if (!m_tracks.isEmpty()) {
        for (int i = 0; i < m_tracks.count(); i++) {
            if (m_tracks.at(i)->trackHeader) {
                m_tracks.at(i)->trackHeader->updateLed();
            }
        }
    }
}

void Timeline::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
    QList<KDualAction *> allButtons = this->findChildren<KDualAction *>();
    for (int i = 0; i < allButtons.count(); i++) {
        KDualAction *m = allButtons.at(i);
        QIcon ic = m->activeIcon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setActiveIcon(newIcon);
        ic = m->inactiveIcon();
        if (ic.isNull() || ic.name().isEmpty()) continue;
        newIcon = KoIconUtils::themedIcon(ic.name());
        m->setInactiveIcon(newIcon);
    }
}

void Timeline::adjustTrackHeaders()
{
    if (m_tracks.isEmpty()) return;
    int height = KdenliveSettings::trackheight() * m_scene->scale().y() - 1;
    for (int i = 1; i < m_tracks.count(); i++) {
        m_tracks.at(i)->trackHeader->adjustSize(height);
    }
}

void Timeline::reloadTrack(int ix, int start, int end)
{
    // Get playlist
    Mlt::Playlist pl = m_tracks.at(ix)->playlist();
    if (end == -1) 
        end = pl.get_length();
    // Remove current clips
    int startIndex = pl.get_clip_index_at(start);
    int endIndex = pl.get_clip_index_at(end);
    double startY = m_trackview->getPositionFromTrack(ix) + 2;
    QRectF r(start, startY, end - start, 2);
    QList<QGraphicsItem *> selection = m_scene->items(r);
    QList<QGraphicsItem *> toDelete;
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWidget)
            toDelete << selection.at(i);
    }
    qDeleteAll(toDelete);
    // Reload items
    loadTrack(ix, 0, pl, startIndex, endIndex, false);
}

int Timeline::loadTrack(int ix, int offset, Mlt::Playlist &playlist, int start, int end, bool updateReferences) {
    // parse track
    Mlt::ClipInfo *info = new Mlt::ClipInfo();
    double fps = m_doc->fps();
    if (end == -1)
        end = playlist.count();
    bool locked = playlist.get_int("kdenlive:locked_track") == 1;
    for(int i = start; i < end; ++i) {
        emit loadingBin(offset + i + 1);
        if (playlist.is_blank(i)) {
            continue;
        }
        playlist.clip_info(i, info);
        Mlt::Producer *clip = info->cut;
        // Found a clip
        int in = info->frame_in;
        int out = info->frame_out;
        QString idString = info->producer->get("id");
        if (in > out || m_invalidProducers.contains(idString)) {
            QString trackName = playlist.get("kdenlive:track_name");
            m_documentErrors.append(i18n("Invalid clip removed from track %1 at %2\n", trackName.isEmpty() ? QString::number(ix) : trackName, info->start));
            playlist.remove(i);
            --i;
            continue;
        }
        QString id = idString;
        Track::SlowmoInfo slowInfo;
        slowInfo.speed = 1.0;
        slowInfo.strobe = 1;
        slowInfo.state = PlaylistState::Original;
        bool hasSpeedEffect = false;
        if (idString.endsWith(QLatin1String("_video"))) {
            // Video only producer, store it in BinController
            m_doc->renderer()->loadExtraProducer(idString, new Mlt::Producer(clip->parent()));
        }
        if (idString.startsWith(QLatin1String("slowmotion"))) {
	    hasSpeedEffect = true;
            QLocale locale;
            locale.setNumberOptions(QLocale::OmitGroupSeparator);
            id = idString.section(':', 1, 1);
            slowInfo.speed = locale.toDouble(idString.section(':', 2, 2));
            slowInfo.strobe = idString.section(':', 3, 3).toInt();
            if (slowInfo.strobe == 0) slowInfo.strobe = 1;
            slowInfo.state = (PlaylistState::ClipState) idString.section(':', 4, 4).toInt();
	    // Slowmotion producer, store it for reuse
            Mlt::Producer *parentProd = new Mlt::Producer(clip->parent());
            QString url = parentProd->get("warp_resource");
            if (!m_doc->renderer()->storeSlowmotionProducer(slowInfo.toString(locale) + url, parentProd)) {
                delete parentProd;
            }
        }
        id = id.section('_', 0, 0);
	int length = out - in + 1;
        ProjectClip *binclip = m_doc->getBinClip(id);
        PlaylistState::ClipState originalState = PlaylistState::Original;
        if (binclip == NULL) {
            // Is this a disabled clip
            id = info->producer->get("kdenlive:binid");
            binclip = m_doc->getBinClip(id);
            originalState = (PlaylistState::ClipState) info->producer->get_int("kdenlive:clipstate");
        }
        if (binclip == NULL) {
	    // Warning, unknown clip found, timeline corruption!!
	    //TODO: fix this
            qDebug()<<"* * * * *UNKNOWN CLIP, WE ARE DEAD: "<<id;
	    continue;
	}
	if (updateReferences)
            binclip->addRef();
        ItemInfo clipinfo;
        clipinfo.startPos = GenTime(info->start, fps);
        clipinfo.endPos = GenTime(info->start + length, fps);
        clipinfo.cropStart = GenTime(in, fps);
        clipinfo.cropDuration = GenTime(length, fps);
        clipinfo.track = ix;
	//qDebug()<<"// Loading clip: "<<idString<<" / SPEED: "<<speed<<"\n++++++++++++++++++++++++";
        ClipItem *item = new ClipItem(binclip, clipinfo, fps, slowInfo.speed, slowInfo.strobe, m_trackview->getFrameWidth(), true);
        connect(item, &AbstractClipItem::selectItem, m_trackview, &CustomTrackView::slotSelectItem);
        item->setPos(clipinfo.startPos.frames(fps), KdenliveSettings::trackheight() * (visibleTracksCount() - clipinfo.track) + 1 + item->itemOffset());
        //qDebug()<<" * * Loaded clip on tk: "<<clipinfo.track<< ", POS: "<<clipinfo.startPos.frames(fps);
        item->updateState(idString, info->producer->get_int("audio_index"), info->producer->get_int("video_index"), originalState);
        m_scene->addItem(item);
        if (locked) item->setItemLocked(true);
        if (hasSpeedEffect) {
            QDomElement speedeffect = MainWindow::videoEffects.getEffectByTag(QString(), QStringLiteral("speed")).cloneNode().toElement();
            EffectsList::setParameter(speedeffect, QStringLiteral("speed"), QString::number((int)(100 * slowInfo.speed + 0.5)));
            EffectsList::setParameter(speedeffect, QStringLiteral("strobe"), QString::number(slowInfo.strobe));
            item->addEffect(m_doc->getProfileInfo(), speedeffect, false);
        }
        // parse clip effects
        getEffects(*clip, item);
    }
    delete info;
    return playlist.get_length();
}

void Timeline::loadGuides(QMap <double, QString> guidesData)
{
    QMapIterator<double, QString> i(guidesData);
    while (i.hasNext()) {
        i.next();
        const GenTime pos = GenTime(i.key());
        m_trackview->addGuide(pos, i.value(), true);
    }
}

void Timeline::getEffects(Mlt::Service &service, ClipItem *clip, int track) {
    int effectNb = clip == NULL ? 0 : clip->effectsCount();
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> effect(service.filter(ix));
        QDomElement clipeffect = getEffectByTag(effect->get("tag"), effect->get("kdenlive_id"));
        if (clipeffect.isNull()) {
            m_documentErrors.append(i18n("Effect %1:%2 not found in MLT, it was removed from this project\n", effect->get("tag"), effect->get("kdenlive_id")));
            service.detach(*effect);
            --ix;
            continue;
        }
        effectNb++;
        QDomElement currenteffect = clipeffect.cloneNode().toElement();
        currenteffect.setAttribute(QStringLiteral("kdenlive_ix"), QString::number(effectNb));
        currenteffect.setAttribute(QStringLiteral("kdenlive_info"), effect->get("kdenlive_info"));
        currenteffect.setAttribute(QStringLiteral("disable"), effect->get("disable"));
        QDomNodeList clipeffectparams = currenteffect.childNodes();

        QDomNodeList params = currenteffect.elementsByTagName(QStringLiteral("parameter"));
	ProfileInfo info = m_doc->getProfileInfo();
        for (int i = 0; i < params.count(); ++i) {
            QDomElement e = params.item(i).toElement();
            if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")) e.setAttribute(QStringLiteral("keyframes"), getKeyframes(service, ix, e));
            else setParam(info, e, effect->get(e.attribute(QStringLiteral("name")).toUtf8().constData()));
        }

        if (effect->get_out()) { // no keyframes but in/out points
            //EffectsList::setParameter(currenteffect, QStringLiteral("in"),  effect->get("in"));
            //EffectsList::setParameter(currenteffect, QStringLiteral("out"),  effect->get("out"));
            currenteffect.setAttribute(QStringLiteral("in"), effect->get_in());
            currenteffect.setAttribute(QStringLiteral("out"), effect->get_out());
        }
        QString sync = effect->get("kdenlive:sync_in_out");
        if (!sync.isEmpty()) {
            currenteffect.setAttribute(QStringLiteral("kdenlive:sync_in_out"), sync);
        }
        if (QString(effect->get("tag")) == QLatin1String("region")) getSubfilters(effect.data(), currenteffect);

        if (clip) {
            clip->addEffect(m_doc->getProfileInfo(), currenteffect, false);
        } else {
            addTrackEffect(track, currenteffect);
        }
    }
}

QString Timeline::getKeyframes(Mlt::Service service, int &ix, QDomElement e) {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QString starttag = e.attribute(QStringLiteral("starttag"), QStringLiteral("start"));
    QString endtag = e.attribute(QStringLiteral("endtag"), QStringLiteral("end"));
    double fact, offset = locale.toDouble(e.attribute(QStringLiteral("offset"), QStringLiteral("0")));
    QString factor = e.attribute(QStringLiteral("factor"), QStringLiteral("1"));
    if (factor.contains('%')) fact = EffectsController::getStringEval(m_doc->getProfileInfo(), factor);
    else fact = locale.toDouble(factor);
    // retrieve keyframes
    QScopedPointer<Mlt::Filter> effect(service.filter(ix));
    int effectNb = effect->get_int("kdenlive_ix");
    QString keyframes = QString::number(effect->get_in()) + '=' + locale.toString(offset + fact * effect->get_double(starttag.toUtf8().constData())) + ';';
    for (;ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> eff2(service.filter(ix));
        if (eff2->get_int("kdenlive_ix") != effectNb) break;
        if (eff2->get_in() < eff2->get_out()) {
            keyframes.append(QString::number(eff2->get_out()) + '=' + locale.toString(offset + fact * eff2->get_double(endtag.toUtf8().constData())) + ';');
        }
    }
    --ix;
    return keyframes;
}

void Timeline::getSubfilters(Mlt::Filter *effect, QDomElement &currenteffect) {
    for (int i = 0; ; ++i) {
        QString name = "filter" + QString::number(i);
        if (!effect->get(name.toUtf8().constData())) break;
        //identify effect
        QString tag = effect->get(name.append(".tag").toUtf8().constData());
        QString id = effect->get(name.append(".kdenlive_id").toUtf8().constData());
        QDomElement subclipeffect = getEffectByTag(tag, id);
        if (subclipeffect.isNull()) {
            qWarning() << "Region sub-effect not found";
            continue;
        }
        //load effect
        subclipeffect = subclipeffect.cloneNode().toElement();
        subclipeffect.setAttribute(QStringLiteral("region_ix"), i);
        //get effect parameters (prefixed by subfilter name)
        QDomNodeList params = subclipeffect.elementsByTagName(QStringLiteral("parameter"));
	ProfileInfo info = m_doc->getProfileInfo();
        for (int i = 0; i < params.count(); ++i) {
            QDomElement param = params.item(i).toElement();
            setParam(info, param, effect->get((name + "." + param.attribute(QStringLiteral("name"))).toUtf8().constData()));
        }
        currenteffect.appendChild(currenteffect.ownerDocument().importNode(subclipeffect, true));
    }
}

//static
void Timeline::setParam(ProfileInfo info, QDomElement param, QString value) {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    //get Kdenlive scaling parameters
    double offset = locale.toDouble(param.attribute(QStringLiteral("offset"), QStringLiteral("0")));
    double fact;
    QString factor = param.attribute(QStringLiteral("factor"), QStringLiteral("1"));
    if (factor.contains('%')) {
        fact = EffectsController::getStringEval(info, factor);
    } else {
        fact = locale.toDouble(factor);
    }
    //adjust parameter if necessary
    QString type = param.attribute(QStringLiteral("type"));
    if (type == QLatin1String("simplekeyframe")) {
        QStringList kfrs = value.split(';');
        for (int l = 0; l < kfrs.count(); ++l) {
            QString fr = kfrs.at(l).section('=', 0, 0);
            double val = locale.toDouble(kfrs.at(l).section('=', 1, 1));
            if (fact != 1) {
                // Add 0.5 since we are converting to integer below so that 0.8 is converted to 1 and not 0
                val = val * fact + 0.5;
            }
            kfrs[l] = fr + '=' + QString::number((int) (val + offset));
        }
        param.setAttribute(QStringLiteral("keyframes"), kfrs.join(QStringLiteral(";")));
    } else if (type == QLatin1String("double") || type == QLatin1String("constant")) {
        param.setAttribute(QStringLiteral("value"), locale.toDouble(value) * fact + offset);
    } else {
        param.setAttribute(QStringLiteral("value"), value);
    }
}

QDomElement Timeline::getEffectByTag(const QString &effecttag, const QString &effectid)
{
    QDomElement clipeffect = MainWindow::customEffects.getEffectByTag(QString(), effectid);
    if (clipeffect.isNull()) {
        clipeffect = MainWindow::videoEffects.getEffectByTag(effecttag, effectid);
    }
    if (clipeffect.isNull()) {
        clipeffect = MainWindow::audioEffects.getEffectByTag(effecttag, effectid);
    }
    return clipeffect;
}


QGraphicsScene *Timeline::projectScene()
{
    return m_scene;
}

CustomTrackView *Timeline::projectView()
{
    return m_trackview;
}

void Timeline::setEditMode(const QString & editMode)
{
    m_editMode = editMode;
}

const QString & Timeline::editMode() const
{
    return m_editMode;
}

void Timeline::slotVerticalZoomDown()
{
    if (m_verticalZoom == 0) return;
    m_verticalZoom--;
    m_doc->setZoom(m_doc->zoom().x(), m_verticalZoom);
    if (m_verticalZoom == 0)
        m_trackview->setScale(m_scene->scale().x(), 0.5);
    else
        m_trackview->setScale(m_scene->scale().x(), 1);
    adjustTrackHeaders();
    m_trackview->verticalScrollBar()->setValue(headers_area->verticalScrollBar()->value());
}

void Timeline::slotVerticalZoomUp()
{
    if (m_verticalZoom == 2) return;
    m_verticalZoom++;
    m_doc->setZoom(m_doc->zoom().x(), m_verticalZoom);
    if (m_verticalZoom == 2)
        m_trackview->setScale(m_scene->scale().x(), 2);
    else
        m_trackview->setScale(m_scene->scale().x(), 1);
    adjustTrackHeaders();
    m_trackview->verticalScrollBar()->setValue(headers_area->verticalScrollBar()->value());
}

void Timeline::slotRenameTrack(int ix, const QString &name)
{
    QString currentName = track(ix)->getProperty(QStringLiteral("kdenlive:track_name"));
    if (currentName == name) return;
    ConfigTracksCommand *configTracks = new ConfigTracksCommand(this, ix, currentName, name);
    m_doc->commandStack()->push(configTracks);
}

void Timeline::renameTrack(int ix, const QString &name)
{
    if (ix < 1) return;
    Track *tk = track(ix);
    if (!tk) return;
    tk->setProperty(QStringLiteral("kdenlive:track_name"), name);
    tk->trackHeader->renameTrack(name);
    slotReloadTracks();
}

void Timeline::slotUpdateVerticalScroll(int /*min*/, int max)
{
    int height = 0;
    if (max > 0) height = m_trackview->horizontalScrollBar()->height() - 1;
    headers_container->layout()->setContentsMargins(0, m_trackview->frameWidth(), 0, height);
}

void Timeline::updateRuler()
{
    m_ruler->update();
}

void Timeline::slotShowTrackEffects(int ix)
{
    m_trackview->clearSelection();
    emit showTrackEffects(ix, getTrackInfo(ix));
}

void Timeline::slotUpdateTrackEffectState(int ix)
{
    if (ix < 1) return;
    Track *tk = track(ix);
    if (!tk) return;
    tk->trackHeader->updateEffectLabel(tk->effectsList.effectNames());
}

void Timeline::slotSaveTimelinePreview(const QString &path)
{
    QImage img(width(), height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(palette().base().color().rgb());
    QPainter painter(&img);
    render(&painter);
    painter.end();
    img = img.scaledToWidth(600, Qt::SmoothTransformation);
    img.save(path);
}

void Timeline::updateProfile(bool fpsChanged)
{
    m_ruler->updateFrameSize();
    m_ruler->updateProjectFps(m_doc->timecode());
    m_ruler->setPixelPerMark(m_doc->zoom().x(), true);
    slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
    slotSetZone(m_doc->zone(), false);
    m_trackview->updateSceneFrameWidth(fpsChanged);
}

void Timeline::checkTrackHeight(bool force)
{
    if (m_trackview->checkTrackHeight(force)) {
        m_doc->clipManager()->clearCache();
        m_ruler->updateFrameSize();
        m_trackview->updateSceneFrameWidth();
        slotChangeZoom(m_doc->zoom().x(), m_doc->zoom().y());
        slotSetZone(m_doc->zone(), false);
    }
}

bool Timeline::moveClip(int startTrack, qreal startPos, int endTrack, qreal endPos, PlaylistState::ClipState state, TimelineMode::EditMode mode, bool duplicate)
{
    if (startTrack == endTrack) {
        return track(startTrack)->move(startPos, endPos, mode);
    }
    Track *sourceTrack = track(startTrack);
    int pos = sourceTrack->frame(startPos);
    int clipIndex = sourceTrack->playlist().get_clip_index_at(pos);
    sourceTrack->playlist().lock();
    Mlt::Producer *clipProducer = sourceTrack->playlist().replace_with_blank(clipIndex);
    sourceTrack->playlist().consolidate_blanks();
    if (!clipProducer || clipProducer->is_blank()) {
        qDebug() << "// Cannot get clip at index: "<<clipIndex<<" / "<< startPos;
        sourceTrack->playlist().unlock();
        return false;
    }
    sourceTrack->playlist().unlock();
    Track *destTrack = track(endTrack);
    bool success = destTrack->add(endPos, clipProducer, GenTime(clipProducer->get_in(), destTrack->fps()).seconds(), GenTime(clipProducer->get_out() + 1, destTrack->fps()).seconds(), state, duplicate, mode);
    delete clipProducer;
    return success;
}

void Timeline::addTrackEffect(int trackIndex, QDomElement effect)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    effect.setAttribute(QStringLiteral("kdenlive_ix"), sourceTrack->effectsList.count() + 1);

    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();

        // Check if this effect has a variable parameter
        if (e.attribute(QStringLiteral("default")).contains('%')) {
            double evaluatedValue = EffectsController::getStringEval(m_doc->getProfileInfo(), e.attribute(QStringLiteral("default")));
            e.setAttribute(QStringLiteral("default"), evaluatedValue);
            if (e.hasAttribute(QStringLiteral("value")) && e.attribute(QStringLiteral("value")).startsWith('%')) {
                e.setAttribute(QStringLiteral("value"), evaluatedValue);
            }
        }

        if (!e.isNull() && (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe") || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe"))) {
            QString def = e.attribute(QStringLiteral("default"));
            // Effect has a keyframe type parameter, we need to set the values
            if (e.attribute(QStringLiteral("keyframes")).isEmpty()) {
                e.setAttribute(QStringLiteral("keyframes"), "0:" + def + ';');
                //qDebug() << "///// EFFECT KEYFRAMES INITED: " << e.attribute("keyframes");
                //break;
            }
        }

        if (effect.attribute(QStringLiteral("id")) == QLatin1String("crop")) {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute(QStringLiteral("name")) == QLatin1String("use_profile") && m_doc->useProxy())
                e.setAttribute(QStringLiteral("value"), QStringLiteral("1"));
        }
    }
    sourceTrack->effectsList.append(effect);
    sourceTrack->addTrackEffect(EffectsController::getEffectArgs(m_doc->getProfileInfo(), effect));
}

bool Timeline::removeTrackEffect(int trackIndex, int effectIndex, const QDomElement &effect)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return false;
    }
    int toRemove = effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    Track *sourceTrack = track(trackIndex);
    bool success = sourceTrack->removeTrackEffect(effectIndex, true);
    if (success) {
        int max = sourceTrack->effectsList.count();
        for (int i = 0; i < max; ++i) {
            int index = sourceTrack->effectsList.at(i).attribute(QStringLiteral("kdenlive_ix")).toInt();
            if (toRemove == index) {
                sourceTrack->effectsList.removeAt(toRemove);
                break;
            }
        }
    }
    return success;
}

void Timeline::setTrackEffect(int trackIndex, int effectIndex, QDomElement effect)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    int max = sourceTrack->effectsList.count();
    if (effectIndex <= 0 || effectIndex > (max) || effect.isNull()) {
        //qDebug() << "Invalid effect index: " << effectIndex;
        return;
    }
    sourceTrack->effectsList.removeAt(effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
    effect.setAttribute(QStringLiteral("kdenlive_ix"), effectIndex);
    sourceTrack->effectsList.insert(effect);
}

void Timeline::enableTrackEffects(int trackIndex, const QList <int> &effectIndexes, bool disable)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return;
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    QDomElement effect;
    for (int i = 0; i < effectIndexes.count(); ++i) {
        effect = list.itemFromIndex(effectIndexes.at(i));
        if (!effect.isNull()) effect.setAttribute(QStringLiteral("disable"), (int) disable);
    }
}

const EffectsList Timeline::getTrackEffects(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return EffectsList();
    }
    Track *sourceTrack = track(trackIndex);
    return sourceTrack->effectsList;
}

QDomElement Timeline::getTrackEffect(int trackIndex, int effectIndex)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return QDomElement();
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    if (effectIndex > list.count() || effectIndex < 1 || list.itemFromIndex(effectIndex).isNull()) return QDomElement();
    return list.itemFromIndex(effectIndex).cloneNode().toElement();
}

int Timeline::hasTrackEffect(int trackIndex, const QString &tag, const QString &id) 
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return -1;
    }
    Track *sourceTrack = track(trackIndex);
    EffectsList list = sourceTrack->effectsList;
    return list.hasEffect(tag, id);
}

MltVideoProfile Timeline::mltProfile() const
{
    return ProfilesDialog::getVideoProfile(*m_tractor->profile());
}

double Timeline::fps() const
{
    return m_doc->fps();
}


QPoint Timeline::getTracksCount()
{
    int audioTracks = 0;
    int videoTracks = 0;
    int max = m_tracks.count();
    for (int i = 0; i < max; i++) {
        Track *tk = track(i);
        if (tk->type == AudioTrack) audioTracks++;
        else videoTracks++;
    }
    return QPoint(videoTracks, audioTracks);
}

int Timeline::getTrackSpaceLength(int trackIndex, int pos, bool fromBlankStart)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.count()) {
        qWarning() << "Set Track effect outisde of range";
        return 0;
    }
    return track(trackIndex)->getBlankLength(pos, fromBlankStart);
}

void Timeline::updateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    for (int i = 0; i< m_tracks.count(); i++) {
        track(i)->updateClipProperties(id, properties);
    }
}

int Timeline::changeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, PlaylistState::ClipState state, double speed, int strobe, Mlt::Producer *originalProd, bool removeEffect)
{
    QLocale locale;
    QString url = QString::fromUtf8(originalProd->get("resource"));
    Track::SlowmoInfo slowInfo;
    slowInfo.speed = speed;
    slowInfo.strobe = strobe;
    slowInfo.state = state;
    url.prepend(slowInfo.toString(locale));
    //if (strobe > 1) url.append("&strobe=" + QString::number(strobe));
    Mlt::Producer *prod;
    if (removeEffect) {
        // We want to remove framebuffer producer, so pass original
        prod = originalProd;
    } else {
        // Pass slowmotion producer
        prod = m_doc->renderer()->getSlowmotionProducer(url);
    }
    QString id = originalProd->get("id");
    id = id.section(QStringLiteral("_"), 0,  0);
    Mlt::Properties passProperties;
    Mlt::Properties original(originalProd->get_properties());
    passProperties.pass_list(original, ClipController::getPassPropertiesList(false));
    return track(info.track)->changeClipSpeed(info, speedIndependantInfo, state, speed, strobe, prod, id, passProperties);
}

void Timeline::duplicateClipOnPlaylist(int tk, qreal startPos, int offset, Mlt::Producer *prod)
{
    Track *sourceTrack = track(tk);
    int pos = sourceTrack->frame(startPos);
    int clipIndex = sourceTrack->playlist().get_clip_index_at(pos);
    if (sourceTrack->playlist().is_blank(clipIndex)) {
	  qDebug()<<"// ERROR FINDING CLIP on TK: "<<tk<<", FRM: "<<pos;
    }
    Mlt::Producer *clipProducer = sourceTrack->playlist().get_clip(clipIndex);
    Clip clp(clipProducer->parent());
    Mlt::Producer *cln = clp.clone();

    // Clip effects must be moved from clip to the playlist entry, so first delete them from parent clip
    Clip(*cln).deleteEffects();
    cln->set_in_and_out(clipProducer->get_in(), clipProducer->get_out());
    Mlt::Playlist trackPlaylist((mlt_playlist) prod->get_service());
    trackPlaylist.lock();

    int newIdx = trackPlaylist.insert_at(pos - offset, cln, 1);
    // Re-add source effects in playlist
    Mlt::Producer *inPlaylist = trackPlaylist.get_clip(newIdx);
    if (inPlaylist) {
        Clip(*inPlaylist).addEffects(*clipProducer);
        delete inPlaylist;
    }

    trackPlaylist.unlock();
    delete clipProducer;
    delete cln;
    delete prod;
}

int Timeline::getSpaceLength(const GenTime &pos, int tk, bool fromBlankStart)
{
    Track *sourceTrack = track(tk);
    if (!sourceTrack) return 0;
    int insertPos = pos.frames(m_doc->fps());
    return sourceTrack->spaceLength(insertPos, fromBlankStart);
}

void Timeline::disableTimelineEffects(bool disable)
{
    for (int i = 0; i< tracksCount(); i++) {
        track(i)->disableEffects(disable);
    }
}

void Timeline::importPlaylist(ItemInfo info, QMap <QString, QString> processedUrl, QMap <QString, QString> idMaps, QDomDocument doc, QUndoCommand *command)
{
    projectView()->importPlaylist(info, processedUrl, idMaps, doc, command);
}

void Timeline::refreshTrackActions()
{
    int tracks = tracksCount();
    if (tracks > 3) {
        return;
    }
    foreach(QAction *action, m_trackActions) {
        if (action->data().toString() == "delete_track") {
            action->setEnabled(tracks > 2);
        }
    }
}

void Timeline::slotMultitrackView(bool enable)
{
    multitrackView = enable;
    transitionHandler->enableMultiTrack(enable);
}

void Timeline::connectOverlayTrack(bool enable)
{
    if (!m_hasOverlayTrack) return;
    m_tractor->lock();
    if (enable) {
        // Re-add overlaytrack
        m_tractor->insert_track(*m_overlayTrack, tracksCount() + 1);
        delete m_overlayTrack;
        m_overlayTrack = NULL;
    } else {
        m_overlayTrack = m_tractor->track(tracksCount());
        m_tractor->remove_track(tracksCount());
    }
    m_tractor->unlock();
}

void Timeline::removeSplitOverlay()
{
    if (!m_hasOverlayTrack) return;
    m_tractor->lock();
    m_tractor->remove_track(tracksCount());
    m_hasOverlayTrack = false;
    m_tractor->unlock();
}

bool Timeline::createOverlay(Mlt::Filter *filter, int tk, int startPos)
{
    Track *sourceTrack = track(tk);
    if (!sourceTrack) return false;
    m_tractor->lock();
    int clipIndex = sourceTrack->playlist().get_clip_index_at(startPos);
    Mlt::Producer *clipProducer = sourceTrack->playlist().get_clip(clipIndex);
    Clip clp(clipProducer->parent());
    Mlt::Producer *cln = clp.clone();
    Clip(*cln).deleteEffects();
    cln->set_in_and_out(clipProducer->get_in(), clipProducer->get_out());
    Mlt::Playlist overlay(*m_tractor->profile());
    Mlt::Tractor trac(*m_tractor->profile());
    trac.set_track(*clipProducer, 0);
    trac.set_track(*cln, 1);
    cln->attach(*filter);
    QString splitTransition = KdenliveSettings::gpu_accel() ? "movit.overlay" : "frei0r.cairoblend";
    Mlt::Transition t(*m_tractor->profile(), splitTransition.toUtf8().constData());
    t.set("always_active", 1);
    trac.plant_transition(t, 0, 1);
    delete cln;
    delete clipProducer;
    overlay.insert_blank(0, startPos);
    Mlt::Producer split(trac.get_producer());
    overlay.insert_at(startPos, &split, 1);
    int trackIndex = tracksCount();
    m_tractor->insert_track(overlay, trackIndex);
    Mlt::Producer *overlayTrack = m_tractor->track(trackIndex);
    overlayTrack->set("hide", 2);
    delete overlayTrack;
    m_hasOverlayTrack = true;
    m_tractor->unlock();
    return true;
}


void Timeline::switchTrackTarget()
{
    if (!KdenliveSettings::splitaudio()) {
        // This feature is only available on split mode
        return;
    }
    Track *current = m_tracks.at(m_trackview->selectedTrack());
    TrackType trackType = current->info().type;
    if (trackType == VideoTrack) {
        if (m_trackview->selectedTrack() == videoTarget) {
            // Switch off
            current->trackHeader->switchTarget(false);
            videoTarget = -1;
        } else {
            if (videoTarget > -1)
                m_tracks.at(videoTarget)->trackHeader->switchTarget(false);
            current->trackHeader->switchTarget(true);
            videoTarget = m_trackview->selectedTrack();
        }
    } else if (trackType == AudioTrack) {
        if (m_trackview->selectedTrack() == audioTarget) {
            // Switch off
            current->trackHeader->switchTarget(false);
            audioTarget = -1;
        } else {
            if (audioTarget > -1)
                m_tracks.at(audioTarget)->trackHeader->switchTarget(false);
            current->trackHeader->switchTarget(true);
            audioTarget = m_trackview->selectedTrack();
        }
    }
}

void Timeline::slotEnableZone(bool enable)
{
    KdenliveSettings::setUseTimelineZoneToEdit(enable);
    m_ruler->activateZone();
}

void Timeline::gotPreviewRender(int frame, const QString &file, int progress)
{
    m_ruler->updatePreview(frame);
    if (!m_hasOverlayTrack) {
        // Create overlay track
        Mlt::Playlist overlay(*m_tractor->profile());
        int trackIndex = tracksCount();
        m_tractor->lock();
        m_tractor->insert_track(overlay, trackIndex);
        m_tractor->unlock();
        m_hasOverlayTrack = true;
    }
    Mlt::Producer *overlayTrack = m_tractor->track(tracksCount());
    m_tractor->lock();
    Mlt::Playlist trackPlaylist((mlt_playlist) overlayTrack->get_service());
    delete overlayTrack;
    if (trackPlaylist.is_blank_at(frame)) {
        Mlt::Producer prod(*m_tractor->profile(), 0, file.toUtf8().constData());
        prod.set("mlt_service", "avformat-novalidate");
        trackPlaylist.insert_at(frame, &prod, 1);
    }
    m_tractor->unlock();
    m_doc->progressInfo(i18n("Rendering preview"), progress);
    //m_doc->updatePreview(progress);
}

void Timeline::invalidatePreview(int startFrame, int length)
{
    if (!m_hasOverlayTrack)
        return;
    int start = startFrame / 100;
    int end = lrintf((startFrame + length) / 100);
    Mlt::Producer *overlayTrack = m_tractor->track(tracksCount());
    m_tractor->lock();
    Mlt::Playlist trackPlaylist((mlt_playlist) overlayTrack->get_service());
    delete overlayTrack;
    QList <int> list;
    for (int i = start; i <=end; i++) {
        int ix = trackPlaylist.get_clip_index_at(100 * i);
        if (trackPlaylist.is_blank(ix))
            continue;
        list << i;
        Mlt::Producer *prod = trackPlaylist.replace_with_blank(ix);
        delete prod;
        m_ruler->updatePreview(i * 100, false);
    }
    trackPlaylist.consolidate_blanks();
    m_tractor->unlock();
    m_doc->invalidatePreviews(list);
}

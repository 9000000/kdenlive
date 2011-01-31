/***************************************************************************
 *                         DocClipBase.cpp  -  description                 *
 *                           -------------------                           *
 *   begin                : Fri Apr 12 2002                                *
 *   Copyright (C) 2002 by Jason Wood (jasonwood@blueyonder.co.uk)         *
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




#include "docclipbase.h"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "clipmanager.h"
#include "slideshowclip.h"

#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KDebug>

#include <QCryptographicHash>
#include <QtConcurrentRun>

#include <cstdio>

DocClipBase::DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id) :
        QObject(),
        m_audioFrameCache(),
        m_refcount(0),
        m_baseTrackProducers(),
        m_audioTrackProducers(),
        m_videoOnlyProducer(NULL),
        m_snapMarkers(QList < CommentedTime >()),
        m_duration(),
        m_audioTimer(NULL),
        m_thumbProd(NULL),
        m_audioThumbCreated(false),
        m_id(id),
        m_placeHolder(xml.hasAttribute("placeholder")),
        m_properties(),
        m_abortProxy(false)
{
    int type = xml.attribute("type").toInt();
    m_clipType = (CLIPTYPE) type;
    if (m_placeHolder) xml.removeAttribute("placeholder");
    QDomNamedNodeMap attributes = xml.attributes();
    for (int i = 0; i < attributes.count(); i++) {
        QString name = attributes.item(i).nodeName();
        if (name.startsWith("meta.attr.")) {
            m_metadata.insert(name.section('.', 2, 3), attributes.item(i).nodeValue());
        } else m_properties.insert(name, attributes.item(i).nodeValue());
    }

    if (xml.hasAttribute("cutzones")) {
        QStringList cuts = xml.attribute("cutzones").split(";", QString::SkipEmptyParts);
        for (int i = 0; i < cuts.count(); i++) {
            QString z = cuts.at(i);
            addCutZone(z.section('-', 0, 0).toInt(), z.section('-', 1, 1).toInt(), z.section('-', 2, 2));
        }
    }

    KUrl url = KUrl(xml.attribute("resource"));
    if (!m_properties.contains("file_hash") && !url.isEmpty()) getFileHash(url.path());

    if (xml.hasAttribute("duration")) {
        setDuration(GenTime(xml.attribute("duration").toInt(), KdenliveSettings::project_fps()));
    } else {
        int out = xml.attribute("out").toInt();
        int in = xml.attribute("in").toInt();
        setDuration(GenTime(out - in, KdenliveSettings::project_fps()));
    }

    if (!m_properties.contains("name")) m_properties.insert("name", url.fileName());

    //if (!url.isEmpty() && QFile::exists(url.path()))
    {
        m_thumbProd = new KThumb(clipManager, url, m_id, m_properties.value("file_hash"));
        if (m_clipType == AV || m_clipType == AUDIO || m_clipType == PLAYLIST) slotCreateAudioTimer();
    }
    //kDebug() << "type is video" << (m_clipType == AV) << " " << m_clipType;
}

/*DocClipBase & DocClipBase::operator=(const DocClipBase & clip) {
    DocClipBase::operator=(clip);
    m_id = clip.getId();
    m_clipType = clip.clipType();
    m_name = clip.name();
    m_duration = clip.duration();
    m_audioThumbCreated = clip.audioThumbCreated();
    m_properties = clip.properties();
    return *this;
}*/

DocClipBase::~DocClipBase()
{
    kDebug() << "CLIP " << m_id << " DELETED******************************";
    delete m_thumbProd;
    if (m_audioTimer) {
        m_audioTimer->stop();
        delete m_audioTimer;
    }
    /*kDebug() <<" * * *CNT "<<m_baseTrackProducers.count();
    if (m_baseTrackProducers.count() > 0) kDebug()<<"YOYO: "<<m_baseTrackProducers.at(0)->get_out()<<", CUT: "<<m_baseTrackProducers.at(0)->is_cut();*/
    qDeleteAll(m_baseTrackProducers);
    m_baseTrackProducers.clear();
    qDeleteAll(m_audioTrackProducers);
    m_audioTrackProducers.clear();
    delete m_videoOnlyProducer;
    m_videoOnlyProducer = NULL;
}

void DocClipBase::setZone(QPoint zone)
{
    m_properties.insert("zone_in", QString::number(zone.x()));
    m_properties.insert("zone_out", QString::number(zone.y()));
}

QPoint DocClipBase::zone() const
{
    QPoint zone(m_properties.value("zone_in", "0").toInt(), m_properties.value("zone_out", "50").toInt());
    return zone;
}

void DocClipBase::slotCreateAudioTimer()
{
    connect(m_thumbProd, SIGNAL(audioThumbReady(QMap <int, QMap <int, QByteArray> >)), this , SLOT(updateAudioThumbnail(QMap <int, QMap <int, QByteArray> >)));
    m_audioTimer = new QTimer(this);
    connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(slotGetAudioThumbs()));
}

void DocClipBase::askForAudioThumbs()
{
    if (m_thumbProd && m_audioTimer) m_thumbProd->askForAudioThumbs(getId());
}

void DocClipBase::slotClearAudioCache()
{
    if (m_thumbProd) m_thumbProd->stopAudioThumbs();
    if (m_audioTimer != NULL) m_audioTimer->stop();
    m_audioFrameCache.clear();
    m_audioThumbCreated = false;
}

/*void DocClipBase::getClipMainThumb() {
    if (m_thumbProd) m_thumbProd->getMainThumb(m_properties.value("thumbnail").toInt());
}*/

KThumb *DocClipBase::thumbProducer()
{
    return m_thumbProd;
}

bool DocClipBase::audioThumbCreated() const
{
    return m_audioThumbCreated;
}

const QString DocClipBase::name() const
{

    return m_properties.value("name");
}

const QString &DocClipBase::getId() const
{
    return m_id;
}

void DocClipBase::setId(const QString &newId)
{
    m_id = newId;
}

const CLIPTYPE & DocClipBase::clipType() const
{
    return m_clipType;
}

void DocClipBase::setClipType(CLIPTYPE type)
{
    m_clipType = type;

    m_properties.insert("type", QString::number((int) type));
    if (m_thumbProd && m_audioTimer == NULL && (m_clipType == AV || m_clipType == AUDIO || m_clipType == PLAYLIST))
        slotCreateAudioTimer();
}

KUrl DocClipBase::fileURL() const
{
    QString res = m_properties.value("resource");
    if (m_clipType != COLOR && !res.isEmpty()) return KUrl(res);
    return KUrl();
}

void DocClipBase::setClipThumbFrame(const uint &ix)
{
    m_properties.insert("thumbnail", QString::number((int) ix));
}

uint DocClipBase::getClipThumbFrame() const
{
    return (uint) m_properties.value("thumbnail").toInt();
}

const QString DocClipBase::description() const
{
    return m_properties.value("description");
}

bool DocClipBase::isTransparent() const
{
    return (m_properties.value("transparency") == "1");
}

const QString DocClipBase::getProperty(const QString prop) const
{
    return m_properties.value(prop);
}

void DocClipBase::setDuration(GenTime dur)
{
    m_duration = dur;
    m_properties.insert("duration", QString::number((int) dur.frames(KdenliveSettings::project_fps())));
}

const GenTime &DocClipBase::duration() const
{
    return m_duration;
}

const GenTime DocClipBase::maxDuration() const
{
    if (m_clipType == COLOR || m_clipType == IMAGE || m_clipType == TEXT || (m_clipType == SLIDESHOW &&  m_properties.value("loop") == "1")) {
        /*const GenTime dur(15000, KdenliveSettings::project_fps());
        return dur;*/
        return GenTime();
    }
    return m_duration;
}

bool DocClipBase::hasFileSize() const
{
    return true;
}

qulonglong DocClipBase::fileSize() const
{
    return m_properties.value("file_size").toULongLong();
}

// virtual
QDomElement DocClipBase::toXML() const
{
    QDomDocument doc;
    QDomElement clip = doc.createElement("producer");

    QMapIterator<QString, QString> i(m_properties);
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) clip.setAttribute(i.key(), i.value());
    }
    doc.appendChild(clip);
    if (!m_cutZones.isEmpty()) {
        QStringList cuts;
        for (int i = 0; i < m_cutZones.size(); i++) {
            CutZoneInfo info = m_cutZones.at(i);
            cuts << QString::number(info.zone.x()) + "-" + QString::number(info.zone.y()) + "-" + info.description;
        }
        clip.setAttribute("cutzones", cuts.join(";"));
    }
    //kDebug() << "/// CLIP XML: " << doc.toString();
    return doc.documentElement();
}


void DocClipBase::setAudioThumbCreated(bool isDone)
{
    m_audioThumbCreated = isDone;
}


void DocClipBase::setThumbnail(const QPixmap & pixmap)
{
    m_thumbnail = pixmap;
}

const QPixmap & DocClipBase::thumbnail() const
{
    return m_thumbnail;
}

void DocClipBase::updateAudioThumbnail(QMap<int, QMap<int, QByteArray> > data)
{
    //kDebug() << "CLIPBASE RECIEDVED AUDIO DATA*********************************************";
    m_audioFrameCache = data;
    m_audioThumbCreated = true;
    emit gotAudioData();
}

QList < GenTime > DocClipBase::snapMarkers() const
{
    QList < GenTime > markers;

    for (int count = 0; count < m_snapMarkers.count(); ++count) {
        markers.append(m_snapMarkers.at(count).time());
    }

    return markers;
}

QList < CommentedTime > DocClipBase::commentedSnapMarkers() const
{
    return m_snapMarkers;
}


void DocClipBase::addSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() >= time)
            break;
    }

    if ((it != m_snapMarkers.end()) && ((*it).time() == time)) {
        (*it).setComment(comment);
        //kError() << "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo";
    } else {
        CommentedTime t(time, comment);
        m_snapMarkers.insert(it, t);
    }

}

void DocClipBase::editSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it;
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() == time)
            break;
    }
    if (it != m_snapMarkers.end()) {
        (*it).setComment(comment);
    } else {
        kError() << "trying to edit Snap Marker that does not already exists";
    }
}

QString DocClipBase::deleteSnapMarker(const GenTime & time)
{
    QString result = i18n("Marker");
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            break;
        ++itt;
    }

    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
        result = (*itt).comment();
        m_snapMarkers.erase(itt);
    }
    return result;
}


GenTime DocClipBase::hasSnapMarkers(const GenTime & time)
{
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            return time;
        ++itt;
    }

    return GenTime(0.0);
}

GenTime DocClipBase::findPreviousSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers.at(it).time() >= currTime)
            break;
    }
    if (it == 0) return GenTime();
    else if (it == m_snapMarkers.count() - 1 && m_snapMarkers.at(it).time() < currTime)
        return m_snapMarkers.at(it).time();
    else return m_snapMarkers.at(it - 1).time();
}

GenTime DocClipBase::findNextSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); it++) {
        if (m_snapMarkers.at(it).time() > currTime)
            break;
    }
    if (it < m_snapMarkers.count() && m_snapMarkers.at(it).time() > currTime) return m_snapMarkers.at(it).time();
    return duration();
}

QString DocClipBase::markerComment(GenTime t)
{
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt).comment();
        ++itt;
    }
    return QString();
}

void DocClipBase::clearProducers()
{
    m_baseTrackProducers.clear();
}

void DocClipBase::deleteProducers(bool clearThumbCreator)
{
    kDebug() << "// CLIP KILL PRODS ct: " << m_baseTrackProducers.count();
    if (clearThumbCreator && m_thumbProd) m_thumbProd->clearProducer();
    /*kDebug()<<"// CLIP KILL PRODS ct: "<<m_baseTrackProducers.count();
    int max = m_baseTrackProducers.count();
    for (int i = 0; i < max; i++) {
        kDebug()<<"// CLIP KILL PROD "<<i;
    Mlt::Producer *p = m_baseTrackProducers.takeAt(i);
    if (p != NULL) {
     delete p;
     p = NULL;
    }
    m_baseTrackProducers.insert(i, NULL);
    }*/

    delete m_videoOnlyProducer;
    m_videoOnlyProducer = NULL;

    qDeleteAll(m_baseTrackProducers);
    m_baseTrackProducers.clear();
    qDeleteAll(m_audioTrackProducers);
    m_audioTrackProducers.clear();
}

void DocClipBase::setValid()
{
    m_placeHolder = false;
}

void DocClipBase::setProducer(Mlt::Producer *producer, bool reset)
{
    if (producer == NULL || (m_placeHolder && !reset)) return;
    if (m_thumbProd && (reset || !m_thumbProd->hasProducer())) m_thumbProd->setProducer(producer);
    if (reset) {
        // Clear all previous producers
        kDebug() << "/+++++++++++++++   DELETE ALL PRODS " << producer->get("id");
        deleteProducers(false);
    }
    QString id = producer->get("id");
    if (id.contains('_')) {
        // this is a subtrack producer, insert it at correct place
        id = id.section('_', 1);
        if (id.endsWith("audio")) {
            int pos = id.section('_', 0, 0).toInt();
            if (pos >= m_audioTrackProducers.count()) {
                while (m_audioTrackProducers.count() - 1 < pos) {
                    m_audioTrackProducers.append(NULL);
                }
            }
            if (m_audioTrackProducers.at(pos) == NULL) m_audioTrackProducers[pos] = producer;
            return;
        } else if (id.endsWith("video")) {
            m_videoOnlyProducer = producer;
            return;
        }
        int pos = id.toInt();
        if (pos >= m_baseTrackProducers.count()) {
            while (m_baseTrackProducers.count() - 1 < pos) {
                m_baseTrackProducers.append(NULL);
            }
        }
        if (m_baseTrackProducers.at(pos) == NULL) m_baseTrackProducers[pos] = producer;
    } else {
        if (m_baseTrackProducers.isEmpty()) m_baseTrackProducers.append(producer);
        else if (m_baseTrackProducers.at(0) == NULL) m_baseTrackProducers[0] = producer;
    }
    //m_clipProducer = producer;
    //m_clipProducer->set("transparency", m_properties.value("transparency").toInt());
}

static double getPixelAspect(QMap<QString, QString>& props) {
    int width = props.value("frame_size").section('x', 0, 0).toInt();
    int height = props.value("frame_size").section('x', 1, 1).toInt();
    int aspectNumerator = props.value("force_aspect_num").toInt();
    int aspectDenominator = props.value("force_aspect_den").toInt();
    if (aspectDenominator != 0 && width != 0)
        return double(height) * aspectNumerator / aspectDenominator / width;    
    else
        return 1.0;
}

Mlt::Producer *DocClipBase::audioProducer(int track)
{
    if (m_audioTrackProducers.count() <= track) {
        while (m_audioTrackProducers.count() - 1 < track) {
            m_audioTrackProducers.append(NULL);
        }
    }
    if (m_audioTrackProducers.at(track) == NULL) {
        Mlt::Producer *base = producer();
        m_audioTrackProducers[track] = new Mlt::Producer(*(base->profile()), base->get("resource"));
        if (m_properties.contains("force_aspect_num") && m_properties.contains("force_aspect_den") && m_properties.contains("frame_size"))
            m_audioTrackProducers.at(track)->set("force_aspect_ratio", getPixelAspect(m_properties));
        if (m_properties.contains("force_fps")) m_audioTrackProducers.at(track)->set("force_fps", m_properties.value("force_fps").toDouble());
        if (m_properties.contains("force_progressive")) m_audioTrackProducers.at(track)->set("force_progressive", m_properties.value("force_progressive").toInt());
        if (m_properties.contains("force_tff")) m_audioTrackProducers.at(track)->set("force_tff", m_properties.value("force_tff").toInt());
        if (m_properties.contains("threads")) m_audioTrackProducers.at(track)->set("threads", m_properties.value("threads").toInt());
        m_audioTrackProducers.at(track)->set("video_index", -1);
        if (m_properties.contains("audio_index")) m_audioTrackProducers.at(track)->set("audio_index", m_properties.value("audio_index").toInt());
        m_audioTrackProducers.at(track)->set("id", QString(getId() + '_' + QString::number(track) + "_audio").toUtf8().data());
        if (m_properties.contains("force_colorspace")) m_audioTrackProducers.at(track)->set("force_colorspace", m_properties.value("force_colorspace").toInt());
        if (m_properties.contains("full_luma")) m_audioTrackProducers.at(track)->set("set.force_full_luma", m_properties.value("full_luma").toInt());
    }
    return m_audioTrackProducers.at(track);
}

Mlt::Producer *DocClipBase::videoProducer()
{
    if (m_videoOnlyProducer == NULL) {
        int i;
        for (i = 0; i < m_baseTrackProducers.count(); i++)
            if (m_baseTrackProducers.at(i) != NULL) break;
        if (i >= m_baseTrackProducers.count()) return NULL;
        m_videoOnlyProducer = new Mlt::Producer(*m_baseTrackProducers.at(i)->profile(), m_baseTrackProducers.at(i)->get("resource"));
        if (m_properties.contains("force_aspect_num") && m_properties.contains("force_aspect_den") && m_properties.contains("frame_size"))
            m_videoOnlyProducer->set("force_aspect_ratio", getPixelAspect(m_properties));
        if (m_properties.contains("force_fps")) m_videoOnlyProducer->set("force_fps", m_properties.value("force_fps").toDouble());
        if (m_properties.contains("force_progressive")) m_videoOnlyProducer->set("force_progressive", m_properties.value("force_progressive").toInt());
        if (m_properties.contains("force_tff")) m_videoOnlyProducer->set("force_tff", m_properties.value("force_tff").toInt());
        if (m_properties.contains("threads")) m_videoOnlyProducer->set("threads", m_properties.value("threads").toInt());
        m_videoOnlyProducer->set("audio_index", -1);
        if (m_properties.contains("video_index")) m_videoOnlyProducer->set("video_index", m_properties.value("video_index").toInt());
        m_videoOnlyProducer->set("id", QString(getId() + "_video").toUtf8().data());
        if (m_properties.contains("force_colorspace")) m_videoOnlyProducer->set("force_colorspace", m_properties.value("force_colorspace").toInt());
        if (m_properties.contains("full_luma")) m_videoOnlyProducer->set("set.force_full_luma", m_properties.value("full_luma").toInt());
    }
    return m_videoOnlyProducer;
}

Mlt::Producer *DocClipBase::producer(int track)
{
    /*for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i)) kDebug() << "// PROD: " << i << ", ID: " << m_baseTrackProducers.at(i)->get("id");
    }*/
    if (track == -1 || (m_clipType != AUDIO && m_clipType != AV)) {
        if (m_baseTrackProducers.count() == 0) return NULL;
        for (int i = 0; i < m_baseTrackProducers.count(); i++) {
            if (m_baseTrackProducers.at(i) != NULL)
                return m_baseTrackProducers.at(i);
        }
        return NULL;
    }
    if (track >= m_baseTrackProducers.count()) {
        while (m_baseTrackProducers.count() - 1 < track) {
            m_baseTrackProducers.append(NULL);
        }
    }
    if (m_baseTrackProducers.at(track) == NULL) {
        int i;
        for (i = 0; i < m_baseTrackProducers.count(); i++)
            if (m_baseTrackProducers.at(i) != NULL) break;

        if (i >= m_baseTrackProducers.count()) return NULL;

        if (KIO::NetAccess::exists(KUrl(m_baseTrackProducers.at(i)->get("resource")), KIO::NetAccess::SourceSide, 0))
            m_baseTrackProducers[track] = new Mlt::Producer(*m_baseTrackProducers.at(i)->profile(), m_baseTrackProducers.at(i)->get("resource"));
        else { // special case for placeholder clips
            m_baseTrackProducers[track] = NULL;
            return NULL;
        }
        if (m_properties.contains("force_aspect_num") && m_properties.contains("force_aspect_den") && m_properties.contains("frame_size"))
            m_baseTrackProducers[track]->set("force_aspect_ratio", getPixelAspect(m_properties));
        if (m_properties.contains("force_fps")) m_baseTrackProducers[track]->set("force_fps", m_properties.value("force_fps").toDouble());
        if (m_properties.contains("force_progressive")) m_baseTrackProducers[track]->set("force_progressive", m_properties.value("force_progressive").toInt());
        if (m_properties.contains("force_tff")) m_baseTrackProducers[track]->set("force_tff", m_properties.value("force_tff").toInt());
        if (m_properties.contains("threads")) m_baseTrackProducers[track]->set("threads", m_properties.value("threads").toInt());
        if (m_properties.contains("video_index")) m_baseTrackProducers[track]->set("video_index", m_properties.value("video_index").toInt());
        if (m_properties.contains("audio_index")) m_baseTrackProducers[track]->set("audio_index", m_properties.value("audio_index").toInt());
        m_baseTrackProducers[track]->set("id", QString(getId() + '_' + QString::number(track)).toUtf8().data());

        if (m_properties.contains("force_colorspace")) m_baseTrackProducers[track]->set("force_colorspace", m_properties.value("force_colorspace").toInt());
        if (m_properties.contains("full_luma")) m_baseTrackProducers[track]->set("set.force_full_luma", m_properties.value("full_luma").toInt());
    }
    return m_baseTrackProducers.at(track);
}

void DocClipBase::setProducerProperty(const char *name, int data)
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, double data)
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::setProducerProperty(const char *name, const char *data)
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, data);
    }
}

void DocClipBase::resetProducerProperty(const char *name)
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL)
            m_baseTrackProducers[i]->set(name, (const char*) NULL);
    }
}

const char *DocClipBase::producerProperty(const char *name) const
{
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            return m_baseTrackProducers.at(i)->get(name);
        }
    }
    return NULL;
}


void DocClipBase::slotRefreshProducer()
{
    if (m_baseTrackProducers.count() == 0) return;
    kDebug() << "////////////   REFRESH CLIP !!!!!!!!!!!!!!!!";
    if (m_clipType == SLIDESHOW) {
        /*Mlt::Producer producer(*(m_clipProducer->profile()), getProperty("resource").toUtf8().data());
        delete m_clipProducer;
        m_clipProducer = new Mlt::Producer(producer.get_producer());
        if (!getProperty("out").isEmpty()) m_clipProducer->set_in_and_out(getProperty("in").toInt(), getProperty("out").toInt());*/
        setProducerProperty("ttl", getProperty("ttl").toInt());
        //m_clipProducer->set("id", getProperty("id"));
        if (!getProperty("animation").isEmpty()) {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "affine") == 0) {
                    break;
                } else if (strcmp(filter->get("mlt_service"), "boxblur") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }

            if (!filter || strcmp(filter->get("mlt_service"), "affine")) {
                // filter does not exist, create it.
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "affine");
                if (filter && filter->is_valid()) {
                    int cycle = getProperty("ttl").toInt();
                    QString geometry = SlideshowClip::animationToGeometry(getProperty("animation"), cycle);
                    if (!geometry.isEmpty()) {
                        if (getProperty("animation").contains("low-pass")) {
                            Mlt::Filter *blur = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "boxblur");
                            if (blur && blur->is_valid())
                                clipService.attach(*blur);
                        }
                        filter->set("transition.geometry", geometry.toUtf8().data());
                        filter->set("transition.cycle", cycle);
                        clipService.attach(*filter);
                    }
                }
            }
        } else {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "affine") == 0 || strcmp(filter->get("mlt_service"), "boxblur") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
        if (getProperty("fade") == "1") {
            // we want a fade filter effect
            kDebug() << "////////////   FADE WANTED";
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "luma") == 0) {
                    break;
                }
                ct++;
                filter = clipService.filter(ct);
            }

            if (filter && strcmp(filter->get("mlt_service"), "luma") == 0) {
                filter->set("cycle", getProperty("ttl").toInt());
                filter->set("duration", getProperty("luma_duration").toInt());
                filter->set("luma.resource", getProperty("luma_file").toUtf8().data());
                if (!getProperty("softness").isEmpty()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
            } else {
                // filter does not exist, create it...
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "luma");
                filter->set("cycle", getProperty("ttl").toInt());
                filter->set("duration", getProperty("luma_duration").toInt());
                filter->set("luma.resource", getProperty("luma_file").toUtf8().data());
                if (!getProperty("softness").isEmpty()) {
                    int soft = getProperty("softness").toInt();
                    filter->set("luma.softness", (double) soft / 100.0);
                }
                clipService.attach(*filter);
            }
        } else {
            kDebug() << "////////////   FADE NOT WANTED!!!";
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "luma") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
        if (getProperty("crop") == "1") {
            // we want a center crop filter effect
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(ct);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "crop") == 0) {
                    break;
                }
                ct++;
                filter = clipService.filter(ct);
            }

            if (!filter || strcmp(filter->get("mlt_service"), "crop")) {
                // filter does not exist, create it...
                Mlt::Filter *filter = new Mlt::Filter(*(m_baseTrackProducers.at(0)->profile()), "crop");
                filter->set("center", 1);
                clipService.attach(*filter);
            }
        } else {
            Mlt::Service clipService(m_baseTrackProducers.at(0)->get_service());
            int ct = 0;
            Mlt::Filter *filter = clipService.filter(0);
            while (filter) {
                if (strcmp(filter->get("mlt_service"), "crop") == 0) {
                    clipService.detach(*filter);
                } else ct++;
                filter = clipService.filter(ct);
            }
        }
    }
}

void DocClipBase::setProperties(QMap <QString, QString> properties)
{
    // changing clip type is not allowed
    properties.remove("type");
    QMapIterator<QString, QString> i(properties);
    bool refreshProducer = false;
    QStringList keys;
    keys << "luma_duration" << "luma_file" << "fade" << "ttl" << "softness" << "crop" << "animation";
    while (i.hasNext()) {
        i.next();
        setProperty(i.key(), i.value());
        if (m_clipType == SLIDESHOW && keys.contains(i.key())) refreshProducer = true;
    }
    if (refreshProducer) slotRefreshProducer();
}

void DocClipBase::setMetadata(QMap <QString, QString> properties)
{
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        if (i.value().isEmpty() && m_metadata.contains(i.key())) {
            m_metadata.remove(i.key());
        } else {
            m_metadata.insert(i.key(), i.value());
        }
    }
}

QMap <QString, QString> DocClipBase::metadata() const
{
    return m_metadata;
}

void DocClipBase::clearProperty(const QString &key)
{
    m_properties.remove(key);
}

void DocClipBase::getFileHash(const QString url)
{
    if (m_clipType == SLIDESHOW) return;
    QFile file(url);
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        //kDebug() << "SETTING HASH of" << value;
        m_properties.insert("file_size", QString::number(file.size()));
        /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
        if (file.size() > 1000000*2) {
            fileData = file.read(1000000);
            if (file.seek(file.size() - 1000000))
                fileData.append(file.readAll());
        } else
            fileData = file.readAll();
        file.close();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        m_properties.insert("file_hash", QString(fileHash.toHex()));
    }
}

bool DocClipBase::checkHash() const
{
    KUrl url = fileURL();
    if (!url.isEmpty() && getClipHash() != getHash(url.path())) return false;
    return true;
}

QString DocClipBase::getClipHash() const
{
    QString hash;
    if (m_clipType == SLIDESHOW) hash = QCryptographicHash::hash(m_properties.value("resource").toAscii().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == COLOR) hash = QCryptographicHash::hash(m_properties.value("colour").toAscii().data(), QCryptographicHash::Md5).toHex();
    else if (m_clipType == TEXT) hash = QCryptographicHash::hash(QString("title" + getId() + m_properties.value("xmldata")).toUtf8().data(), QCryptographicHash::Md5).toHex();
    else hash = m_properties.value("file_hash");
    return hash;
}

void DocClipBase::setPlaceHolder(bool place)
{
    m_placeHolder = place;
}

// static
QString DocClipBase::getHash(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        QByteArray fileData;
        QByteArray fileHash;
        /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
        if (file.size() > 1000000*2) {
            fileData = file.read(1000000);
            if (file.seek(file.size() - 1000000))
                fileData.append(file.readAll());
        } else
            fileData = file.readAll();
        file.close();
        return QCryptographicHash::hash(fileData, QCryptographicHash::Md5).toHex();
    }
    return QString();
}

void DocClipBase::refreshThumbUrl()
{
    if (m_thumbProd) m_thumbProd->updateThumbUrl(m_properties.value("file_hash"));
}

void DocClipBase::setProperty(const QString &key, const QString &value)
{
    m_properties.insert(key, value);
    if (key == "resource") {
        getFileHash(value);
        if (m_thumbProd) m_thumbProd->updateClipUrl(KUrl(value), m_properties.value("file_hash"));
    } else if (key == "out") setDuration(GenTime(value.toInt(), KdenliveSettings::project_fps()));
    //else if (key == "transparency") m_clipProducer->set("transparency", value.toInt());
    else if (key == "colour") {
        setProducerProperty("colour", value.toUtf8().data());
    } else if (key == "templatetext") {
        setProducerProperty("templatetext", value.toUtf8().data());
        setProducerProperty("force_reload", 1);
    } else if (key == "xmldata") {
        setProducerProperty("xmldata", value.toUtf8().data());
        setProducerProperty("force_reload", 1);
    } else if (key == "force_aspect_num") {
        if (value.isEmpty()) {
            m_properties.remove("force_aspect_num");
            resetProducerProperty("force_aspect_ratio");
        } else setProducerProperty("force_aspect_ratio", getPixelAspect(m_properties));
    } else if (key == "force_aspect_den") {
        if (value.isEmpty()) {
            m_properties.remove("force_aspect_den");
            resetProducerProperty("force_aspect_ratio");
        } else setProducerProperty("force_aspect_ratio", getPixelAspect(m_properties));
    } else if (key == "force_fps") {
        if (value.isEmpty()) {
            m_properties.remove("force_fps");
            resetProducerProperty("force_fps");
        } else setProducerProperty("force_fps", value.toDouble());
    } else if (key == "force_progressive") {
        if (value.isEmpty()) {
            m_properties.remove("force_progressive");
            resetProducerProperty("force_progressive");
        } else setProducerProperty("force_progressive", value.toInt());
    } else if (key == "force_tff") {
        if (value.isEmpty()) {
            m_properties.remove("force_tff");
            resetProducerProperty("force_tff");
        } else setProducerProperty("force_tff", value.toInt());
    } else if (key == "threads") {
        if (value.isEmpty()) {
            m_properties.remove("threads");
            setProducerProperty("threads", 1);
        } else setProducerProperty("threads", value.toInt());
    } else if (key == "video_index") {
        if (value.isEmpty()) {
            m_properties.remove("video_index");
            setProducerProperty("video_index", m_properties.value("default_video").toInt());
        } else setProducerProperty("video_index", value.toInt());
    } else if (key == "audio_index") {
        if (value.isEmpty()) {
            m_properties.remove("audio_index");
            setProducerProperty("audio_index", m_properties.value("default_audio").toInt());
        } else setProducerProperty("audio_index", value.toInt());
    } else if (key == "force_colorspace") {
        if (value.isEmpty()) {
            m_properties.remove("force_colorspace");
            resetProducerProperty("force_colorspace");
        } else setProducerProperty("force_colorspace", value.toInt());
    } else if (key == "full_luma") {
        if (value.isEmpty()) {
            m_properties.remove("full_luma");
            resetProducerProperty("set.force_full_luma");
        } else setProducerProperty("set.force_full_luma", value.toInt());
    }
}

QMap <QString, QString> DocClipBase::properties() const
{
    return m_properties;
}

bool DocClipBase::slotGetAudioThumbs()
{
    if (m_thumbProd == NULL || isPlaceHolder()) return false;
    if (!KdenliveSettings::audiothumbnails() || m_audioTimer == NULL) {
        if (m_audioTimer != NULL) m_audioTimer->stop();
        return false;
    }
    if (m_audioThumbCreated) {
        m_audioTimer->stop();
        return false;
    }
    m_audioTimer->start(1500);
    double lengthInFrames = duration().frames(KdenliveSettings::project_fps());
    m_thumbProd->getAudioThumbs(2, 0, lengthInFrames /*must be number of frames*/, 20);
    return true;
}

bool DocClipBase::isPlaceHolder() const
{
    return m_placeHolder;
}

void DocClipBase::addCutZone(int in, int out, QString desc)
{
    CutZoneInfo info;
    info.zone = QPoint(in, out);
    info.description = desc;
    for (int i = 0; i < m_cutZones.count(); i++)
        if (m_cutZones.at(i).zone == info.zone) {
            return;
        }
    m_cutZones.append(info);
}

bool DocClipBase::hasCutZone(QPoint p) const
{
    for (int i = 0; i < m_cutZones.count(); i++)
        if (m_cutZones.at(i).zone == p) return true;
    return false;
}


void DocClipBase::removeCutZone(int in, int out)
{
    QPoint p(in, out);
    for (int i = 0; i < m_cutZones.count(); i++) {
        if (m_cutZones.at(i).zone == p) {
            m_cutZones.removeAt(i);
            i--;
        }
    }
}

void DocClipBase::updateCutZone(int oldin, int oldout, int in, int out, QString desc)
{
    QPoint old(oldin, oldout);
    for (int i = 0; i < m_cutZones.size(); ++i) {
        if (m_cutZones.at(i).zone == old) {
            CutZoneInfo info;
            info.zone = QPoint(in, out);
            info.description = desc;
            m_cutZones.replace(i, info);
            break;
        }
    }
}

QList <CutZoneInfo> DocClipBase::cutZones() const
{
    return m_cutZones;
}

bool DocClipBase::hasVideoCodec(const QString &codec) const
{
    Mlt::Producer *prod = NULL;
    if (m_baseTrackProducers.count() == 0) return false;
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            prod = m_baseTrackProducers.at(i);
            break;
        }
    }

    if (!prod) return false;
    int default_video = prod->get_int("video_index");
    char property[200];
    snprintf(property, sizeof(property), "meta.media.%d.codec.name", default_video);
    return prod->get(property) == codec;
}

bool DocClipBase::hasAudioCodec(const QString &codec) const
{
    Mlt::Producer *prod = NULL;
    if (m_baseTrackProducers.count() == 0) return false;
    for (int i = 0; i < m_baseTrackProducers.count(); i++) {
        if (m_baseTrackProducers.at(i) != NULL) {
            prod = m_baseTrackProducers.at(i);
            break;
        }
    }
    if (!prod) return false;
    int default_video = prod->get_int("audio_index");
    char property[200];
    snprintf(property, sizeof(property), "meta.media.%d.codec.name", default_video);
    return prod->get(property) == codec;
}

void DocClipBase::generateProxy(KUrl proxyFolder)
{
    if (m_proxyThread.isRunning()) return;
    QStringList parameters;
    parameters << "-i" << m_properties.value("resource");
    QString params = KdenliveSettings::proxyparams().simplified();
    foreach(QString s, params.split(' '))
    parameters << s;
    // Make sure we don't block when proxy file already exists
    parameters << "-y";
    if (m_properties.value("file_hash").isEmpty()) getFileHash(m_properties.value("resource"));
    QString proxydir=proxyFolder.path( KUrl::AddTrailingSlash) + "proxy/";
    KStandardDirs::makeDir(proxydir);
    QString path = proxydir + m_properties.value("file_hash") + ".avi";
    setProperty("proxy", path.toUtf8().data());
    if (QFile::exists(path)) {
        emit proxyReady(m_id, true);
        return;
    }
    parameters << path;
    m_proxyThread = QtConcurrent::run(this, &DocClipBase::slotGenerateProxy, parameters);
}

void DocClipBase::slotGenerateProxy(QStringList parameters)
{
    QProcess myProcess;
    myProcess.start("ffmpeg", parameters);
    myProcess.waitForStarted();
    int result = -1;
    while (myProcess.state() != QProcess::NotRunning) {
        // building proxy file
        if (m_abortProxy) {
            QString path = getProperty("proxy");
            myProcess.close();
            myProcess.waitForFinished();
            QFile::remove(path);
            m_abortProxy = false;
            result = 1;
        }
        myProcess.waitForFinished(500);
    }
    myProcess.waitForFinished();
    if (result == -1) result = myProcess.exitStatus();
    if (result == 0) emit proxyReady(m_id, true);
    else {
        clearProperty("proxy");
        emit proxyReady(m_id, false);
    }
}

void DocClipBase::abortProxy()
{
    if (m_proxyThread.isRunning()) {
        // Proxy is being created, abort
        m_abortProxy = true;
    }
    else {
        clearProperty("proxy");
        emit proxyReady(m_id, false);
    }
}


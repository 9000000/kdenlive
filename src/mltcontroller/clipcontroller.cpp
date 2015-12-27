/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clipcontroller.h"
#include "bincontroller.h"
#include "mltcontroller/effectscontroller.h"
#include "lib/audio/audioStreamInfo.h"
#include "timeline/timeline.h"
#include "renderer.h"

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QFileInfo>
#include <KLocalizedString>


ClipController::ClipController(BinController *bincontroller, Mlt::Producer& producer) : QObject()
    , selectedEffectIndex(1)
    , audioThumbCreated(false)
    , m_properties(new Mlt::Properties(producer.get_properties()))
    , m_usesProxy(false)
    , m_audioInfo(NULL)
    , m_audioIndex(0)
    , m_videoIndex(0)
    , m_clipType(Unknown)
    , m_hasLimitedDuration(true)
    , m_binController(bincontroller)
    , m_snapMarkers(QList < CommentedTime >())
{
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) {
        qDebug()<<"// WARNING, USING INVALID PRODUCER";
        return;
    }
    else {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            m_url = QUrl::fromLocalFile(m_properties->get("kdenlive:originalurl"));
            m_usesProxy = true;
        }
        else m_url = QUrl::fromLocalFile(m_properties->get("resource"));
        m_service = m_properties->get("mlt_service");
        getInfoForProducer();
    }
}

ClipController::ClipController(BinController *bincontroller) : QObject()
    , selectedEffectIndex(1)
    , audioThumbCreated(false)
    , m_masterProducer(NULL)
    , m_properties(NULL)
    , m_audioInfo(NULL)
    , m_audioIndex(0)
    , m_videoIndex(0)
    , m_clipType(Unknown)
    , m_hasLimitedDuration(true)
    , m_binController(bincontroller)
    , m_snapMarkers(QList < CommentedTime >())
{
}

ClipController::~ClipController()
{
  delete m_properties;
  delete m_masterProducer;
}

double ClipController::dar() const
{
    return m_binController->dar();
}

AudioStreamInfo *ClipController::audioInfo() const
{
    return m_audioInfo;
}

void ClipController::addMasterProducer(Mlt::Producer &producer)
{
    m_properties = new Mlt::Properties(producer.get_properties());
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            m_url = QUrl::fromLocalFile(m_properties->get("kdenlive:originalurl"));
            m_usesProxy = true;
        }
        else {
            m_url = QUrl::fromLocalFile(m_properties->get("resource"));
            m_usesProxy = false;
        }
        m_service = m_properties->get("mlt_service");
        getInfoForProducer();
    }
}

void ClipController::getProducerXML(QDomDocument& document, bool includeMeta)
{
    if (m_masterProducer) {
        QString xml = m_binController->getProducerXML(*m_masterProducer, includeMeta);
        document.setContent(xml);
    }
    else qDebug()<<" + + ++ NO MASTER PROD";
}

void ClipController::getInfoForProducer()
{
    date = QFileInfo(m_url.path()).lastModified();
    m_audioIndex = int_property("audio_index");
    m_videoIndex = int_property("video_index");
    if (m_service == "avformat" || m_service == "avformat-novalidate") {
        if (m_audioIndex == -1) {
            m_clipType = Video;
        }
        else if (m_videoIndex == -1) {
            m_clipType = Audio;
        }
        else {
            m_clipType = AV;
        }
    }
    else if (m_service == "qimage" || m_service == "pixbuf") {
        if (m_url.path().contains("%") || m_url.path().contains("./.all.")) {
            m_clipType = SlideShow;
        }
        else {
            m_clipType = Image;
        }
        m_hasLimitedDuration = false;
    }
    else if (m_service == "colour" || m_service == "color") {
        m_clipType = Color;
        m_hasLimitedDuration = false;
    }
    else if (m_service == "kdenlivetitle") {
        m_clipType = Text;
        m_hasLimitedDuration = false;
    }
    else if (m_service == "xml" || m_service == "consumer") {
        m_clipType = Playlist;
    }
    else if (m_service == "webvfx") {
        m_clipType = WebVfx;
    }
    else m_clipType = Unknown;
    if (m_audioIndex > -1) m_audioInfo = new AudioStreamInfo(m_masterProducer, m_audioIndex);
}

bool ClipController::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

Mlt::Producer &ClipController::originalProducer()
{
    return *m_masterProducer;
}

Mlt::Producer *ClipController::masterProducer()
{
    return new Mlt::Producer(*m_masterProducer);
}

bool ClipController::isValid()
{
    if (m_masterProducer == NULL) return false;
    return m_masterProducer->is_valid();
}

const QString ClipController::clipId()
{
    if (m_masterProducer == NULL) return QString();
    return property("id");
}

// static
const char *ClipController::getPassPropertiesList(bool passLength)
{
    if (!passLength) {
	return "kdenlive:proxy,kdenlive:originalurl,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_colorspace,set.force_full_luma,file_hash";
    }
    return "kdenlive:proxy,kdenlive:originalurl,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_colorspace,set.force_full_luma,templatetext,file_hash,xmldata,length";
}

QMap <QString, QString> ClipController::getPropertiesFromPrefix(const QString &prefix, bool withPrefix)
{
    Mlt::Properties subProperties;
    subProperties.pass_values(*m_properties, prefix.toUtf8().constData());
    QMap <QString,QString> subclipsData;
    for (int i = 0; i < subProperties.count(); i++) {
        subclipsData.insert(withPrefix ? QString(prefix + subProperties.get_name(i)) : subProperties.get_name(i), subProperties.get(i));
    }
    return subclipsData;
}


void ClipController::updateProducer(const QString &id, Mlt::Producer* producer)
{
    //TODO replace all track producers
    Q_UNUSED(id)

    Mlt::Properties passProperties;
    // Keep track of necessary properties
    passProperties.pass_list(*m_properties, getPassPropertiesList());
    delete m_properties;
    delete m_masterProducer;
    m_masterProducer = producer;
    m_properties = new Mlt::Properties(producer->get_properties());
    QString proxy = m_properties->get("kdenlive:proxy");
    if (proxy.length() > 2) {
        // This is a proxy producer, read original url from kdenlive property
        m_usesProxy = true;
    }
    else m_usesProxy = false;
    // Pass properties from previous producer
    m_properties->pass_list(passProperties, getPassPropertiesList());
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        // URL and name shoule not be updated otherwise when proxying a clip we cannot find back the original url
        /*m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        */
    }
}


Mlt::Producer *ClipController::getTrackProducer(const QString trackName, PlaylistState::ClipState clipState, double speed)
{
    //TODO
    Q_UNUSED(speed)

    if (trackName.isEmpty()) {
        return m_masterProducer;
    }
    if  (m_clipType != AV && m_clipType != Audio && m_clipType != Playlist) {
        // Only producers with audio need a different producer for each track (or we have an audio crackle bug)
        return new Mlt::Producer(m_masterProducer->parent());
    }
    QString clipWithTrackId = clipId();
    clipWithTrackId.append("_" + trackName);
    
    //TODO handle audio / video only producers and framebuffer
    if (clipState == PlaylistState::AudioOnly) clipWithTrackId.append("_audio");
    else if (clipState == PlaylistState::VideoOnly) clipWithTrackId.append("_video");
    
    Mlt::Producer *clone = m_binController->cloneProducer(*m_masterProducer);
    clone->set("id", clipWithTrackId.toUtf8().constData());
    //m_binController->replaceBinPlaylistClip(clipWithTrackId, clone->parent());
    return clone;
}

const QString ClipController::getStringDuration()
{
    if (m_masterProducer) return m_masterProducer->get_length_time(mlt_time_smpte);
    return QString(i18n("Unknown"));
}

GenTime ClipController::getPlaytime() const
{
    return GenTime(m_masterProducer->get_playtime(), m_binController->fps());
}

QString ClipController::property(const QString &name) const
{
    if (!m_properties) return QString();
    if (m_usesProxy && name.startsWith("meta.")) {
        QString correctedName = QStringLiteral("kdenlive:") + name;
        return m_properties->get(correctedName.toUtf8().constData());
    }
    return QString(m_properties->get(name.toUtf8().constData()));
}

int ClipController::int_property(const QString &name) const
{
    if (!m_properties) return 0;
    if (m_usesProxy && name.startsWith("meta.")) {
        QString correctedName = QStringLiteral("kdenlive:") + name;
        return m_properties->get_int(correctedName.toUtf8().constData());
    }
    return m_properties->get_int(name.toUtf8().constData());
}

double ClipController::double_property(const QString &name) const
{
    if (!m_properties) return 0;
    return m_properties->get_double(name.toUtf8().constData());
}

QColor ClipController::color_property(const QString &name) const
{
    if (!m_properties) return QColor();
    mlt_color color = m_properties->get_color(name.toUtf8().constData());
    return QColor::fromRgb(color.r, color.g, color.b);
}

double ClipController::originalFps() const
{
    if (!m_properties) return 0;
    QString propertyName = QString("meta.media.%1.stream.frame_rate").arg(m_videoIndex);
    return m_properties->get_double(propertyName.toUtf8().constData());
}

QString ClipController::videoCodecProperty(const QString &property) const
{
    if (!m_properties) return QString();
    QString propertyName = QString("meta.media.%1.codec.%2").arg(m_videoIndex).arg(property);
    return m_properties->get(propertyName.toUtf8().constData());
}

const QString ClipController::codec(bool audioCodec) const
{
    if (!m_properties || (m_clipType!= AV && m_clipType != Video && m_clipType != Audio)) return QString();
    QString propertyName = QString("meta.media.%1.codec.name").arg(audioCodec ? m_audioIndex : m_videoIndex);
    return m_properties->get(propertyName.toUtf8().constData());
}

QUrl ClipController::clipUrl() const
{
    return m_url;
}

QString ClipController::clipName() const
{
    QString name = property("kdenlive:clipname");
    if (!name.isEmpty()) return name;
    return m_url.fileName();
}

QString ClipController::description() const
{
    QString name = property("kdenlive:description");
    if (!name.isEmpty()) return name;
    return property("meta.attr.comment.markup");
}

QString ClipController::serviceName() const
{
    return m_service;
}

void ClipController::setProperty(const QString& name, int value)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProperty(const QString& name, double value)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProperty(const QString& name, const QString& value)
{
    //TODO: also set property on all track producers
    if (value.isEmpty()) {
        m_masterProducer->parent().set(name.toUtf8().constData(), (char *)NULL);
    }
    else m_masterProducer->parent().set(name.toUtf8().constData(), value.toUtf8().constData());
}

void ClipController::resetProperty(const QString& name)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), (char *)NULL);
}

ClipType ClipController::clipType() const
{
    return m_clipType;
}


QPixmap ClipController::pixmap(int framePosition, int width, int height)
{
    //int currentPosition = position();
    m_masterProducer->seek(framePosition);
    Mlt::Frame *frame = m_masterProducer->get_frame();
    if (frame == NULL || !frame->is_valid()) {
        QPixmap p(width, height);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }

    frame->set("rescale.interp", "bilinear");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);

    if (width == 0) {
        width = m_masterProducer->get_int("meta.media.width");
        if (width == 0) {
            width = m_masterProducer->get_int("width");
        }
    }
    if (height == 0) {
        height = m_masterProducer->get_int("meta.media.height");
        if (height == 0) {
            height = m_masterProducer->get_int("height");
        }
    }
    
    //     int ow = frameWidth;
    //     int oh = height;
    mlt_image_format format = mlt_image_rgb24a;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    const uchar* imagedata = frame->get_image(format, width, height);
    if (imagedata) {
        QImage temp(width, height, QImage::Format_ARGB32_Premultiplied);
        memcpy(temp.bits(), imagedata, width * height * 4);
        image = temp.rgbSwapped();
    }
    else image.fill(QColor(Qt::red).rgb());
    delete frame;


    QPixmap pixmap;;
    pixmap.convertFromImage(image);

    return pixmap;
}

QList < GenTime > ClipController::snapMarkers() const
{
    QList < GenTime > markers;
    for (int count = 0; count < m_snapMarkers.count(); ++count) {
        markers.append(m_snapMarkers.at(count).time());
    }

    return markers;
}

QList < CommentedTime > ClipController::commentedSnapMarkers() const
{
    return m_snapMarkers;
}

void ClipController::loadSnapMarker(const QString &seconds, const QString &hash)
{
    QLocale locale;
    GenTime markerTime(locale.toDouble(seconds));
    CommentedTime marker(hash, markerTime);
    if (m_snapMarkers.contains(marker)) {
        m_snapMarkers.removeAll(marker);
    }
    m_snapMarkers.append(marker);
    qSort(m_snapMarkers);
}

void ClipController::addSnapMarker(const CommentedTime &marker)
{
    if (m_snapMarkers.contains(marker)) {
        m_snapMarkers.removeAll(marker);
    }
    m_snapMarkers.append(marker);
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(marker.time().seconds());
    m_binController->storeMarker(markerId, marker.hash());
    qSort(m_snapMarkers);
}

void ClipController::editSnapMarker(const GenTime & time, const QString &comment)
{
    CommentedTime marker(time, comment);
    int ix = m_snapMarkers.indexOf(marker);
    if (ix == -1) {
        qCritical() << "trying to edit Snap Marker that does not already exists";
        return;
    }
    m_snapMarkers[ix].setComment(comment);
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(time.seconds());
    m_binController->storeMarker(markerId, QString());
}

QString ClipController::deleteSnapMarker(const GenTime & time)
{
    CommentedTime marker(time, QString());
    int ix = m_snapMarkers.indexOf(marker);
    if (ix == -1) {
        qCritical() << "trying to edit Snap Marker that does not already exists";
        return QString();
    }
    QString result = m_snapMarkers.at(ix).comment();
    m_snapMarkers.removeAt(ix);
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(time.seconds());
    m_binController->storeMarker(markerId, QString());
    return result;
}


GenTime ClipController::findPreviousSnapMarker(const GenTime & currTime)
{
    CommentedTime marker(currTime, QString());
    int ix = m_snapMarkers.indexOf(marker) - 1;
    return m_snapMarkers.at(qMax(ix, 0)).time();
}

GenTime ClipController::findNextSnapMarker(const GenTime & currTime)
{
    CommentedTime marker(currTime, QString());
    int ix = m_snapMarkers.indexOf(marker) + 1;
    if (ix == 0 || ix == m_snapMarkers.count()) return getPlaytime();
    return m_snapMarkers.at(ix).time();
}

QString ClipController::markerComment(const GenTime &t) const
{
    CommentedTime marker(t, QString());
    int ix = m_snapMarkers.indexOf(marker);
    if (ix == -1) return QString();
    return m_snapMarkers.at(ix).comment();
}

CommentedTime ClipController::markerAt(const GenTime &t) const
{
    CommentedTime marker(t, QString());
    int ix = m_snapMarkers.indexOf(marker);
    if (ix == -1) return CommentedTime();
    return m_snapMarkers.at(ix);
}

void ClipController::setZone(const QPoint &zone)
{
    setProperty("kdenlive:zone_in", zone.x());
    setProperty("kdenlive:zone_out", zone.y());
}

QPoint ClipController::zone() const
{
    int in = int_property("kdenlive:zone_in");
    int out = int_property("kdenlive:zone_out");
    if (out <= in ) out = getPlaytime().frames(m_binController->fps());
    QPoint zone(in, out);
    return zone;
}

const QString ClipController::getClipHash() const
{
    return property("kdenlive:file_hash");
}

Mlt::Properties &ClipController::properties()
{
    return *m_properties;
}

Mlt::Profile *ClipController::profile()
{
    return m_binController->profile();
}

void ClipController::addEffect(const ProfileInfo &pInfo, QDomElement &effect)
{
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service = m_masterProducer->parent();
    ItemInfo info;
    info.cropStart = GenTime();
    info.cropDuration = getPlaytime();
    EffectsList eff = effectList();
    EffectsController::initEffect(info, pInfo, eff, property("kdenlive:proxy"), effect);
    // Add effect to list and setup a kdenlive_ix value
    int kdenlive_ix = 0;
    for (int i = 0; i < service.filter_count(); ++i) {
        Mlt::Filter *effect = service.filter(i);
        int ix = effect->get_int("kdenlive_ix");
        if (ix > kdenlive_ix) kdenlive_ix = ix;
    }
    kdenlive_ix++;
    effect.setAttribute(QLatin1String("kdenlive_ix"), kdenlive_ix);
    EffectsParameterList params = EffectsController::getEffectArgs(pInfo, effect);
    Render::addFilterToService(service, params, getPlaytime().frames(m_binController->fps()));
    m_binController->updateTrackProducer(clipId());
}

void ClipController::removeEffect(int effectIndex)
{
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service(m_masterProducer->parent());
    Render::removeFilterFromService(service, effectIndex, true);
    m_binController->updateTrackProducer(clipId());
}

EffectsList ClipController::effectList()
{
    return xmlEffectList(m_masterProducer->profile(), m_masterProducer->parent());
}

// static
EffectsList ClipController::xmlEffectList(Mlt::Profile *profile, Mlt::Service &service)
{
    ProfileInfo profileinfo;
    profileinfo.profileSize = QSize(profile->width(), profile->height());
    profileinfo.profileFps = profile->fps();
    EffectsList effList(true);
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        Mlt::Filter *effect = service.filter(ix);
        QDomElement clipeffect = Timeline::getEffectByTag(effect->get("tag"), effect->get("kdenlive_id"));
        QDomElement currenteffect = clipeffect.cloneNode().toElement();
	// recover effect parameters
        QDomNodeList params = currenteffect.elementsByTagName("parameter");
	if (effect->get_int("disable") == 1) {
	    currenteffect.setAttribute("disable", 1);
	}
        for (int i = 0; i < params.count(); ++i) {
            QDomElement param = params.item(i).toElement();
            Timeline::setParam(profileinfo, param, effect->get(param.attribute("name").toUtf8().constData()));
        }
        effList.append(currenteffect);
    }
    return effList;
}

void ClipController::changeEffectState(const QList <int> indexes, bool disable)
{
    Mlt::Service service = m_masterProducer->parent();
    for (int i = 0; i < service.filter_count(); ++i) {
        Mlt::Filter *effect = service.filter(i);
        if (effect && effect->is_valid() && indexes.contains(effect->get_int("kdenlive_ix"))) {
            effect->set("disable", (int) disable);
        }
    }
    m_binController->updateTrackProducer(clipId());
}

void ClipController::updateEffect(const ProfileInfo &pInfo, const QDomElement &e, int ix)
{
    EffectsParameterList params = EffectsController::getEffectArgs(pInfo, e);
    Mlt::Service service = m_masterProducer->parent();
    for (int i = 0; i < service.filter_count(); ++i) {
        Mlt::Filter *effect = service.filter(i);
        if (!effect || !effect->is_valid() || effect->get_int("kdenlive_ix") != ix) continue;
        service.lock();
        QString prefix;
        QString ser = effect->get("mlt_service");
        if (ser == "region") prefix = "filter0.";
        for (int j = 0; j < params.count(); ++j) {
            effect->set((prefix + params.at(j).name()).toUtf8().constData(), params.at(j).value().toUtf8().constData());
            //qDebug()<<params.at(j).name()<<" = "<<params.at(j).value();
        }
        service.unlock();
    }
    m_binController->updateTrackProducer(clipId());
    //slotRefreshTracks();
}

bool ClipController::hasEffects() const
{
    Mlt::Service service = m_masterProducer->parent();
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        Mlt::Filter *effect = service.filter(ix);
        QString id = effect->get("kdenlive_ix");
        if (!id.isEmpty()) return true;
    }
    return false;
}

void ClipController::disableEffects(bool disable)
{
    Mlt::Service service = m_masterProducer->parent();
    bool changed = false;
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        Mlt::Filter *effect = service.filter(ix);
        QString id = effect->get("kdenlive_ix");
        if (id.isEmpty()) continue;
        int disabled = effect->get_int("disable");
        if (disable) {
            // we want to disable all kdenlive effects
            if (disabled == 1) {
                continue;
            }
            effect->set("disable", 1);
            effect->set("auto_disable", 1);
            changed = true;
        } else {
            // We want to re-enable effects
            int auto_disable = effect->get_int("auto_disable");
            if (auto_disable == 1) {
                effect->set("disable", (char*) NULL);
                effect->set("auto_disable", (char*) NULL);
                changed = true;
            }
        }
    }
    if (changed) m_binController->updateTrackProducer(clipId());
}


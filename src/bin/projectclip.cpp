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

#include "projectclip.h"
#include "projectfolder.h"
#include "projectsubclip.h"
#include "bin.h"
#include "timecode.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "timeline/clip.h"
#include "project/projectcommands.h"
#include "mltcontroller/clipcontroller.h"
#include "lib/audio/audioStreamInfo.h"
#include "mltcontroller/clippropertiescontroller.h"

#include <QDomElement>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <KLocalizedString>
#include <KMessageBox>


ProjectClip::ProjectClip(const QString &id, QIcon thumb, ClipController *controller, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, id, parent)
    , m_abortAudioThumb(false)
    , m_controller(controller)
    , m_thumbsProducer(NULL)
{
    m_clipStatus = StatusReady;
    m_thumbnail = thumb;
    m_name = m_controller->clipName();
    m_duration = m_controller->getStringDuration();
    m_date = m_controller->date;
    m_description = m_controller->description();
    m_type = m_controller->clipType();
    // Make sure we have a hash for this clip
    hash();
    setParent(parent);
    connect(this, &ProjectClip::updateJobStatus, this, &ProjectClip::setJobStatus);
    bin()->loadSubClips(id, m_controller->getPropertiesFromPrefix(QStringLiteral("kdenlive:clipzone.")));
    createAudioThumbs();
}

ProjectClip::ProjectClip(const QDomElement& description, QIcon thumb, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::ClipItem, description, parent)
    , m_abortAudioThumb(false)
    , m_controller(NULL)
    , m_type(Unknown)
    , m_thumbsProducer(NULL)
{
    Q_ASSERT(description.hasAttribute("id"));
    m_clipStatus = StatusWaiting;
    m_thumbnail = thumb;
    if (description.hasAttribute(QStringLiteral("type"))) {
        m_type = (ClipType) description.attribute(QStringLiteral("type")).toInt();
    }
    m_temporaryUrl = QUrl::fromLocalFile(getXmlProperty(description, QStringLiteral("resource")));
    QString clipName = getXmlProperty(description, QStringLiteral("kdenlive:clipname"));
    if (!clipName.isEmpty()) {
        m_name = clipName;
    }
    else if (m_temporaryUrl.isValid()) {
        m_name = m_temporaryUrl.fileName();
    }
    else m_name = i18n("Untitled");
    connect(this, &ProjectClip::updateJobStatus, this, &ProjectClip::setJobStatus);
    setParent(parent);
}


ProjectClip::~ProjectClip()
{
    // controller is deleted in bincontroller
    abortAudioThumbs();
    bin()->slotAbortAudioThumb(m_id);
    QMutexLocker audioLock(&m_audioMutex);
    m_thumbMutex.lock();
    m_requestedThumbs.clear();
    m_thumbMutex.unlock();
    m_thumbThread.waitForFinished();
    delete m_thumbsProducer;
    audioFrameCache.clear();
}

void ProjectClip::abortAudioThumbs()
{
    m_abortAudioThumb = true;
    emit doAbortAudioThumbs();
}

QString ProjectClip::getToolTip() const
{
    return url().toLocalFile();
}

QString ProjectClip::getXmlProperty(const QDomElement &producer, const QString &propertyName, const QString &defaultValue)
{
    QString value = defaultValue;
    QDomNodeList props = producer.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().attribute(QStringLiteral("name")) == propertyName) {
            value = props.at(i).firstChild().nodeValue();
            break;
        }
    }
    return value;
}

void ProjectClip::updateAudioThumbnail(QVariantList audioLevels)
{
    audioFrameCache = audioLevels;
    m_controller->audioThumbCreated = true;
    bin()->emitRefreshAudioThumbs(m_id);
    emit gotAudioData();
}

QList < CommentedTime > ProjectClip::commentedSnapMarkers() const
{
    if (m_controller) return m_controller->commentedSnapMarkers();
    return QList < CommentedTime > ();
}

bool ProjectClip::audioThumbCreated() const
{
    return (m_controller && m_controller->audioThumbCreated);
}

ClipType ProjectClip::clipType() const
{
    return m_type;
}

bool ProjectClip::hasParent(const QString &id) const
{
    AbstractProjectItem *par = parent();
    while (par) {
	if (par->clipId() == id) {
	    return true;
	}
	par = par->parent();
    }
    return false;
}

ProjectClip* ProjectClip::clip(const QString &id)
{
    if (id == m_id) {
        return this;
    }
    return NULL;
}

ProjectFolder* ProjectClip::folder(const QString &id)
{
    Q_UNUSED(id)
    return NULL;
}

void ProjectClip::disableEffects(bool disable)
{
    if (m_controller) m_controller->disableEffects(disable);
}

ProjectSubClip* ProjectClip::getSubClip(int in, int out)
{
    for (int i = 0; i < count(); ++i) {
        ProjectSubClip *clip = static_cast<ProjectSubClip *>(at(i))->subClip(in, out);
        if (clip) {
            return clip;
        }
    }
    return NULL;
}

QStringList ProjectClip::subClipIds() const
{
    QStringList subIds;
    for (int i = 0; i < count(); ++i) {
        AbstractProjectItem *clip = at(i);
        if (clip) {
            subIds << clip->clipId();
        }
    }
    return subIds;
}

ProjectClip* ProjectClip::clipAt(int ix)
{
    if (ix == index()) {
        return this;
    }
    return NULL;
}

/*bool ProjectClip::isValid() const
{
    return m_controller->isValid();
}*/

QUrl ProjectClip::url() const
{
    if (m_controller) return m_controller->clipUrl();
    return m_temporaryUrl;
}

bool ProjectClip::hasLimitedDuration() const
{
    if (m_controller) {
        return m_controller->hasLimitedDuration();
    }
    return true;
}

GenTime ProjectClip::duration() const
{
    if (m_controller) {
	return m_controller->getPlaytime();
    }
    return GenTime();
}

void ProjectClip::reloadProducer(bool thumbnailOnly)
{
    QDomDocument doc;
    QDomElement xml = toXml(doc);
    if (thumbnailOnly) {
        // set a special flag to request thumbnail only
        xml.setAttribute(QStringLiteral("thumbnailOnly"), QStringLiteral("1"));
    }
    bin()->reloadProducer(m_id, xml);
}

void ProjectClip::setCurrent(bool current, bool notify)
{
    Q_UNUSED(notify)
    if (current && m_controller) {
        bin()->openProducer(m_controller);
        bin()->editMasterEffect(m_controller);
    }
}

QDomElement ProjectClip::toXml(QDomDocument& document, bool includeMeta)
{
    if (m_controller) {
        m_controller->getProducerXML(document, includeMeta);
        return document.documentElement().firstChildElement(QStringLiteral("producer"));
    } else {
        // We only have very basic infos, ike id and url, pass them
        QDomElement prod = document.createElement(QStringLiteral("producer"));
        prod.setAttribute(QStringLiteral("id"), m_id);
        EffectsList::setProperty(prod, QStringLiteral("resource"), m_temporaryUrl.path());
        if (m_type != Unknown) {
            prod.setAttribute(QStringLiteral("type"), (int) m_type);
        }
        document.appendChild(prod);
        return prod;
    }
}

void ProjectClip::setThumbnail(QImage img)
{
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    if (hasProxy() && !thumb.isNull()) {
        // Overlay proxy icon
        QPainter p(&thumb);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, thumb.height() / 2.5, thumb.height() / 2.5);
        p.fillRect(r, c);
        QFont font = p.font();
        font.setPixelSize(r.height());
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("The first letter of Proxy, used as abbreviation", "P"));
    }
    m_thumbnail = QIcon(thumb);
    emit thumbUpdated(img);
    bin()->emitItemUpdated(this);
}

QPixmap ProjectClip::thumbnail(int width, int height)
{
    return m_thumbnail.pixmap(width, height);
}

bool ProjectClip::setProducer(ClipController *controller, bool replaceProducer)
{
    if (!replaceProducer && m_controller) {
        qDebug()<<"// RECEIVED PRODUCER BUT WE ALREADY HAVE ONE\n----------";
        return false;
    }
    bool isNewProducer = true;
    if (m_controller) {
        // Replace clip for this controller
        resetProducerProperty("kdenlive:file_hash");
        isNewProducer = false;
    }
    else if (controller) {
        // We did not yet have the controller, update info
        m_controller = controller;
        if (m_name.isEmpty()) m_name = m_controller->clipName();
        m_date = m_controller->date;
        m_description = m_controller->description();
        m_temporaryUrl.clear();
        if (m_type == Unknown) m_type = m_controller->clipType();
    }
    m_duration = m_controller->getStringDuration();
    m_clipStatus = StatusReady;
    if (!hasProxy()) bin()->emitRefreshPanel(m_id);
    bin()->emitItemUpdated(this);
    // Make sure we have a hash for this clip
    hash();
    createAudioThumbs();
    return isNewProducer;
}

void ProjectClip::createAudioThumbs()
{
    if (KdenliveSettings::audiothumbnails() && (m_type == AV || m_type == Audio)) {
        bin()->requestAudioThumbs(m_id);
    }
}

Mlt::Producer *ProjectClip::originalProducer()
{
    if (!m_controller) {
        return NULL;
    }
    return &m_controller->originalProducer();
}

Mlt::Producer *ProjectClip::thumbProducer()
{
    QMutexLocker locker(&m_producerMutex);
    if (m_thumbsProducer) {
        return m_thumbsProducer;
    }
    if (!m_controller) {
        return NULL;
    }
    Mlt::Producer prod = m_controller->originalProducer();
    Clip clip(prod);
    m_thumbsProducer = clip.softClone(ClipController::getPassPropertiesList());
    // Check if we are using GPU accel, then we need to use alternate producer
    if (KdenliveSettings::gpu_accel()) {
        Mlt::Filter scaler(*prod.profile(), "swscale");
        Mlt::Filter converter(*prod.profile(), "avcolor_space");
        m_thumbsProducer->attach(scaler);
        m_thumbsProducer->attach(converter);
    }
    return m_thumbsProducer;
}

ClipController *ProjectClip::controller()
{
    return m_controller;
}

bool ProjectClip::isReady() const
{
    return m_controller != NULL && m_clipStatus == StatusReady;
}

/*void ProjectClip::setZone(const QPoint &zone)
{
    m_zone = zone;
}*/

QPoint ProjectClip::zone() const
{
    int x = getProducerIntProperty(QStringLiteral("kdenlive:zone_in"));
    int y = getProducerIntProperty(QStringLiteral("kdenlive:zone_out"));
    return QPoint(x, y);
}

void ProjectClip::resetProducerProperty(const QString &name)
{
    if (m_controller) {
        m_controller->resetProperty(name);
    }
}

void ProjectClip::setProducerProperty(const QString &name, int data)
{
    if (m_controller) {
	m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const QString &name, double data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

void ProjectClip::setProducerProperty(const QString &name, const QString &data)
{
    if (m_controller) {
        m_controller->setProperty(name, data);
    }
}

QMap <QString, QString> ProjectClip::currentProperties(const QMap <QString, QString> &props)
{
    QMap <QString, QString> currentProps;
    if (!m_controller) {
        return currentProps;
    }
    QMap<QString, QString>::const_iterator i = props.constBegin();
    while (i != props.constEnd()) {
        currentProps.insert(i.key(), m_controller->property(i.key()));
        ++i;
    }
    return currentProps;
}

QColor ProjectClip::getProducerColorProperty(const QString &key) const
{
    if (m_controller) {
        return m_controller->color_property(key);
    }
    return QColor();
}

int ProjectClip::getProducerIntProperty(const QString &key) const
{
    int value = 0;
    if (m_controller) {
        value = m_controller->int_property(key);
    }
    return value;
}

QString ProjectClip::getProducerProperty(const QString &key) const
{
    QString value;
    if (m_controller) {
	value = m_controller->property(key);
    }
    return value;
}

const QString ProjectClip::hash()
{
    if (m_controller) {
        QString clipHash = m_controller->property(QStringLiteral("kdenlive:file_hash"));
        if (!clipHash.isEmpty()) {
            return clipHash;
        }
    }
    return getFileHash();
}

const QString ProjectClip::getFileHash() const
{
    QByteArray fileData;
    QByteArray fileHash;
    switch (m_type) {
      case SlideShow:
          fileData = m_controller ? m_controller->clipUrl().toLocalFile().toUtf8() : m_temporaryUrl.toLocalFile().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      case Text:
          fileData = m_controller ? m_controller->property(QStringLiteral("xmldata")).toUtf8() : name().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      case QText:
          fileData = m_controller ? m_controller->property(QStringLiteral("text")).toUtf8() : name().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      case Color:
          fileData = m_controller ? m_controller->property(QStringLiteral("resource")).toUtf8() : name().toUtf8();
          fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          break;
      default:
          QFile file(m_controller ? m_controller->clipUrl().toLocalFile() : m_temporaryUrl.toLocalFile());
          if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
              /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
            if (file.size() > 2000000) {
                fileData = file.read(1000000);
                if (file.seek(file.size() - 1000000))
                    fileData.append(file.readAll());
            } else
                fileData = file.readAll();
            file.close();
            if (m_controller) m_controller->setProperty(QStringLiteral("kdenlive:file_size"), QString::number(file.size()));
            fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
          }
          break;
    }
    if (fileHash.isEmpty()) return QString();
    QString result = fileHash.toHex();
    if (m_controller) {
	m_controller->setProperty(QStringLiteral("kdenlive:file_hash"), result);
    }
    return result;
}

double ProjectClip::getOriginalFps() const
{
    if (!m_controller) return 0;
    return m_controller->originalFps();
}

bool ProjectClip::hasProxy() const
{
    QString proxy = getProducerProperty(QStringLiteral("kdenlive:proxy"));
    if (proxy.isEmpty() || proxy == QLatin1String("-")) return false;
    return true;
}

void ProjectClip::setProperties(QMap <QString, QString> properties, bool refreshPanel)
{
    QMapIterator<QString, QString> i(properties);
    QMap <QString, QString> passProperties;
    bool refreshAnalysis = false;
    bool reload = false;
    // Some properties also need to be passed to track producers
    QStringList timelineProperties;
    timelineProperties << QStringLiteral("force_aspect_ratio") << QStringLiteral("video_index") << QStringLiteral("audio_index") << QStringLiteral("set.force_full_luma")<< QStringLiteral("full_luma") <<QStringLiteral("threads") <<QStringLiteral("force_colorspace")<<QStringLiteral("force_tff")<<QStringLiteral("force_progressive")<<QStringLiteral("force_fps");
    QStringList keys;
    keys << QStringLiteral("luma_duration") << QStringLiteral("luma_file") << QStringLiteral("fade") << QStringLiteral("ttl") << QStringLiteral("softness") << QStringLiteral("crop") << QStringLiteral("animation");
    while (i.hasNext()) {
        i.next();
        setProducerProperty(i.key(), i.value());
        if (m_type == SlideShow && keys.contains(i.key())) {
            reload = true;
        }
        if (i.key().startsWith(QLatin1String("kdenlive:clipanalysis"))) refreshAnalysis = true;
        if (timelineProperties.contains(i.key())) {
            passProperties.insert(i.key(), i.value());
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:proxy"))) {
        QString value = properties.value(QStringLiteral("kdenlive:proxy"));
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == QLatin1String("-")) {
            // reset proxy
            if (bin()->hasPendingJob(m_id, AbstractClipJob::PROXYJOB)) {
                bin()->discardJobs(m_id, AbstractClipJob::PROXYJOB);
            }
            else {
                reloadProducer();
            }
        }
        else {
            // A proxy was requested, make sure to keep original url
            setProducerProperty(QStringLiteral("kdenlive:originalurl"), url().toLocalFile());
            bin()->startJob(m_id, AbstractClipJob::PROXYJOB);
        }
    }
    else if (properties.contains(QStringLiteral("resource"))) {
        // Clip resource changed, update thumbnail
        if (m_type != Color) {
            reloadProducer();
        }
        else reload = true;
    }

    if (properties.contains(QStringLiteral("xmldata")) || !passProperties.isEmpty()) {
        reload = true;
    }
    if (refreshAnalysis) emit refreshAnalysisPanel();
    if (properties.contains(QStringLiteral("length"))) {
        m_duration = m_controller->getStringDuration();
        bin()->emitItemUpdated(this);
    }

    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        m_name = properties.value(QStringLiteral("kdenlive:clipname"));
        bin()->emitItemUpdated(this);
    }
    if (refreshPanel) {
        // Some of the clip properties have changed through a command, update properties panel
        emit refreshPropertiesPanel();
    }
    if (reload) {
        // producer has changed, refresh monitor and thumbnail
        if (reload) reloadProducer(true);
        bin()->refreshClip(m_id);
    }
    if (!passProperties.isEmpty()) {
        bin()->updateTimelineProducers(m_id, passProperties);
    }
}

void ProjectClip::setJobStatus(int jobType, int status, int progress, const QString &statusMessage)
{
    m_jobType = (AbstractClipJob::JOBTYPE) jobType;
    if (progress > 0) {
        if (m_jobProgress == progress) return;
	m_jobProgress = progress;
    }
    else {
	m_jobProgress = (ClipJobStatus) status;
	if ((status == JobAborted || status == JobCrashed  || status == JobDone) && !statusMessage.isEmpty()) {
	    m_jobMessage = statusMessage;
            bin()->emitMessage(statusMessage, OperationCompletedMessage);
	}
    }
    bin()->emitItemUpdated(this);
}


ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    ClipPropertiesController *panel = new ClipPropertiesController(bin()->projectTimecode(), m_controller, parent);
    connect(this, SIGNAL(refreshPropertiesPanel()), panel, SLOT(slotReloadProperties()));
    connect(this, SIGNAL(refreshAnalysisPanel()), panel, SLOT(slotFillAnalysisData()));
    return panel;
}

void ProjectClip::updateParentInfo(const QString &folderid, const QString &foldername)
{
    Q_UNUSED(foldername)
    m_controller->setProperty(QStringLiteral("kdenlive:folderid"), folderid);
}

bool ProjectClip::matches(QString condition)
{
    //TODO
    Q_UNUSED(condition)
    return true;
}

const QString ProjectClip::codec(bool audioCodec) const
{
    if (!m_controller) return QString();
    return m_controller->codec(audioCodec);
}

bool ProjectClip::rename(const QString &name, int column)
{
    QMap <QString, QString> newProperites;
    QMap <QString, QString> oldProperites;
    bool edited = false;
    switch (column) {
      case 0:
        if (m_name == name) return false;
        // Rename clip
        oldProperites.insert(QStringLiteral("kdenlive:clipname"), m_name);
        newProperites.insert(QStringLiteral("kdenlive:clipname"), name);
        m_name = name;
        edited = true;
        break;
      case 2:
        if (m_description == name) return false;
        // Rename clip
        oldProperites.insert(QStringLiteral("kdenlive:description"), m_description);
        newProperites.insert(QStringLiteral("kdenlive:description"), name);
        m_description = name;
        edited = true;
        break;
    }
    if (edited) {
        bin()->slotEditClipCommand(m_id, oldProperites, newProperites);
    }
    return edited;
}

void ProjectClip::addClipMarker(QList <CommentedTime> newMarkers, QUndoCommand *groupCommand)
{
    if (!m_controller) return;
    QList <CommentedTime> oldMarkers;
    for (int i = 0; i < newMarkers.count(); ++i) {
        CommentedTime oldMarker = m_controller->markerAt(newMarkers.at(i).time());
        if (oldMarker == CommentedTime()) {
            oldMarker = newMarkers.at(i);
            oldMarker.setMarkerType(-1);
        }
        oldMarkers << oldMarker;
    }
    (void) new AddMarkerCommand(this, oldMarkers, newMarkers, groupCommand);
}

bool ProjectClip::deleteClipMarkers(QUndoCommand *command)
{
    QList <CommentedTime> markers = commentedSnapMarkers();
    if (markers.isEmpty()) {
        return false;
    }
    QList <CommentedTime> newMarkers;
    for (int i = 0; i < markers.size(); ++i) {
        CommentedTime marker = markers.at(i);
        marker.setMarkerType(-1);
        newMarkers << marker;
    }
    new AddMarkerCommand(this, markers, newMarkers, command);
    return true;
}

void ProjectClip::addMarkers(QList <CommentedTime> &markers)
{
    if (!m_controller) return;
    for (int i = 0; i < markers.count(); ++i) {
      if (markers.at(i).markerType() < 0) m_controller->deleteSnapMarker(markers.at(i).time());
      else m_controller->addSnapMarker(markers.at(i));
    }
    // refresh markers in clip monitor
    bin()->refreshClipMarkers(m_id);
    // refresh markers in timeline clips
    emit refreshClipDisplay();
}

void ProjectClip::addEffect(const ProfileInfo &pInfo, QDomElement &effect)
{
    m_controller->addEffect(pInfo, effect);
    bin()->updateMasterEffect(m_controller);
    bin()->emitItemUpdated(this);
}

void ProjectClip::removeEffect(int ix)
{
    m_controller->removeEffect(ix);
    bin()->updateMasterEffect(m_controller);
    bin()->emitItemUpdated(this);
}

QVariant ProjectClip::data(DataType type) const
{
    switch (type) {
      case AbstractProjectItem::IconOverlay:
            return m_controller != NULL ? (m_controller->hasEffects() ? QVariant("kdenlive-track_has_effect") : QVariant()) : QVariant();
            break;
        default:
	    break;
    }
    return AbstractProjectItem::data(type);
}

void ProjectClip::slotExtractImage(QList <int> frames)
{
    QMutexLocker lock(&m_thumbMutex);
    for (int i = 0; i < frames.count(); i++) {
        if (!m_requestedThumbs.contains(frames.at(i))) {
            m_requestedThumbs << frames.at(i);
        }
    }
    qSort(m_requestedThumbs);
    if (!m_thumbThread.isRunning()) {
        m_thumbThread = QtConcurrent::run(this, &ProjectClip::doExtractImage);
    }
}

void ProjectClip::doExtractImage()
{
    Mlt::Producer *prod = thumbProducer();
    if (prod == NULL || !prod->is_valid()) return;
    int fullWidth = (int)((double) 150 * prod->profile()->dar() + 0.5);
    QDir thumbFolder(bin()->projectFolder().path() + "/thumbs/");
    int max = prod->get_length();
    int pos;
    while (!m_requestedThumbs.isEmpty()) {
        m_thumbMutex.lock();
        pos = m_requestedThumbs.takeFirst();
        m_thumbMutex.unlock();
        if (thumbFolder.exists(hash() + '#' + QString::number(pos) + ".png")) {
            emit thumbReady(pos, QImage(thumbFolder.absoluteFilePath(hash() + '#' + QString::number(pos) + ".png")));
            continue;
        }
	if (pos >= max) pos = max - 1;
	prod->seek(pos);
	Mlt::Frame *frame = prod->get_frame();
	if (frame && frame->is_valid()) {
            QImage img = KThumb::getFrame(frame, fullWidth, 150);
            emit thumbReady(pos, img);
        }
        delete frame;
    }
}

void ProjectClip::slotExtractSubImage(QList <int> frames)
{
    Mlt::Producer *prod = thumbProducer();
    if (prod == NULL || !prod->is_valid()) return;
    int fullWidth = (int)((double) 150 * prod->profile()->dar() + 0.5);
    QDir thumbFolder(bin()->projectFolder().path() + "/thumbs/");
    for (int i = 0; i < frames.count(); i++) {
        int pos = frames.at(i);
        QString path = thumbFolder.absoluteFilePath(hash() + "#" + QString::number(pos) + ".png");
        QImage img(path);
        if (!img.isNull()) {
            for (int i = 0; i < count(); ++i) {
                ProjectSubClip *clip = static_cast<ProjectSubClip *>(at(i));
                if (clip && clip->zone().x() == pos) {
                    clip->setThumbnail(img);
                }
            }
            continue;
        }
        int max = prod->get_out();
        if (pos >= max) pos = max - 1;
        if (pos < 0) pos = 0;
        prod->seek(pos);
        Mlt::Frame *frame = prod->get_frame();
        if (frame && frame->is_valid()) {
            QImage img = KThumb::getFrame(frame, fullWidth, 150);
            if (!img.isNull()) {
                img.save(path);
                for (int i = 0; i < count(); ++i) {
                    ProjectSubClip *clip = static_cast<ProjectSubClip *>(at(i));
                    if (clip && clip->zone().x() == pos) {
                        clip->setThumbnail(img);
                    }
                }
            }
        }
        delete frame;
    }
}

int ProjectClip::audioChannels() const
{
    if (!m_controller || !m_controller->audioInfo()) return 0;
    return m_controller->audioInfo()->channels();
}

void ProjectClip::slotCreateAudioThumbs()
{
    QMutexLocker lock(&m_audioMutex);
    Mlt::Producer *prod = originalProducer();
    if (!prod || !prod->is_valid()) return;
    AudioStreamInfo *audioInfo = m_controller->audioInfo();
    int audioStream = audioInfo->ffmpeg_audio_index();
    if (audioInfo == NULL) return;
    QString clipHash = hash();
    if (clipHash.isEmpty()) return;
    QString audioPath = bin()->projectFolder().path() + "/thumbs/" + clipHash;
    if (audioStream > 0) {
        audioPath.append("_" + QString::number(audioInfo->audio_index()));
    }
    audioPath.append("_audio.png");
    int lengthInFrames = prod->get_length();
    int frequency = audioInfo->samplingRate();
    if (frequency <= 0) frequency = 48000;
    int channels = audioInfo->channels();
    if (channels <= 0) channels = 2;
    double frame = 0.0;
    QVariantList audioLevels;
    QImage image(audioPath);
    if (!image.isNull()) {
        // convert cached image
        int n = image.width() * image.height();
        for (int i = 0; i < n; i++) {
            QRgb p = image.pixel(i / 2, i % channels);
            audioLevels << qRed(p);
            audioLevels << qGreen(p);
            audioLevels << qBlue(p);
            audioLevels << qAlpha(p);
        }
    }
    if (audioLevels.size() > 0) {
        updateAudioThumbnail(audioLevels);
        return;
    }

    if (KdenliveSettings::ffmpegaudiothumbnails()) {
        QStringList args;
        QTemporaryFile tmpfile;
        if (!tmpfile.open()) {
            bin()->emitMessage(i18n("Cannot create temporary file, check disk space and permissions"), ErrorMessage);
            return;
        }
        QTemporaryFile tmpfile2;
        if (!tmpfile2.open()) {
            bin()->emitMessage(i18n("Cannot create temporary file, check disk space and permissions"), ErrorMessage);
            return;
        }
        tmpfile.close();
        tmpfile2.close();
        args << QStringLiteral("-i") << QUrl::fromLocalFile(prod->get("resource")).path();

        bool isFFmpeg = KdenliveSettings::ffmpegpath().contains("ffmpeg");

        if (channels == 1) {
            if (isFFmpeg) {
                args << QStringLiteral("-ac") << QString::number(channels);
                args << QStringLiteral("-filter_complex:a") << QStringLiteral("aformat=channel_layouts=mono,aresample=async=100");
                args << QStringLiteral("-map") << QStringLiteral("0:a%1").arg(audioStream > 0 ? ":" + QString::number(audioStream) : "") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("data")<< tmpfile.fileName();
            } else {
                args << QStringLiteral("-filter_complex:a") << QStringLiteral("aformat=channel_layouts=mono:sample_rates=100");
                args << QStringLiteral("-map") << QStringLiteral("0:a%1").arg(audioStream > 0 ? ":" + QString::number(audioStream) : "") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("s16le")<< tmpfile.fileName();
            }
        } else {
            if (isFFmpeg) {
                //args << QStringLiteral("-ac") << QString::number(channels);
                args << QStringLiteral("-filter_complex:a") << QStringLiteral("[0:a%1]aresample=async=100,channelsplit=channel_layout=stereo[0:0][0:1]").arg(audioStream > 0 ? ":" + QString::number(audioStream) : "");
                // Channel 1
                args << QStringLiteral("-map") << QStringLiteral("[0:1]") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("data")<< tmpfile.fileName();
                // Channel 2
                args << QStringLiteral("-map") << QStringLiteral("[0:0]") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("data")<< tmpfile2.fileName();
            } else {
                args << QStringLiteral("-filter_complex:a") << QStringLiteral("[0:a%1]aformat=sample_rates=100,channelsplit=channel_layout=stereo[0:0][0:1]").arg(audioStream > 0 ? ":" + QString::number(audioStream) : "");
                // Channel 1
                args << QStringLiteral("-map") << QStringLiteral("[0:1]") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("s16le")<< tmpfile.fileName();
                // Channel 2
                args << QStringLiteral("-map") << QStringLiteral("[0:0]") << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-y") << QStringLiteral("-f") << QStringLiteral("s16le")<< tmpfile2.fileName();
            }
        }
        emit updateJobStatus(AbstractClipJob::THUMBJOB, JobWaiting, 0);
        QProcess audioThumbsProcess;
        connect(this, SIGNAL(doAbortAudioThumbs()), &audioThumbsProcess, SLOT(kill()));
        audioThumbsProcess.start(KdenliveSettings::ffmpegpath(), args);
        bool ffmpegError = false;
        if (!audioThumbsProcess.waitForStarted()) {
            ffmpegError = true;
        }
        audioThumbsProcess.waitForFinished();
        if (m_abortAudioThumb) {
            emit updateJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0);
            m_abortAudioThumb = false;
            return;
        }
        if (ffmpegError || audioThumbsProcess.exitStatus() == QProcess::CrashExit) {
            emit updateJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0);
            bin()->emitMessage(i18n("Crash in %1 - creating audio thumbnails", KdenliveSettings::ffmpegpath()), ErrorMessage);
            return;
        }
        tmpfile.open();
        QByteArray res = tmpfile.readAll();
        tmpfile.close();
        if (res.size() == 0) {
            emit updateJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0);
            bin()->emitMessage(i18n("Error reading audio thumbnail"), ErrorMessage);
            return;
        }
        const qint16* raw = (const qint16*) res.constData();

        const qint16* raw2;
        QByteArray res2;
        QList<qint16> data2;
        if (channels > 1) {
            tmpfile2.open();
            res2 = tmpfile2.readAll();
            tmpfile2.close();
            raw2 = (const qint16*) res2.constData();
        }
        int progress = 0;
        double offset = (double) res.size() / (2 * lengthInFrames);
        int pos = 0;
        for (int i = 0; i < lengthInFrames; i++) {
            long c1 = 0;
            long c2 = 0;
            pos = (int) (i * offset);
            int steps = 0;
            for (int j = 0; j < (int) offset && (pos + j < res.size()); j++) {
                steps ++;
                c1 += abs(raw[pos + j]);
                if (channels > 1) {
                    c2 += abs(raw2[pos + j]);
                }
            }
            c1 /= steps;
            c1 = c1 * 800 / 32768.0;
            audioLevels << (double) c1;
            if (channels > 1) {
                c2 /= steps;
                c2 = c2 * 800 / 32768.0;
                audioLevels << (double)c2;
            }
            int p = i * 100 / lengthInFrames;
            if (p != progress) {
                emit updateJobStatus(AbstractClipJob::THUMBJOB, JobWorking, p);
                progress = p;
            }
            if (m_abortAudioThumb) break;
        }
    } else {
        QString service = prod->get("mlt_service");
        if (service == QLatin1String("avformat-novalidate"))
        service = QStringLiteral("avformat");
        else if (service.startsWith(QLatin1String("xml")))
            service = QStringLiteral("xml-nogl");
        QScopedPointer <Mlt::Producer> audioProducer(new Mlt::Producer(*prod->profile(), service.toUtf8().constData(), prod->get("resource")));
        if (!audioProducer->is_valid()) {
            return;
        }
        audioProducer->set("video_index", "-1");
        Mlt::Filter chans(*prod->profile(), "audiochannels");
        Mlt::Filter converter(*prod->profile(), "audioconvert");
        Mlt::Filter levels(*prod->profile(), "audiolevel");
        audioProducer->attach(chans);
        audioProducer->attach(converter);
        audioProducer->attach(levels);

        int last_val = 0;
        emit updateJobStatus(AbstractClipJob::THUMBJOB, JobWaiting, 0);
        double framesPerSecond = audioProducer->get_fps();
        mlt_audio_format audioFormat = mlt_audio_s16;
        QStringList keys;
        for (int i = 0; i < channels; i++) {
            keys << "meta.media.audio_level." + QString::number(i);
        }

        int val = 0;
        for (int z = 0;z < lengthInFrames && !m_abortAudioThumb; ++z) {
            val = (int)(100.0 * z / lengthInFrames);
            if (last_val != val) {
                emit updateJobStatus(AbstractClipJob::THUMBJOB, JobWorking, val);
                last_val = val;
            }
            QScopedPointer<Mlt::Frame> mlt_frame(audioProducer->get_frame());
            if (mlt_frame && mlt_frame->is_valid() && !mlt_frame->get_int("test_audio")) {
                int samples = mlt_sample_calculator(framesPerSecond, frequency, z);
                mlt_frame->get_audio(audioFormat, frequency, channels, samples);
                for (int channel = 0; channel < channels; ++channel) {
                    double level = 256 * qMin(mlt_frame->get_double(keys.at(channel).toUtf8().constData()) * 0.9, 1.0);
                    audioLevels << level;
                }
            } else if (!audioLevels.isEmpty()) {
                for (int channel = 0; channel < channels; channel++)
                    audioLevels << audioLevels.last();
            }
            if (m_abortAudioThumb) break;
        }
    }

    emit updateJobStatus(AbstractClipJob::THUMBJOB, JobDone, 0);
    if (!m_abortAudioThumb) {
        updateAudioThumbnail(audioLevels);
    }

    if (!m_abortAudioThumb && audioLevels.size() > 0) {
        // Put into an image for caching.
        int count = audioLevels.size();
        QImage image((count + 3) / 4 / channels, channels, QImage::Format_ARGB32);
        int n = image.width() * image.height();
        for (int i = 0; i < n; i ++) {
            QRgb p; 
            if ((4*i + 3) < count) {
                p = qRgba(audioLevels.at(4*i).toInt(), audioLevels.at(4*i+1).toInt(), audioLevels.at(4*i+2).toInt(), audioLevels.at(4*i+3).toInt());
            } else {
                int last = audioLevels.last().toInt();
                int r = (4*i+0) < count? audioLevels.at(4*i+0).toInt() : last;
                int g = (4*i+1) < count? audioLevels.at(4*i+1).toInt() : last;
                int b = (4*i+2) < count? audioLevels.at(4*i+2).toInt() : last;
                int a = last;
                p = qRgba(r, g, b, a);
            }
            image.setPixel(i / 2, i % channels, p);
        }
        image.save(audioPath);
    }
    m_abortAudioThumb = false;
}

bool ProjectClip::isTransparent() const
{
    if (m_type == Text) return true;
    if (m_type == Image && m_controller->int_property(QStringLiteral("kdenlive:transparency")) == 1) return true;
    return false;
}

QStringList ProjectClip::updatedAnalysisData(const QString &name, const QString &data, int offset)
{
    if (data.isEmpty()) {
        // Remove data
        return QStringList() << QString("kdenlive:clipanalysis." + name) << QString();
        //m_controller->resetProperty("kdenlive:clipanalysis." + name);
    }
    else {
        QString current = m_controller->property("kdenlive:clipanalysis." + name);
        if (!current.isEmpty()) {
            if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("Clip already contains analysis data %1", name), QString(), KGuiItem(i18n("Merge")), KGuiItem(i18n("Add"))) == KMessageBox::Yes) {
                // Merge data
                Mlt::Profile *profile = m_controller->profile();
                Mlt::Geometry geometry(current.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
                Mlt::Geometry newGeometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
                Mlt::GeometryItem item;
                int pos = 0;
                while (!newGeometry.next_key(&item, pos)) {
                    pos = item.frame();
                    item.frame(pos + offset);
                    pos++;
                    geometry.insert(item);
                }
                return QStringList() << QString("kdenlive:clipanalysis." + name) << geometry.serialise();
                //m_controller->setProperty("kdenlive:clipanalysis." + name, geometry.serialise());
            }
            else {
                // Add data with another name
                int i = 1;
                QString data = m_controller->property("kdenlive:clipanalysis." + name + ' ' + QString::number(i));
                while (!data.isEmpty()) {
                    ++i;
                    data = m_controller->property("kdenlive:clipanalysis." + name + ' ' + QString::number(i));
                }
                return QStringList() << QString("kdenlive:clipanalysis." + name + ' ' + QString::number(i)) << geometryWithOffset(data, offset);
                //m_controller->setProperty("kdenlive:clipanalysis." + name + ' ' + QString::number(i), geometryWithOffset(data, offset));
            }
        }
        else {
            return QStringList() << QString("kdenlive:clipanalysis." + name) << geometryWithOffset(data, offset);
            //m_controller->setProperty("kdenlive:clipanalysis." + name, geometryWithOffset(data, offset));
        }
    }
}

QMap <QString, QString> ProjectClip::analysisData(bool withPrefix)
{
    return m_controller->getPropertiesFromPrefix(QStringLiteral("kdenlive:clipanalysis."), withPrefix);
}

const QString ProjectClip::geometryWithOffset(const QString &data, int offset)
{
    if (offset == 0) return data;
    Mlt::Profile *profile = m_controller->profile();
    Mlt::Geometry geometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
    Mlt::Geometry newgeometry(NULL, duration().frames(profile->fps()), profile->width(), profile->height());
    Mlt::GeometryItem item;
    int pos = 0;
    while (!geometry.next_key(&item, pos)) {
        pos = item.frame();
        item.frame(pos + offset);
        pos++;
        newgeometry.insert(item);
    }
    return newgeometry.serialise();
}

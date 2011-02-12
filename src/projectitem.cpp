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


#include "projectitem.h"
#include "timecode.h"
#include "kdenlivesettings.h"
#include "docclipbase.h"

#include <KDebug>
#include <KLocale>
#include <KIcon>

const int DurationRole = Qt::UserRole + 1;
const int ProxyRole = Qt::UserRole + 5;
const int itemHeight = 38;

ProjectItem::ProjectItem(QTreeWidget * parent, DocClipBase *clip) :
        m_clip(clip),
        m_clipId(clip->getId()),
        QTreeWidgetItem(parent, PROJECTCLIPTYPE)
{
    buildItem();
}

ProjectItem::ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip) :
        m_clip(clip),
        m_clipId(clip->getId()),
        QTreeWidgetItem(parent, PROJECTCLIPTYPE)
        
{
    buildItem();
}

void ProjectItem::buildItem()
{
    setSizeHint(0, QSize(itemHeight * 3, itemHeight));
    if (m_clip->isPlaceHolder()) setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    else setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    QString name = m_clip->getProperty("name");
    if (name.isEmpty()) name = KUrl(m_clip->getProperty("resource")).fileName();
    m_clipType = (CLIPTYPE) m_clip->getProperty("type").toInt();
    setText(0, name);
    setText(1, m_clip->description());
    GenTime duration = m_clip->duration();
    if (duration != GenTime()) setData(0, DurationRole, Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps()));
}

ProjectItem::~ProjectItem()
{
}

//static
int ProjectItem::itemDefaultHeight()
{
    return itemHeight;
}

int ProjectItem::numReferences() const
{
    if (!m_clip) return 0;
    return m_clip->numReferences();
}

const QString &ProjectItem::clipId() const
{
    return m_clipId;
}

CLIPTYPE ProjectItem::clipType() const
{
    return m_clipType;
}

int ProjectItem::clipMaxDuration() const
{
    return m_clip->getProperty("duration").toInt();
}

QStringList ProjectItem::names() const
{
    QStringList result;
    result.append(text(0));
    result.append(text(1));
    result.append(text(2));
    return result;
}

QDomElement ProjectItem::toXml() const
{
    return m_clip->toXML();
}

const KUrl ProjectItem::clipUrl() const
{
    if (m_clipType != COLOR && m_clipType != VIRTUAL && m_clipType != UNKNOWN)
        return KUrl(m_clip->getProperty("resource"));
    else return KUrl();
}

void ProjectItem::changeDuration(int frames)
{
    setData(0, DurationRole, Timecode::getEasyTimecode(GenTime(frames, KdenliveSettings::project_fps()), KdenliveSettings::project_fps()));
}

void ProjectItem::setProperties(QMap <QString, QString> props)
{
    if (m_clip == NULL) return;
    m_clip->setProperties(props);
}

QString ProjectItem::getClipHash() const
{
    if (m_clip == NULL) return QString();
    return m_clip->getClipHash();
}

void ProjectItem::setProperty(const QString &key, const QString &value)
{
    if (m_clip == NULL) return;
    m_clip->setProperty(key, value);
}

void ProjectItem::clearProperty(const QString &key)
{
    if (m_clip == NULL) return;
    m_clip->clearProperty(key);
}

DocClipBase *ProjectItem::referencedClip()
{
    return m_clip;
}

void ProjectItem::slotSetToolTip()
{
    QString tip = "<b>";
    if (m_clip->isPlaceHolder()) tip.append(i18n("Missing") + " | ");
    switch (m_clipType) {
    case AUDIO:
        tip.append(i18n("Audio clip") + "</b><br />" + clipUrl().path());
        break;
    case VIDEO:
        tip.append(i18n("Mute video clip") + "</b><br />" + clipUrl().path());
        break;
    case AV:
        tip.append(i18n("Video clip") + "</b><br />" + clipUrl().path());
        break;
    case COLOR:
        tip.append(i18n("Color clip"));
        break;
    case IMAGE:
        tip.append(i18n("Image clip") + "</b><br />" + clipUrl().path());
        break;
    case TEXT:
        if (!clipUrl().isEmpty() && m_clip->getProperty("xmldata").isEmpty()) tip.append(i18n("Template text clip") + "</b><br />" + clipUrl().path());
        else tip.append(i18n("Text clip") + "</b><br />" + clipUrl().path());
        break;
    case SLIDESHOW:
        tip.append(i18n("Slideshow clip") + "</b><br />" + clipUrl().directory());
        break;
    case VIRTUAL:
        tip.append(i18n("Virtual clip"));
        break;
    case PLAYLIST:
        tip.append(i18n("Playlist clip") + "</b><br />" + clipUrl().path());
        break;
    default:
        tip.append(i18n("Unknown clip"));
        break;
    }

    setToolTip(0, tip);
}


void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata)
{
    if (m_clip == NULL) return;
    //setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
    if (attributes.contains("duration")) {
        GenTime duration = GenTime(attributes.value("duration").toInt(), KdenliveSettings::project_fps());
        setData(0, DurationRole, Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps()));
        m_clip->setDuration(duration);
    } else  {
        // No duration known, use an arbitrary one until it is.
    }


    //extend attributes -reh

    if (m_clipType == UNKNOWN) {
        QString cliptype = attributes.value("type");
        if (cliptype == "audio") m_clipType = AUDIO;
        else if (cliptype == "video") m_clipType = VIDEO;
        else if (cliptype == "av") m_clipType = AV;
        else if (cliptype == "playlist") m_clipType = PLAYLIST;
        else m_clipType = AV;

        m_clip->setClipType(m_clipType);
        slotSetToolTip();
    }
    m_clip->setProperties(attributes);
    m_clip->setMetadata(metadata);

    if (m_clip->description().isEmpty()) {
        if (metadata.contains("description")) {
            m_clip->setProperty("description", metadata.value("description"));
            setText(1, m_clip->description());
        } else if (metadata.contains("comment")) {
            m_clip->setProperty("description", metadata.value("comment"));
            setText(1, m_clip->description());
        }
    }
}

void ProjectItem::setProxyStatus(int status)
{
    if (status == data(0, ProxyRole).toInt()) return;
    setData(0, ProxyRole, status);
    if (m_clip && status == 0) m_clip->abortProxy();
}

bool ProjectItem::hasProxy() const
{
    if (m_clip == NULL) return false;
    return !m_clip->getProperty("proxy").isEmpty();
}

bool ProjectItem::isProxyRunning() const
{
     return (data(0, ProxyRole).toInt() == 1);
}


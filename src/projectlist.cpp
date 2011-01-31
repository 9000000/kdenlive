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

#include "projectlist.h"
#include "projectitem.h"
#include "addfoldercommand.h"
#include "kdenlivesettings.h"
#include "slideshowclip.h"
#include "ui_colorclip_ui.h"
#include "titlewidget.h"
#include "definitions.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "kthumb.h"
#include "projectlistview.h"
#include "timecodedisplay.h"
#include "profilesdialog.h"
#include "editclipcommand.h"
#include "editclipcutcommand.h"
#include "editfoldercommand.h"
#include "addclipcutcommand.h"

#include "ui_templateclip_ui.h"

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>
#include <KIO/NetAccess>
#include <KFileItem>
#include <KApplication>
#ifdef NEPOMUK
#include <nepomuk/global.h>
#include <nepomuk/resourcemanager.h>
//#include <nepomuk/tag.h>
#endif

#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QHeaderView>
#include <QInputDialog>
#include <QtConcurrentRun>

ProjectList::ProjectList(QWidget *parent) :
    QWidget(parent),
    m_render(NULL),
    m_fps(-1),
    m_commandStack(NULL),
    m_openAction(NULL),
    m_reloadAction(NULL),
    m_transcodeAction(NULL),
    m_doc(NULL),
    m_refreshed(false),
    m_infoQueue(),
    m_thumbnailQueue()
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    qRegisterMetaType<QDomElement>("QDomElement");
    // setup toolbar
    QFrame *frame = new QFrame;
    frame->setFrameStyle(QFrame::NoFrame);
    QHBoxLayout *box = new QHBoxLayout;
    KTreeWidgetSearchLine *searchView = new KTreeWidgetSearchLine;

    box->addWidget(searchView);
    //int s = style()->pixelMetric(QStyle::PM_SmallIconSize);
    //m_toolbar->setIconSize(QSize(s, s));

    m_addButton = new QToolButton;
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_addButton->setAutoRaise(true);
    box->addWidget(m_addButton);

    m_editButton = new QToolButton;
    m_editButton->setAutoRaise(true);
    box->addWidget(m_editButton);

    m_deleteButton = new QToolButton;
    m_deleteButton->setAutoRaise(true);
    box->addWidget(m_deleteButton);
    frame->setLayout(box);
    layout->addWidget(frame);

    m_listView = new ProjectListView;
    layout->addWidget(m_listView);
    setLayout(layout);
    searchView->setTreeWidget(m_listView);

    m_proxyAction = new QAction(i18n("Proxy clip"), this);
    m_proxyAction->setCheckable(true);
    m_proxyAction->setChecked(false);
    connect(m_proxyAction, SIGNAL(toggled(bool)), this, SLOT(slotProxyCurrentItem(bool)));
    connect(this, SIGNAL(processNextThumbnail()), this, SLOT(slotProcessNextThumbnail()));
    connect(m_listView, SIGNAL(projectModified()), this, SIGNAL(projectModified()));
    connect(m_listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(focusMonitor()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(pauseMonitor()), this, SLOT(slotPauseMonitor()));
    connect(m_listView, SIGNAL(requestMenu(const QPoint &, QTreeWidgetItem *)), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
    connect(m_listView, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_listView, SIGNAL(addClip(const QList <QUrl>, const QString &, const QString &)), this, SLOT(slotAddClip(const QList <QUrl>, const QString &, const QString &)));
    connect(m_listView, SIGNAL(addClipCut(const QString &, int, int)), this, SLOT(slotAddClipCut(const QString &, int, int)));
    connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));
    connect(m_listView, SIGNAL(showProperties(DocClipBase *)), this, SIGNAL(showClipProperties(DocClipBase *)));

    m_listViewDelegate = new ItemDelegate(m_listView);
    m_listView->setItemDelegate(m_listViewDelegate);
#ifdef NEPOMUK
    if (KdenliveSettings::activate_nepomuk()) {
        Nepomuk::ResourceManager::instance()->init();
        if (!Nepomuk::ResourceManager::instance()->initialized()) {
            kDebug() << "Cannot communicate with Nepomuk, DISABLING it";
            KdenliveSettings::setActivate_nepomuk(false);
        }
    }
#endif
}

ProjectList::~ProjectList()
{
    delete m_menu;
    m_listView->blockSignals(true);
    m_listView->clear();
    delete m_listViewDelegate;
}

void ProjectList::focusTree() const
{
    m_listView->setFocus();
}

void ProjectList::setupMenu(QMenu *addMenu, QAction *defaultAction)
{
    QList <QAction *> actions = addMenu->actions();
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toString() == "clip_properties") {
            m_editButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "delete_clip") {
            m_deleteButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "edit_clip") {
            m_openAction = actions.at(i);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "reload_clip") {
            m_reloadAction = actions.at(i);
            actions.removeAt(i);
            i--;
        }
    }

    QMenu *m = new QMenu();
    m->addActions(actions);
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_menu = new QMenu();
    m_menu->addActions(addMenu->actions());
}

void ProjectList::setupGeneratorMenu(QMenu *addMenu, QMenu *transcodeMenu, QMenu *inTimelineMenu)
{
    if (!addMenu)
        return;
    QMenu *menu = m_addButton->menu();
    menu->addMenu(addMenu);
    m_addButton->setMenu(menu);

    m_menu->addMenu(addMenu);
    if (addMenu->isEmpty())
        addMenu->setEnabled(false);
    m_menu->addMenu(transcodeMenu);
    if (transcodeMenu->isEmpty())
        transcodeMenu->setEnabled(false);
    m_transcodeAction = transcodeMenu;
    m_menu->addAction(m_reloadAction);
    m_menu->addAction(m_proxyAction);
    m_menu->addMenu(inTimelineMenu);
    inTimelineMenu->setEnabled(false);
    m_menu->addAction(m_editButton->defaultAction());
    m_menu->addAction(m_openAction);
    m_menu->addAction(m_deleteButton->defaultAction());
    m_menu->insertSeparator(m_deleteButton->defaultAction());
}


QByteArray ProjectList::headerInfo() const
{
    return m_listView->header()->saveState();
}

void ProjectList::setHeaderInfo(const QByteArray &state)
{
    m_listView->header()->restoreState(state);
}

void ProjectList::updateProjectFormat(Timecode t)
{
    m_timecode = t;
}

void ProjectList::slotEditClip()
{
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    if (list.isEmpty()) return;
    if (list.count() > 1 || list.at(0)->type() == PROJECTFOLDERTYPE) {
        editClipSelection(list);
        return;
    }
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE)
        return;
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE)
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    else
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && (item->flags() & Qt::ItemIsDragEnabled)) {
        emit clipSelected(item->referencedClip());
        emit showClipProperties(item->referencedClip());
    }
}

void ProjectList::editClipSelection(QList<QTreeWidgetItem *> list)
{
    // Gather all common properties
    QMap <QString, QString> commonproperties;
    QList <DocClipBase *> clipList;
    commonproperties.insert("force_aspect_num", "-");
    commonproperties.insert("force_aspect_den", "-");
    commonproperties.insert("force_fps", "-");
    commonproperties.insert("force_progressive", "-");
    commonproperties.insert("force_tff", "-");
    commonproperties.insert("threads", "-");
    commonproperties.insert("video_index", "-");
    commonproperties.insert("audio_index", "-");
    commonproperties.insert("force_colorspace", "-");
    commonproperties.insert("full_luma", "-");

    bool allowDurationChange = true;
    int commonDuration = -1;
    ProjectItem *item;
    for (int i = 0; i < list.count(); i++) {
        item = NULL;
        if (list.at(i)->type() == PROJECTFOLDERTYPE) {
            // Add folder items to the list
            int ct = list.at(i)->childCount();
            for (int j = 0; j < ct; j++) {
                list.append(list.at(i)->child(j));
            }
            continue;
        }
        else if (list.at(i)->type() == PROJECTSUBCLIPTYPE)
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        else
            item = static_cast <ProjectItem*>(list.at(i));
        if (!(item->flags() & Qt::ItemIsDragEnabled))
            continue;
        if (item) {
            // check properties
            DocClipBase *clip = item->referencedClip();
            if (clipList.contains(clip)) continue;
            if (clip->clipType() != COLOR && clip->clipType() != IMAGE && clip->clipType() != TEXT)
                allowDurationChange = false;
            if (allowDurationChange && commonDuration != 0) {
                if (commonDuration == -1)
                    commonDuration = clip->duration().frames(m_fps);
                else if (commonDuration != clip->duration().frames(m_fps))
                    commonDuration = 0;
            }
            clipList.append(clip);
            QMap <QString, QString> clipprops = clip->properties();
            QMapIterator<QString, QString> p(commonproperties);
            while (p.hasNext()) {
                p.next();
                if (p.value().isEmpty()) continue;
                if (clipprops.contains(p.key())) {
                    if (p.value() == "-")
                        commonproperties.insert(p.key(), clipprops.value(p.key()));
                    else if (p.value() != clipprops.value(p.key()))
                        commonproperties.insert(p.key(), QString());
                } else {
                    commonproperties.insert(p.key(), QString());
                }
            }
        }
    }
    if (allowDurationChange)
        commonproperties.insert("out", QString::number(commonDuration));
    /*QMapIterator<QString, QString> p(commonproperties);
    while (p.hasNext()) {
        p.next();
        kDebug() << "Result: " << p.key() << " = " << p.value();
    }*/
    emit showClipProperties(clipList, commonproperties);
}

void ProjectList::slotOpenClip()
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE)
        return;
    if (m_listView->currentItem()->type() == QTreeWidgetItem::UserType + 1)
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    else
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item) {
        if (item->clipType() == IMAGE) {
            if (KdenliveSettings::defaultimageapp().isEmpty())
                KMessageBox::sorry(kapp->activeWindow(), i18n("Please set a default application to open images in the Settings dialog"));
            else
                QProcess::startDetached(KdenliveSettings::defaultimageapp(), QStringList() << item->clipUrl().path());
        }
        if (item->clipType() == AUDIO) {
            if (KdenliveSettings::defaultaudioapp().isEmpty())
                KMessageBox::sorry(kapp->activeWindow(), i18n("Please set a default application to open audio files in the Settings dialog"));
            else
                QProcess::startDetached(KdenliveSettings::defaultaudioapp(), QStringList() << item->clipUrl().path());
        }
    }
}

void ProjectList::cleanup()
{
    m_listView->clearSelection();
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0)
            item->setSelected(true);
        it++;
    }
    slotRemoveClip();
}

void ProjectList::trashUnusedClips()
{
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    QStringList ids;
    QStringList urls;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0) {
            ids << item->clipId();
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && !urls.contains(url.path()))
                urls << url.path();
        }
        it++;
    }

    // Check that we don't use the URL in another clip
    QTreeWidgetItemIterator it2(m_listView);
    while (*it2) {
        if ((*it2)->type() != PROJECTCLIPTYPE) {
            it2++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it2);
        if (item->numReferences() > 0) {
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && urls.contains(url.path())) urls.removeAll(url.path());
        }
        it2++;
    }

    emit deleteProjectClips(ids, QMap <QString, QString>());
    for (int i = 0; i < urls.count(); i++)
        KIO::NetAccess::del(KUrl(urls.at(i)), this);
}

void ProjectList::slotReloadClip(const QString &id)
{
    QList<QTreeWidgetItem *> selected;
    if (id.isEmpty())
        selected = m_listView->selectedItems();
    else {
        ProjectItem *itemToReLoad = getItemById(id);
        if (itemToReLoad) selected.append(itemToReLoad);
    }
    ProjectItem *item;
    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() != PROJECTCLIPTYPE) {
            if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
                for (int j = 0; j < selected.at(i)->childCount(); j++)
                    selected.append(selected.at(i)->child(j));
            }
            continue;
        }
        item = static_cast <ProjectItem *>(selected.at(i));
        if (item) {
            CLIPTYPE t = item->clipType();
            if (t == TEXT) {
                if (!item->referencedClip()->getProperty("xmltemplate").isEmpty())
                    regenerateTemplate(item);
            } else if (t != COLOR && t != SLIDESHOW && item->referencedClip() &&  item->referencedClip()->checkHash() == false) {
                item->referencedClip()->setPlaceHolder(true);
                item->setProperty("file_hash", QString());
            } else if (t == IMAGE) {
                item->referencedClip()->producer()->set("force_reload", 1);
            }

            QDomElement e = item->toXml();
            // Make sure we get the correct producer length if it was adjusted in timeline
            if (t == COLOR || t == IMAGE || t == SLIDESHOW || t == TEXT) {
                int length = QString(item->referencedClip()->producerProperty("length")).toInt();
                if (length > 0 && !e.hasAttribute("length")) {
                    e.setAttribute("length", length);
                    e.setAttribute("out", length - 1);
                }
            }
            
            emit getFileProperties(e, item->clipId(), m_listView->iconSize().height(), true, false);
        }
    }
}

void ProjectList::slotModifiedClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        QPixmap pixmap = qVariantValue<QPixmap>(item->data(0, Qt::DecorationRole));
        if (!pixmap.isNull()) {
            QPainter p(&pixmap);
            p.fillRect(0, 0, pixmap.width(), pixmap.height(), QColor(255, 255, 255, 200));
            p.drawPixmap(0, 0, KIcon("view-refresh").pixmap(m_listView->iconSize()));
            p.end();
        } else {
            pixmap = KIcon("view-refresh").pixmap(m_listView->iconSize());
        }
        item->setData(0, Qt::DecorationRole, pixmap);
    }
}

void ProjectList::slotMissingClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
        if (item->referencedClip()) {
            item->referencedClip()->setPlaceHolder(true);
            if (m_render == NULL) kDebug() << "*********  ERROR, NULL RENDR";
            item->referencedClip()->setProducer(m_render->invalidProducer(id), true);
            item->slotSetToolTip();
            emit clipNeedsReload(id, true);
        }
    }
    update();
    emit displayMessage(i18n("Check missing clips"), -2);
    emit updateRenderStatus();
}

void ProjectList::slotAvailableClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item == NULL)
        return;
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    if (item->referencedClip()) { // && item->referencedClip()->checkHash() == false) {
        item->setProperty("file_hash", QString());
        slotReloadClip(id);
    }
    /*else {
    item->referencedClip()->setValid();
    item->slotSetToolTip();
    }
    update();*/
    emit updateRenderStatus();
}

bool ProjectList::hasMissingClips()
{
    bool missing = false;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTCLIPTYPE && !((*it)->flags() & Qt::ItemIsDragEnabled)) {
            missing = true;
            break;
        }
        it++;
    }
    return missing;
}

void ProjectList::setRenderer(Render *projectRender)
{
    m_render = projectRender;
    m_listView->setIconSize(QSize((ProjectItem::itemDefaultHeight() - 2) * m_render->dar(), ProjectItem::itemDefaultHeight() - 2));
}

void ProjectList::slotClipSelected()
{
    if (!m_listView->isEnabled()) return;
    if (m_listView->currentItem()) {
        if (m_listView->currentItem()->type() == PROJECTFOLDERTYPE) {
            emit clipSelected(NULL);
            m_editButton->defaultAction()->setEnabled(m_listView->currentItem()->childCount() > 0);
            m_deleteButton->defaultAction()->setEnabled(true);
            m_openAction->setEnabled(false);
            m_reloadAction->setEnabled(false);
            m_transcodeAction->setEnabled(false);
            m_proxyAction->setEnabled(false);
        } else {
            ProjectItem *clip;
            if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
                // this is a sub item, use base clip
                m_deleteButton->defaultAction()->setEnabled(true);
                clip = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
                if (clip == NULL) kDebug() << "-----------ERROR";
                SubProjectItem *sub = static_cast <SubProjectItem*>(m_listView->currentItem());
                emit clipSelected(clip->referencedClip(), sub->zone());
                m_transcodeAction->setEnabled(false);
                return;
            }
            clip = static_cast <ProjectItem*>(m_listView->currentItem());
            if (clip && clip->referencedClip())
                emit clipSelected(clip->referencedClip());
            m_editButton->defaultAction()->setEnabled(true);
            m_deleteButton->defaultAction()->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
                m_openAction->setEnabled(true);
            } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
                m_openAction->setEnabled(true);
            } else {
                m_openAction->setEnabled(false);
            }
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        }
    } else {
        emit clipSelected(NULL);
        m_editButton->defaultAction()->setEnabled(false);
        m_deleteButton->defaultAction()->setEnabled(false);
        m_openAction->setEnabled(false);
        m_reloadAction->setEnabled(false);
        m_transcodeAction->setEnabled(false);
    }
}

void ProjectList::adjustProxyActions(ProjectItem *clip) const
{
    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == PLAYLIST || clip->clipType() == SLIDESHOW || clip->clipType() == AUDIO) {
        m_proxyAction->setEnabled(false);
        return;
    }
    m_proxyAction->setEnabled(true);
    m_proxyAction->blockSignals(true);
    m_proxyAction->setChecked(clip->hasProxy());
    m_proxyAction->blockSignals(false);
}

void ProjectList::adjustTranscodeActions(ProjectItem *clip) const
{
    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == PLAYLIST || clip->clipType() == SLIDESHOW) {
        m_transcodeAction->setEnabled(false);
        return;
    }
    m_transcodeAction->setEnabled(true);
    QList<QAction *> transcodeActions = m_transcodeAction->actions();
    QStringList data;
    QString condition;
    for (int i = 0; i < transcodeActions.count(); i++) {
        data = transcodeActions.at(i)->data().toStringList();
        if (data.count() > 2) {
            condition = data.at(2);
            if (condition.startsWith("vcodec"))
                transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section('=', 1, 1)));
            else if (condition.startsWith("acodec"))
                transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section('=', 1, 1)));
        }
    }

}

void ProjectList::slotPauseMonitor()
{
    if (m_render)
        m_render->pause();
}

void ProjectList::slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        slotUpdateClipProperties(item, properties);
        if (properties.contains("out") || properties.contains("force_fps") || properties.contains("resource")) {
            slotReloadClip(id);
        } else if (properties.contains("colour") ||
                   properties.contains("xmldata") ||
                   properties.contains("force_aspect_num") ||
                   properties.contains("force_aspect_den") ||
                   properties.contains("templatetext")) {
            slotRefreshClipThumbnail(item);
            emit refreshClip(id, true);
        } else if (properties.contains("full_luma") || properties.contains("force_colorspace") || properties.contains("loop")) {
            emit refreshClip(id, false);
        }
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties)
{
    if (!clip)
        return;
    clip->setProperties(properties);
    if (properties.contains("name")) {
        monitorItemEditing(false);
        clip->setText(0, properties.value("name"));
        monitorItemEditing(true);
        emit clipNameChanged(clip->clipId(), properties.value("name"));
    }
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        monitorItemEditing(false);
        clip->setText(1, properties.value("description"));
        monitorItemEditing(true);
#ifdef NEPOMUK
        if (KdenliveSettings::activate_nepomuk() && (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST)) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            f.setDescription(properties.value("description"));
        }
#endif
        emit projectModified();
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column)
{
    if (item->type() == PROJECTSUBCLIPTYPE) {
        // this is a sub-item
        if (column == 1) {
            // user edited description
            SubProjectItem *sub = static_cast <SubProjectItem*>(item);
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), sub->zone(), sub->description(), sub->text(1), true);
            m_commandStack->push(command);
            //slotUpdateCutClipProperties(sub->clipId(), sub->zone(), sub->text(1), sub->text(1));
        }
        return;
    }
    if (item->type() == PROJECTFOLDERTYPE) {
        if (column == 0) {
            FolderProjectItem *folder = static_cast <FolderProjectItem*>(item);
            editFolder(item->text(0), folder->groupName(), folder->clipId());
            folder->setGroupName(item->text(0));
            m_doc->clipManager()->addFolder(folder->clipId(), item->text(0));
            const int children = item->childCount();
            for (int i = 0; i < children; i++) {
                ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                child->setProperty("groupname", item->text(0));
            }
        }
        return;
    }

    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 1) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["description"] = clip->referencedClip()->getProperty("description");
            newprops["description"] = item->text(1);

            if (clip->clipType() == TEXT) {
                // This is a text template clip, update the image
                /*oldprops.insert("xmldata", clip->referencedClip()->getProperty("xmldata"));
                newprops.insert("xmldata", generateTemplateXml(clip->referencedClip()->getProperty("xmltemplate"), item->text(2)).toString());*/
                oldprops.insert("templatetext", clip->referencedClip()->getProperty("templatetext"));
                newprops.insert("templatetext", item->text(1));
            }
            slotUpdateClipProperties(clip->clipId(), newprops);
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
        }
    } else if (column == 0) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["name"] = clip->referencedClip()->getProperty("name");
            newprops["name"] = item->text(0);
            slotUpdateClipProperties(clip, newprops);
            emit projectModified();
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
        }
    }
}

void ProjectList::slotContextMenu(const QPoint &pos, QTreeWidgetItem *item)
{
    bool enable = item ? true : false;
    m_editButton->defaultAction()->setEnabled(enable);
    m_deleteButton->defaultAction()->setEnabled(enable);
    m_reloadAction->setEnabled(enable);
    m_transcodeAction->setEnabled(enable);
    if (enable) {
        ProjectItem *clip = NULL;
        if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item->parent());
            m_transcodeAction->setEnabled(false);
        } else if (m_listView->currentItem()->type() == PROJECTCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item);
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            adjustProxyActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        } else {
            m_transcodeAction->setEnabled(false);
        }
        if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else {
            m_openAction->setEnabled(false);
        }

    } else {
        m_openAction->setEnabled(false);
    }
    m_menu->popup(pos);
}

void ProjectList::slotRemoveClip()
{
    if (!m_listView->currentItem())
        return;
    QStringList ids;
    QMap <QString, QString> folderids;
    QList<QTreeWidgetItem *> selected = m_listView->selectedItems();

    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Clip Zone"));
    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(selected.at(i));
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            new AddClipCutCommand(this, item->clipId(), sub->zone().x(), sub->zone().y(), sub->description(), false, true, delCommand);
        } else if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
            // folder
            FolderProjectItem *folder = static_cast <FolderProjectItem *>(selected.at(i));
            folderids[folder->groupName()] = folder->clipId();
            int children = folder->childCount();

            if (children > 0 && KMessageBox::questionYesNo(kapp->activeWindow(), i18np("Delete folder <b>%2</b>?<br />This will also remove the clip in that folder", "Delete folder <b>%2</b>?<br />This will also remove the %1 clips in that folder",  children, folder->text(1)), i18n("Delete Folder")) != KMessageBox::Yes)
                return;
            for (int i = 0; i < children; ++i) {
                ProjectItem *child = static_cast <ProjectItem *>(folder->child(i));
                ids << child->clipId();
            }
        } else {
            ProjectItem *item = static_cast <ProjectItem *>(selected.at(i));
            ids << item->clipId();
            if (item->numReferences() > 0 && KMessageBox::questionYesNo(kapp->activeWindow(), i18np("Delete clip <b>%2</b>?<br />This will also remove the clip in timeline", "Delete clip <b>%2</b>?<br />This will also remove its %1 clips in timeline", item->numReferences(), item->names().at(1)), i18n("Delete Clip"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "DeleteAll") == KMessageBox::No) {
                KMessageBox::enableMessage("DeleteAll");
                return;
            }
        }
    }
    KMessageBox::enableMessage("DeleteAll");
    if (delCommand->childCount() == 0)
        delete delCommand;
    else
        m_commandStack->push(delCommand);
    emit deleteProjectClips(ids, folderids);
}

void ProjectList::updateButtons() const
{
    if (m_listView->topLevelItemCount() == 0) {
        m_deleteButton->defaultAction()->setEnabled(false);
        m_editButton->defaultAction()->setEnabled(false);
    } else {
        m_deleteButton->defaultAction()->setEnabled(true);
        if (!m_listView->currentItem())
            m_listView->setCurrentItem(m_listView->topLevelItem(0));
        QTreeWidgetItem *item = m_listView->currentItem();
        if (item && item->type() == PROJECTCLIPTYPE) {
            m_editButton->defaultAction()->setEnabled(true);
            m_openAction->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            m_proxyAction->setEnabled(true);
            return;
        }
        else if (item && item->type() == PROJECTFOLDERTYPE && item->childCount() > 0) {
            m_editButton->defaultAction()->setEnabled(true);
        }
        else m_editButton->defaultAction()->setEnabled(false);
    }
    m_openAction->setEnabled(false);
    m_reloadAction->setEnabled(false);
    m_transcodeAction->setEnabled(false);
    m_proxyAction->setEnabled(false);
}

void ProjectList::selectItemById(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (item)
        m_listView->setCurrentItem(item);
}


void ProjectList::slotDeleteClip(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (!item) {
        kDebug() << "/// Cannot find clip to delete";
        return;
    }
    m_listView->blockSignals(true);
    QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
    if (!newSelectedItem)
        newSelectedItem = m_listView->itemBelow(item);
    delete item;
    m_doc->clipManager()->deleteClip(clipId);
    m_listView->blockSignals(false);
    if (newSelectedItem) {
        m_listView->setCurrentItem(newSelectedItem);
    } else {
        updateButtons();
        emit clipSelected(NULL);
    }
}


void ProjectList::editFolder(const QString folderName, const QString oldfolderName, const QString &clipId)
{
    EditFolderCommand *command = new EditFolderCommand(this, folderName, oldfolderName, clipId, false);
    m_commandStack->push(command);
    m_doc->setModified(true);
}

void ProjectList::slotAddFolder()
{
    AddFolderCommand *command = new AddFolderCommand(this, i18n("Folder"), QString::number(m_doc->clipManager()->getFreeFolderId()), true);
    m_commandStack->push(command);
}

void ProjectList::slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit)
{
    if (remove) {
        FolderProjectItem *item = getFolderItemById(clipId);
        if (item) {
            m_doc->clipManager()->deleteFolder(clipId);
            QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
            if (!newSelectedItem)
                newSelectedItem = m_listView->itemBelow(item);
            delete item;
            if (newSelectedItem)
                m_listView->setCurrentItem(newSelectedItem);
            else
                updateButtons();
        }
    } else {
        if (edit) {
            FolderProjectItem *item = getFolderItemById(clipId);
            if (item) {
                m_listView->blockSignals(true);
                item->setGroupName(foldername);
                m_listView->blockSignals(false);
                m_doc->clipManager()->addFolder(clipId, foldername);
                const int children = item->childCount();
                for (int i = 0; i < children; i++) {
                    ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                    child->setProperty("groupname", foldername);
                }
            }
        } else {
            m_listView->blockSignals(true);
            m_listView->setCurrentItem(new FolderProjectItem(m_listView, QStringList() << foldername, clipId));
            m_doc->clipManager()->addFolder(clipId, foldername);
            m_listView->blockSignals(false);
            m_listView->editItem(m_listView->currentItem(), 0);
        }
        updateButtons();
    }
    m_doc->setModified(true);
}



void ProjectList::deleteProjectFolder(QMap <QString, QString> map)
{
    QMapIterator<QString, QString> i(map);
    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Folder"));
    while (i.hasNext()) {
        i.next();
        new AddFolderCommand(this, i.key(), i.value(), false, delCommand);
    }
    m_commandStack->push(delCommand);
}

void ProjectList::slotAddClip(DocClipBase *clip, bool getProperties)
{
    m_listView->setEnabled(false);
    const QString parent = clip->getProperty("groupid");
    ProjectItem *item = NULL;
    monitorItemEditing(false);
    if (!parent.isEmpty()) {
        FolderProjectItem *parentitem = getFolderItemById(parent);
        if (!parentitem) {
            QStringList text;
            QString groupName = clip->getProperty("groupname");
            //kDebug() << "Adding clip to new group: " << groupName;
            if (groupName.isEmpty()) groupName = i18n("Folder");
            text << groupName;
            parentitem = new FolderProjectItem(m_listView, text, parent);
        }

        if (parentitem)
            item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL)
        item = new ProjectItem(m_listView, clip);
    if (item->data(0, DurationRole).isNull()) item->setData(0, DurationRole, i18n("Loading"));
    if (getProperties) {
        m_refreshed = false;
        // Proxy clips
        CLIPTYPE t = clip->clipType();
        if ((t == VIDEO || t == AV || t == UNKNOWN) && KdenliveSettings::enableproxy()) {
            if (clip->getProperty("proxy").isEmpty()) {
                connect(clip, SIGNAL(proxyReady(const QString, bool)), this, SLOT(slotGotProxy(const QString, bool)));
                setProxyStatus(item, 1);
                clip->generateProxy(m_doc->projectFolder());
            }
            else {
                // Proxy clip already created
                setProxyStatus(item, 2);
                QDomElement e = clip->toXML().cloneNode().toElement();
                e.removeAttribute("file_hash");
                m_infoQueue.insert(clip->getId(), e);
            }
        }
        else {
            // We don't use proxies
            // remove file_hash so that we load all properties for the clip
            QDomElement e = clip->toXML().cloneNode().toElement();
            e.removeAttribute("file_hash");
            m_infoQueue.insert(clip->getId(), e);
        }
        //m_render->getFileProperties(clip->toXML(), clip->getId(), true);
    }
    else if (!clip->getProperty("proxy").isEmpty()) {
        connect(clip, SIGNAL(proxyReady(const QString, bool)), this, SLOT(slotGotProxy(const QString, bool)));
        setProxyStatus(item, 1);
        clip->generateProxy(m_doc->projectFolder());
    }
    clip->askForAudioThumbs();
    
    KUrl url = clip->fileURL();
    if (getProperties == false && !clip->getClipHash().isEmpty()) {
        QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + ".png";
        if (QFile::exists(cachedPixmap)) {
            QPixmap pix(cachedPixmap);
            if (pix.isNull())
                KIO::NetAccess::del(KUrl(cachedPixmap), this);
            item->setData(0, Qt::DecorationRole, pix);
        }
    }
#ifdef NEPOMUK
    if (!url.isEmpty() && KdenliveSettings::activate_nepomuk()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation = f.description();
        if (!annotation.isEmpty()) item->setText(1, annotation);
        item->setText(2, QString::number(f.rating()));
    }
#endif
    // Add cut zones
    QList <CutZoneInfo> cuts = clip->cutZones();
    if (!cuts.isEmpty()) {
        for (int i = 0; i < cuts.count(); i++) {
            SubProjectItem *sub = new SubProjectItem(item, cuts.at(i).zone.x(), cuts.at(i).zone.y(), cuts.at(i).description);
            if (!clip->getClipHash().isEmpty()) {
                QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + '#' + QString::number(cuts.at(i).zone.x()) + ".png";
                if (QFile::exists(cachedPixmap)) {
                    QPixmap pix(cachedPixmap);
                    if (pix.isNull())
                        KIO::NetAccess::del(KUrl(cachedPixmap), this);
                    sub->setData(0, Qt::DecorationRole, pix);
                }
            }
        }
    }
    monitorItemEditing(true);
    if (m_listView->isEnabled()) {
        updateButtons();
    }
    
    if (getProperties && m_processingClips.isEmpty())
        m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
}

void ProjectList::slotGotProxy(const QString id, bool success)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        if (success) {
            // Proxy clip successfully created
            setProxyStatus(item, 2);
            QDomElement e = item->referencedClip()->toXML().cloneNode().toElement();  
            e.removeAttribute("file_hash");
            e.setAttribute("replace", 1);
            m_infoQueue.insert(id, e);
            if (!m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
        }
        else setProxyStatus(item, 0);
        update();
    }
}

void ProjectList::slotResetProjectList()
{
    m_listView->clear();
    emit clipSelected(NULL);
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
}

void ProjectList::requestClipInfo(const QDomElement xml, const QString id)
{
    m_refreshed = false;
    m_infoQueue.insert(id, xml);
    //if (m_infoQueue.count() == 1 || ) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotProcessNextClipInQueue()
{
    if (m_infoQueue.isEmpty()) {
        emit processNextThumbnail();
        return;
    }

    QMap<QString, QDomElement>::const_iterator j = m_infoQueue.constBegin();
    if (j != m_infoQueue.constEnd()) {
        QDomElement dom = j.value();
        const QString id = j.key();
        m_infoQueue.remove(id);
        m_processingClips.append(id);
        bool replace;
        if (dom.hasAttribute("replace")) {
            // Proxy action was enabled / disabled and we want to replace current producer
            dom.removeAttribute("replace");
            replace = true;
        }
        else replace = false;
        bool selectClip = !replace;
        if (m_infoQueue.count() > 1) selectClip = false;
        emit getFileProperties(dom, id, m_listView->iconSize().height(), replace, selectClip);
    }
}

void ProjectList::slotUpdateClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    monitorItemEditing(false);
    if (item) item->setData(0, UsageRole, QString::number(item->numReferences()));
    monitorItemEditing(true);
}

void ProjectList::updateAllClips()
{
    m_listView->setSortingEnabled(false);
    kDebug() << "// UPDATE ALL CLPY";

    QTreeWidgetItemIterator it(m_listView);
    DocClipBase *clip;
    ProjectItem *item;
    monitorItemEditing(false);
    while (*it) {
        if ((*it)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(*it);
            if (sub->data(0, Qt::DecorationRole).isNull()) {
                item = static_cast <ProjectItem *>((*it)->parent());
                requestClipThumbnail(item->clipId() + '#' + QString::number(sub->zone().x()));
            }
            ++it;
            continue;
        } else if ((*it)->type() == PROJECTFOLDERTYPE) {
            // folder
            ++it;
            continue;
        } else {
            item = static_cast <ProjectItem *>(*it);
            clip = item->referencedClip();
            if (item->referencedClip()->producer() == NULL) {
                if (clip->isPlaceHolder() == false)
                    requestClipInfo(clip->toXML(), clip->getId());
                else if (!clip->isPlaceHolder())
                    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
            } else {
                if (item->data(0, Qt::DecorationRole).isNull())
                    requestClipThumbnail(clip->getId());
                if (item->data(0, DurationRole).toString().isEmpty())
                    item->changeDuration(item->referencedClip()->producer()->get_playtime());
            }
            item->setData(0, UsageRole, QString::number(item->numReferences()));
        }
        //qApp->processEvents();
        ++it;
    }
    /*if (!m_queueTimer.isActive())
        m_queueTimer.start();*/
    if (!m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
    if (m_listView->isEnabled())
        monitorItemEditing(true);
    m_listView->setSortingEnabled(true);
    if (m_infoQueue.isEmpty())
        slotProcessNextThumbnail();
}

// static
QString ProjectList::getExtensions()
{
    // Build list of mime types
    QStringList mimeTypes = QStringList() << "application/x-kdenlive" << "application/x-kdenlivetitle" << "video/mlt-playlist" << "text/plain"
                            << "video/x-flv" << "application/vnd.rn-realmedia" << "video/x-dv" << "video/dv" << "video/x-msvideo" << "video/x-matroska" << "video/mpeg" << "video/ogg" << "video/x-ms-wmv" << "video/mp4" << "video/quicktime" << "video/webm"
                            << "audio/x-flac" << "audio/x-matroska" << "audio/mp4" << "audio/mpeg" << "audio/x-mp3" << "audio/ogg" << "audio/x-wav" << "application/ogg" << "application/mxf" << "application/x-shockwave-flash"
                            << "image/gif" << "image/jpeg" << "image/png" << "image/x-tga" << "image/x-bmp" << "image/svg+xml" << "image/tiff" << "image/x-xcf" << "image/x-xcf-gimp" << "image/x-vnd.adobe.photoshop" << "image/x-pcx" << "image/x-exr";

    QString allExtensions;
    foreach(const QString & mimeType, mimeTypes) {
        KMimeType::Ptr mime(KMimeType::mimeType(mimeType));
        if (mime) {
            allExtensions.append(mime->patterns().join(" "));
            allExtensions.append(' ');
        }
    }
    return allExtensions.simplified();
}

void ProjectList::slotAddClip(const QList <QUrl> givenList, const QString &groupName, const QString &groupId)
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    KUrl::List list;
    if (givenList.isEmpty()) {
        QString allExtensions = getExtensions();
        const QString dialogFilter = allExtensions + ' ' + QLatin1Char('|') + i18n("All Supported Files") + "\n* " + QLatin1Char('|') + i18n("All Files");
        QCheckBox *b = new QCheckBox(i18n("Import image sequence"));
        b->setChecked(KdenliveSettings::autoimagesequence());
        KFileDialog *d = new KFileDialog(KUrl("kfiledialog:///clipfolder"), dialogFilter, kapp->activeWindow(), b);
        d->setOperationMode(KFileDialog::Opening);
        d->setMode(KFile::Files);
        d->exec();
        list = d->selectedUrls();
        if (b->isChecked() && list.count() == 1) {
            // Check for image sequence
            KUrl url = list.at(0);
            QString fileName = url.fileName().section('.', 0, -2);
            if (fileName.at(fileName.size() - 1).isDigit()) {
                KFileItem item(KFileItem::Unknown, KFileItem::Unknown, url);
                if (item.mimetype().startsWith("image")) {
                    // import as sequence if we found more than one image in the sequence
                    QStringList list;
                    QString pattern = SlideshowClip::selectedPath(url.path(), false, QString(), &list);
                    int count = list.count();
                    if (count > 1) {
                        delete d;
                        QStringList groupInfo = getGroup();

                        // get image sequence base name
                        while (fileName.at(fileName.size() - 1).isDigit()) {
                            fileName.chop(1);
                        }

                        m_doc->slotCreateSlideshowClipFile(fileName, pattern, count, m_timecode.reformatSeparators(KdenliveSettings::sequence_duration()),
                                                           false, false, false,
                                                           m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))), QString(), 0,
                                                           QString(), groupInfo.at(0), groupInfo.at(1));
                        return;
                    }
                }
            }
        }
        delete d;
    } else {
        for (int i = 0; i < givenList.count(); i++)
            list << givenList.at(i);
    }

    foreach(const KUrl & file, list) {
        // Check there is no folder here
        KMimeType::Ptr type = KMimeType::findByUrl(file);
        if (type->is("inode/directory")) {
            // user dropped a folder
            list.removeAll(file);
        }
    }

    if (list.isEmpty())
        return;

    if (givenList.isEmpty()) {
        QStringList groupInfo = getGroup();
        m_doc->slotAddClipList(list, groupInfo.at(0), groupInfo.at(1));
    } else {
        m_doc->slotAddClipList(list, groupName, groupId);
    }
}

void ProjectList::slotRemoveInvalidClip(const QString &id, bool replace)
{
    ProjectItem *item = getItemById(id);
    m_processingClips.removeAll(id);
    if (!m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
    if (item) {
        const QString path = item->referencedClip()->fileURL().path();
        if (item->referencedClip()->isPlaceHolder()) replace = false;
        if (!path.isEmpty()) {
            if (replace)
                KMessageBox::sorry(kapp->activeWindow(), i18n("Clip <b>%1</b><br />is invalid, will be removed from project.", path));
            else if (KMessageBox::questionYesNo(kapp->activeWindow(), i18n("Clip <b>%1</b><br />is missing or invalid. Remove it from project?", path), i18n("Invalid clip")) == KMessageBox::Yes)
                replace = true;
        }
        if (replace)
            emit deleteProjectClips(QStringList() << id, QMap <QString, QString>());
    }
}

void ProjectList::slotAddColorClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    QDialog *dia = new QDialog(this);
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia);
    dia->setWindowTitle(i18n("Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));

    TimecodeDisplay *t = new TimecodeDisplay(m_timecode);
    t->setValue(KdenliveSettings::color_duration());
    t->setTimeCodeFormat(false);
    dia_ui.clip_durationBox->addWidget(t);
    dia_ui.clip_color->setColor(KdenliveSettings::colorclipcolor());

    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        KdenliveSettings::setColorclipcolor(color);
        color = color.replace(0, 1, "0x") + "ff";
        QStringList groupInfo = getGroup();
        m_doc->slotCreateColorClip(dia_ui.clip_name->text(), color, m_timecode.getTimecode(t->gentime()), groupInfo.at(0), groupInfo.at(1));
    }
    delete t;
    delete dia;
}


void ProjectList::slotAddSlideshowClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    SlideshowClip *dia = new SlideshowClip(m_timecode, this);

    if (dia->exec() == QDialog::Accepted) {
        QStringList groupInfo = getGroup();
        m_doc->slotCreateSlideshowClipFile(dia->clipName(), dia->selectedPath(), dia->imageCount(), dia->clipDuration(),
                                           dia->loop(), dia->crop(), dia->fade(),
                                           dia->lumaDuration(), dia->lumaFile(), dia->softness(),
                                           dia->animation(), groupInfo.at(0), groupInfo.at(1));
    }
    delete dia;
}

void ProjectList::slotAddTitleClip()
{
    QStringList groupInfo = getGroup();
    m_doc->slotCreateTextClip(groupInfo.at(0), groupInfo.at(1));
}

void ProjectList::slotAddTitleTemplateClip()
{
    if (!m_commandStack)
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";

    QStringList groupInfo = getGroup();

    // Get the list of existing templates
    QStringList filter;
    filter << "*.kdenlivetitle";
    const QString path = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
    QStringList templateFiles = QDir(path).entryList(filter, QDir::Files);

    QDialog *dia = new QDialog(this);
    Ui::TemplateClip_UI dia_ui;
    dia_ui.setupUi(dia);
    for (int i = 0; i < templateFiles.size(); ++i)
        dia_ui.template_list->comboBox()->addItem(templateFiles.at(i), path + templateFiles.at(i));

    if (!templateFiles.isEmpty())
        dia_ui.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    dia_ui.template_list->fileDialog()->setFilter("application/x-kdenlivetitle");
    //warning: setting base directory doesn't work??
    KUrl startDir(path);
    dia_ui.template_list->fileDialog()->setUrl(startDir);
    dia_ui.text_box->setHidden(true);
    if (dia->exec() == QDialog::Accepted) {
        QString textTemplate = dia_ui.template_list->comboBox()->itemData(dia_ui.template_list->comboBox()->currentIndex()).toString();
        if (textTemplate.isEmpty()) textTemplate = dia_ui.template_list->comboBox()->currentText();
        // Create a cloned template clip
        m_doc->slotCreateTextTemplateClip(groupInfo.at(0), groupInfo.at(1), KUrl(textTemplate));
    }
    delete dia;
}

QStringList ProjectList::getGroup() const
{
    QStringList result;
    QTreeWidgetItem *item = m_listView->currentItem();
    while (item && item->type() != PROJECTFOLDERTYPE)
        item = item->parent();

    if (item) {
        FolderProjectItem *folder = static_cast <FolderProjectItem *>(item);
        result << folder->groupName() << folder->clipId();
    } else {
        result << QString() << QString();
    }
    return result;
}

void ProjectList::setDocument(KdenliveDoc *doc)
{
    m_listView->blockSignals(true);
    m_listView->clear();
    m_processingClips.clear();
    m_listView->setSortingEnabled(false);
    emit clipSelected(NULL);
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;

    QMap <QString, QString> flist = doc->clipManager()->documentFolderList();
    QMapIterator<QString, QString> f(flist);
    while (f.hasNext()) {
        f.next();
        (void) new FolderProjectItem(m_listView, QStringList() << f.value(), f.key());
    }

    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    for (int i = 0; i < list.count(); i++)
        slotAddClip(list.at(i), false);

    m_listView->blockSignals(false);
    connect(m_doc->clipManager(), SIGNAL(reloadClip(const QString &)), this, SLOT(slotReloadClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(modifiedClip(const QString &)), this, SLOT(slotModifiedClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(missingClip(const QString &)), this, SLOT(slotMissingClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(availableClip(const QString &)), this, SLOT(slotAvailableClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(checkAllClips()), this, SLOT(updateAllClips()));
}

QList <DocClipBase*> ProjectList::documentClipList() const
{
    if (m_doc == NULL)
        return QList <DocClipBase*> ();

    return m_doc->clipManager()->documentClipList();
}

QDomElement ProjectList::producersList()
{
    QDomDocument doc;
    QDomElement prods = doc.createElement("producerlist");
    doc.appendChild(prods);
    kDebug() << "////////////  PRO LIST BUILD PRDSLIST ";
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        prods.appendChild(doc.importNode(((ProjectItem *)(*it))->toXml(), true));
        ++it;
    }
    return prods;
}

void ProjectList::slotCheckForEmptyQueue()
{
    if (!m_refreshed && m_thumbnailQueue.isEmpty() && m_infoQueue.isEmpty()) {
        m_refreshed = true;
        emit loadingIsOver();
        emit displayMessage(QString(), -1);
        m_listView->blockSignals(false);
        m_listView->setEnabled(true);
        updateButtons();
    } else if (!m_refreshed) {
        QTimer::singleShot(300, this, SLOT(slotCheckForEmptyQueue()));
    }
}

void ProjectList::reloadClipThumbnails()
{
    m_thumbnailQueue.clear();
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        m_thumbnailQueue << ((ProjectItem *)(*it))->clipId();
        ++it;
    }
    QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
}

void ProjectList::requestClipThumbnail(const QString id)
{
    if (!m_thumbnailQueue.contains(id)) m_thumbnailQueue.append(id);
}

void ProjectList::slotProcessNextThumbnail()
{
    if (m_thumbnailQueue.isEmpty() && m_infoQueue.isEmpty()) {
        slotCheckForEmptyQueue();
        return;
    }
    if (!m_infoQueue.isEmpty()) {
        //QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
        return;
    }
    if (m_thumbnailQueue.count() > 1) {
        int max = m_doc->clipManager()->clipsCount();
        emit displayMessage(i18n("Loading thumbnails"), (int)(100 *(max - m_thumbnailQueue.count()) / max));
    }
    slotRefreshClipThumbnail(m_thumbnailQueue.takeFirst(), false);
}

void ProjectList::slotRefreshClipThumbnail(const QString &clipId, bool update)
{
    QTreeWidgetItem *item = getAnyItemById(clipId);
    if (item)
        slotRefreshClipThumbnail(item, update);
    else
        slotProcessNextThumbnail();
}

void ProjectList::slotRefreshClipThumbnail(QTreeWidgetItem *it, bool update)
{
    if (it == NULL) return;
    ProjectItem *item = NULL;
    bool isSubItem = false;
    int frame;
    if (it->type() == PROJECTFOLDERTYPE) return;
    if (it->type() == PROJECTSUBCLIPTYPE) {
        item = static_cast <ProjectItem *>(it->parent());
        frame = static_cast <SubProjectItem *>(it)->zone().x();
        isSubItem = true;
    } else {
        item = static_cast <ProjectItem *>(it);
        frame = item->referencedClip()->getClipThumbFrame();
    }

    if (item) {
        DocClipBase *clip = item->referencedClip();
        if (!clip) {
            slotProcessNextThumbnail();
            return;
        }
        QPixmap pix;
        int height = m_listView->iconSize().height();
        int width = (int)(height  * m_render->dar());
        if (clip->clipType() == AUDIO)
            pix = KIcon("audio-x-generic").pixmap(QSize(width, height));
        else if (clip->clipType() == IMAGE)
            pix = QPixmap::fromImage(KThumb::getFrame(item->referencedClip()->producer(), 0, width, height));
        else
            pix = item->referencedClip()->thumbProducer()->extractImage(frame, width, height);

        if (!pix.isNull()) {
            monitorItemEditing(false);
            it->setData(0, Qt::DecorationRole, pix);
            monitorItemEditing(true);
                
            if (!isSubItem)
                m_doc->cachePixmap(item->getClipHash(), pix);
            else
                m_doc->cachePixmap(item->getClipHash() + '#' + QString::number(frame), pix);
        }
        if (update)
            emit projectModified();

        slotProcessNextThumbnail();
    }
}

void ProjectList::slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata, bool replace, bool selectClip)
{
    QString toReload;
    ProjectItem *item = getItemById(clipId);
    m_processingClips.removeAll(clipId);
    if (m_infoQueue.isEmpty() && m_processingClips.isEmpty()) m_listView->setEnabled(true);
    if (item && producer) {
        //m_listView->blockSignals(true);
        monitorItemEditing(false);
        item->setProperties(properties, metadata);
        if (item->referencedClip()->isPlaceHolder() && producer->is_valid()) {
            item->referencedClip()->setValid();
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
            toReload = clipId;
        }
        if (item->referencedClip()->getProperty("proxy").isEmpty()) setProxyStatus(item, 0);
        item->referencedClip()->setProducer(producer, replace);
        item->referencedClip()->askForAudioThumbs();
        if (!replace && item->data(0, Qt::DecorationRole).isNull())
            requestClipThumbnail(clipId);
        if (!toReload.isEmpty())
            item->slotSetToolTip();

        if (m_listView->isEnabled() && replace) {
            // update clip in clip monitor
            emit clipSelected(NULL);
            emit clipSelected(item->referencedClip());
            //TODO: Make sure the line below has no side effect
            toReload = clipId;
        }
        /*else {
            // Check if duration changed.
            emit receivedClipDuration(clipId);
            delete producer;
        }*/
        if (m_listView->isEnabled())
            monitorItemEditing(true);
        /*if (item->icon(0).isNull()) {
            requestClipThumbnail(clipId);
        }*/
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
    if (selectClip && m_infoQueue.isEmpty()) {
    if (item && m_infoQueue.isEmpty() && m_thumbnailQueue.isEmpty()) {
        m_listView->setCurrentItem(item);
        bool updatedProfile = false;
        if (item->parent()) {
            if (item->parent()->type() == PROJECTFOLDERTYPE)
                static_cast <FolderProjectItem *>(item->parent())->switchIcon();
        } else if (KdenliveSettings::checkfirstprojectclip() &&  m_listView->topLevelItemCount() == 1) {
            // this is the first clip loaded in project, check if we want to adjust project settings to the clip
            updatedProfile = adjustProjectProfileToItem(item);
        }
        if (updatedProfile == false) emit clipSelected(item->referencedClip());
    } else {
        int max = m_doc->clipManager()->clipsCount();
        emit displayMessage(i18n("Loading clips"), (int)(100 *(max - m_infoQueue.count()) / max));
    }
    }
    if (!toReload.isEmpty())
        emit clipNeedsReload(toReload, true);

    if (!m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
}

bool ProjectList::adjustProjectProfileToItem(ProjectItem *item)
{
    if (item == NULL) {
        if (m_listView->currentItem() && m_listView->currentItem()->type() != PROJECTFOLDERTYPE)
            item = static_cast <ProjectItem*>(m_listView->currentItem());
    }
    if (item == NULL || item->referencedClip() == NULL) {
        KMessageBox::information(kapp->activeWindow(), i18n("Cannot find profile from current clip"));
        return false;
    }
    bool profileUpdated = false;
    QString size = item->referencedClip()->getProperty("frame_size");
    int width = size.section('x', 0, 0).toInt();
    int height = size.section('x', -1).toInt();
    double fps = item->referencedClip()->getProperty("fps").toDouble();
    double par = item->referencedClip()->getProperty("aspect_ratio").toDouble();
    if (item->clipType() == IMAGE || item->clipType() == AV || item->clipType() == VIDEO) {
        if (ProfilesDialog::matchProfile(width, height, fps, par, item->clipType() == IMAGE, m_doc->mltProfile()) == false) {
            // get a list of compatible profiles
            QMap <QString, QString> suggestedProfiles = ProfilesDialog::getProfilesFromProperties(width, height, fps, par, item->clipType() == IMAGE);
            if (!suggestedProfiles.isEmpty()) {
                KDialog *dialog = new KDialog(this);
                dialog->setCaption(i18n("Change project profile"));
                dialog->setButtons(KDialog::Ok | KDialog::Cancel);

                QWidget container;
                QVBoxLayout *l = new QVBoxLayout;
                QLabel *label = new QLabel(i18n("Your clip does not match current project's profile.\nDo you want to change the project profile?\n\nThe following profiles match the clip (size: %1, fps: %2)", size, fps));
                l->addWidget(label);
                QListWidget *list = new QListWidget;
                list->setAlternatingRowColors(true);
                QMapIterator<QString, QString> i(suggestedProfiles);
                while (i.hasNext()) {
                    i.next();
                    QListWidgetItem *item = new QListWidgetItem(i.value(), list);
                    item->setData(Qt::UserRole, i.key());
                    item->setToolTip(i.key());
                }
                list->setCurrentRow(0);
                l->addWidget(list);
                container.setLayout(l);
                dialog->setButtonText(KDialog::Ok, i18n("Update profile"));
                dialog->setMainWidget(&container);
                if (dialog->exec() == QDialog::Accepted) {
                    //Change project profile
                    profileUpdated = true;
                    if (list->currentItem())
                        emit updateProfile(list->currentItem()->data(Qt::UserRole).toString());
                }
                delete list;
                delete label;
            } else if (fps > 0) {
                KMessageBox::information(kapp->activeWindow(), i18n("Your clip does not match current project's profile.\nNo existing profile found to match the clip's properties.\nClip size: %1\nFps: %2\n", size, fps));
            }
        }
    }
    return profileUpdated;
}

void ProjectList::slotReplyGetImage(const QString &clipId, const QPixmap &pix)
{
    ProjectItem *item = getItemById(clipId);
    if (item && !pix.isNull()) {
        monitorItemEditing(false);
        item->setData(0, Qt::DecorationRole, pix);
        monitorItemEditing(true);
        m_doc->cachePixmap(item->getClipHash(), pix);
        if (m_listView->isEnabled())
            m_listView->blockSignals(false);
    }
}

QTreeWidgetItem *ProjectList::getAnyItemById(const QString &id)
{
    QTreeWidgetItemIterator it(m_listView);
    QString lookId = id;
    if (id.contains('#'))
        lookId = id.section('#', 0, 0);

    ProjectItem *result = NULL;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        ProjectItem *item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == lookId) {
            result = item;
            break;
        }
        ++it;
    }
    if (result == NULL || !id.contains('#')) {
        return result;
    } else {
        for (int i = 0; i < result->childCount(); i++) {
            SubProjectItem *sub = static_cast <SubProjectItem *>(result->child(i));
            if (sub && sub->zone().x() == id.section('#', 1, 1).toInt())
                return sub;
        }
    }

    return NULL;
}


ProjectItem *ProjectList::getItemById(const QString &id)
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == id)
            return item;
        ++it;
    }
    return NULL;
}

FolderProjectItem *ProjectList::getFolderItemById(const QString &id)
{
    FolderProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTFOLDERTYPE) {
            item = static_cast<FolderProjectItem *>(*it);
            if (item->clipId() == id)
                return item;
        }
        ++it;
    }
    return NULL;
}

void ProjectList::slotSelectClip(const QString &ix)
{
    ProjectItem *clip = getItemById(ix);
    if (clip) {
        m_listView->setCurrentItem(clip);
        m_listView->scrollToItem(clip);
        m_editButton->defaultAction()->setEnabled(true);
        m_deleteButton->defaultAction()->setEnabled(true);
        m_reloadAction->setEnabled(true);
        m_transcodeAction->setEnabled(true);
        m_proxyAction->setEnabled(true);
        if (clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else {
            m_openAction->setEnabled(false);
        }
    }
}

QString ProjectList::currentClipUrl() const
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE) return QString();
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
        // subitem
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    } else {
        item = static_cast <ProjectItem*>(m_listView->currentItem());
    }
    if (item == NULL)
        return QString();
    return item->clipUrl().path();
}

KUrl::List ProjectList::getConditionalUrls(const QString &condition) const
{
    KUrl::List result;
    ProjectItem *item;
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == PROJECTFOLDERTYPE)
            continue;
        if (list.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        } else {
            item = static_cast <ProjectItem*>(list.at(i));
        }
        if (item == NULL || item->type() == COLOR || item->type() == SLIDESHOW || item->type() == TEXT)
            continue;
        DocClipBase *clip = item->referencedClip();
        if (!condition.isEmpty()) {
            if (condition.startsWith("vcodec") && !clip->hasVideoCodec(condition.section('=', 1, 1)))
                continue;
            else if (condition.startsWith("acodec") && !clip->hasAudioCodec(condition.section('=', 1, 1)))
                continue;
        }
        result.append(item->clipUrl());
    }
    return result;
}

void ProjectList::regenerateTemplate(const QString &id)
{
    ProjectItem *clip = getItemById(id);
    if (clip)
        regenerateTemplate(clip);
}

void ProjectList::regenerateTemplate(ProjectItem *clip)
{
    //TODO: remove this unused method, only force_reload is necessary
    clip->referencedClip()->producer()->set("force_reload", 1);
}

QDomDocument ProjectList::generateTemplateXml(QString path, const QString &replaceString)
{
    QDomDocument doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        return doc;
    }
    if (!doc.setContent(&file)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        file.close();
        return doc;
    }
    file.close();
    QDomNodeList texts = doc.elementsByTagName("content");
    for (int i = 0; i < texts.count(); i++) {
        QString data = texts.item(i).firstChild().nodeValue();
        data.replace("%s", replaceString);
        texts.item(i).firstChild().setNodeValue(data);
    }
    return doc;
}


void ProjectList::slotAddClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip == NULL || clip->referencedClip()->hasCutZone(QPoint(in, out)))
        return;
    AddClipCutCommand *command = new AddClipCutCommand(this, id, in, out, QString(), true, false);
    m_commandStack->push(command);
}

void ProjectList::addClipCut(const QString &id, int in, int out, const QString desc, bool newItem)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->addCutZone(in, out);
        monitorItemEditing(false);
        SubProjectItem *sub = new SubProjectItem(clip, in, out, desc);
        if (newItem && desc.isEmpty() && !m_listView->isColumnHidden(1)) {
            if (!clip->isExpanded())
                clip->setExpanded(true);
            m_listView->scrollToItem(sub);
            m_listView->editItem(sub, 1);
        }
        QPixmap p = clip->referencedClip()->thumbProducer()->extractImage(in, (int)(sub->sizeHint(0).height()  * m_render->dar()), sub->sizeHint(0).height() - 2);
        sub->setData(0, Qt::DecorationRole, p);
        m_doc->cachePixmap(clip->getClipHash() + '#' + QString::number(in), p);
        monitorItemEditing(true);
    }
    emit projectModified();
}

void ProjectList::removeClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->removeCutZone(in, out);
        SubProjectItem *sub = getSubItem(clip, QPoint(in, out));
        if (sub) {
            monitorItemEditing(false);
            delete sub;
            monitorItemEditing(true);
        }
    }
    emit projectModified();
}

SubProjectItem *ProjectList::getSubItem(ProjectItem *clip, QPoint zone)
{
    SubProjectItem *sub = NULL;
    if (clip) {
        for (int i = 0; i < clip->childCount(); i++) {
            QTreeWidgetItem *it = clip->child(i);
            if (it->type() == PROJECTSUBCLIPTYPE) {
                sub = static_cast <SubProjectItem*>(it);
                if (sub->zone() == zone)
                    break;
                else
                    sub = NULL;
            }
        }
    }
    return sub;
}

void ProjectList::slotUpdateClipCut(QPoint p)
{
    if (!m_listView->currentItem() || m_listView->currentItem()->type() != PROJECTSUBCLIPTYPE)
        return;
    SubProjectItem *sub = static_cast <SubProjectItem*>(m_listView->currentItem());
    ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
    EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), p, sub->text(1), sub->text(1), true);
    m_commandStack->push(command);
}

void ProjectList::doUpdateClipCut(const QString &id, const QPoint oldzone, const QPoint zone, const QString &comment)
{
    ProjectItem *clip = getItemById(id);
    SubProjectItem *sub = getSubItem(clip, oldzone);
    if (sub == NULL || clip == NULL)
        return;
    DocClipBase *base = clip->referencedClip();
    base->updateCutZone(oldzone.x(), oldzone.y(), zone.x(), zone.y(), comment);
    monitorItemEditing(false);
    sub->setZone(zone);
    sub->setDescription(comment);
    monitorItemEditing(true);
    emit projectModified();
}

void ProjectList::slotForceProcessing(const QString &id)
{
    while (m_infoQueue.contains(id)) {
        slotProcessNextClipInQueue();
    }
}

void ProjectList::slotAddOrUpdateSequence(const QString frameName)
{
    QString fileName = KUrl(frameName).fileName().section('_', 0, -2);
    QStringList list;
    QString pattern = SlideshowClip::selectedPath(frameName, false, QString(), &list);
    int count = list.count();
    if (count > 1) {
        const QList <DocClipBase *> existing = m_doc->clipManager()->getClipByResource(pattern);
        if (!existing.isEmpty()) {
            // Sequence already exists, update
            QString id = existing.at(0)->getId();
            //ProjectItem *item = getItemById(id);
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            int ttl = existing.at(0)->getProperty("ttl").toInt();
            oldprops["out"] = existing.at(0)->getProperty("out");
            newprops["out"] = QString::number(ttl * count - 1);
            slotUpdateClipProperties(id, newprops);
            EditClipCommand *command = new EditClipCommand(this, id, oldprops, newprops, false);
            m_commandStack->push(command);
        } else {
            // Create sequence
            QStringList groupInfo = getGroup();
            m_doc->slotCreateSlideshowClipFile(fileName, pattern, count, m_timecode.reformatSeparators(KdenliveSettings::sequence_duration()),
                                               false, false, false,
                                               m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))), QString(), 0,
                                               QString(), groupInfo.at(0), groupInfo.at(1));
        }
    } else emit displayMessage(i18n("Sequence not found"), -2);
}

QMap <QString, QString> ProjectList::getProxies()
{
    QMap <QString, QString> list;
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item && item->referencedClip() != NULL) {
            QString proxy = item->referencedClip()->getProperty("proxy");
            if (!proxy.isEmpty()) list.insert(proxy, item->clipUrl().path());
        }
        ++it;
    }
    return list;
}

void ProjectList::updateProxyConfig()
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item && item->referencedClip() != NULL) {
            if  (KdenliveSettings::enableproxy()) {
                DocClipBase *clip = item->referencedClip();
                connect(clip, SIGNAL(proxyReady(const QString, bool)), this, SLOT(slotGotProxy(const QString, bool)));
                setProxyStatus(item, 1);
                clip->generateProxy(m_doc->projectFolder());
            }
            else if (!item->referencedClip()->getProperty("proxy").isEmpty()) {
                // remove proxy
                item->referencedClip()->clearProperty("proxy");
                QDomElement e = item->toXml().cloneNode().toElement();
                e.removeAttribute("file_hash");
                e.setAttribute("replace", 1);
                m_infoQueue.insert(item->clipId(), e);
            }
        }
        ++it;
    }
    if (!m_infoQueue.isEmpty() && !m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
}

void ProjectList::slotProxyCurrentItem(bool doProxy)
{
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    QTreeWidgetItem *listItem;
    for (int i = 0; i < list.count(); i++) {
        listItem = list.at(i);
        if (listItem->type() == PROJECTFOLDERTYPE) {
            for (int j = 0; j < listItem->childCount(); j++) {
                QTreeWidgetItem *sub = listItem->child(j);
                if (!list.contains(sub)) list.append(sub);
            }
        }
        if (listItem->type() == PROJECTCLIPTYPE) {
            ProjectItem *item = static_cast <ProjectItem*>(listItem);
            if (item->referencedClip()) {
                if (doProxy) {
                    DocClipBase *clip = item->referencedClip();
                    connect(clip, SIGNAL(proxyReady(const QString, bool)), this, SLOT(slotGotProxy(const QString, bool)));
                    setProxyStatus(item, 1);
                    clip->generateProxy(m_doc->projectFolder());
                }
                else if (!item->referencedClip()->getProperty("proxy").isEmpty()) {
                    // remove proxy
                    item->referencedClip()->clearProperty("proxy");
                    QDomElement e = item->toXml().cloneNode().toElement();
                    e.removeAttribute("file_hash");
                    e.setAttribute("replace", 1);
                    m_infoQueue.insert(item->clipId(), e);
                }
            }
        }
    }
    if (!m_infoQueue.isEmpty() && !m_queueRunner.isRunning() && m_processingClips.isEmpty()) m_queueRunner = QtConcurrent::run(this, &ProjectList::slotProcessNextClipInQueue);
}

void ProjectList::setProxyStatus(ProjectItem *item, int status)
{
    monitorItemEditing(false);
    item->setProxyStatus(status);
    monitorItemEditing(true);
}

void ProjectList::monitorItemEditing(bool enable)
{
    if (enable) connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));     
    else disconnect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));     
}

#include "projectlist.moc"

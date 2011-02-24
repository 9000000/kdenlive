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


#include "effectslistwidget.h"
#include "effectslist.h"
#include "mainwindow.h"

#include <KDebug>
#include <KStandardDirs>
#include <KAction>

#include <QApplication>
#include <QMouseEvent>
#include <QMenu>


static const int EFFECT_VIDEO = 1;
static const int EFFECT_AUDIO = 2;
static const int EFFECT_CUSTOM = 3;
static const int EFFECT_FOLDER = 4;

const int TypeRole = Qt::UserRole;
const int IdRole = TypeRole + 1;


EffectsListWidget::EffectsListWidget(QMenu *contextMenu, QWidget *parent) :
        QTreeWidget(parent),
        m_menu(contextMenu)
{
    setColumnCount(1);
    setDragEnabled(true);
    setAcceptDrops(false);
    setHeaderHidden(true);
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    setRootIsDecorated(false);
    //setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragOnly);
    QPalette p = palette();
    p.setBrush(QPalette::Base, Qt::NoBrush);
    setPalette(p);
    connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(slotExpandItem(const QModelIndex &)));
}

EffectsListWidget::~EffectsListWidget()
{
}

void EffectsListWidget::slotExpandItem(const QModelIndex & index)
{
    setExpanded(index, !isExpanded(index));
}

void EffectsListWidget::initList(QMenu *effectsMenu, KActionCategory *effectActions)
{
    QString current;
    QString currentFolder;
    QTreeWidgetItem *item = NULL;
    bool found = false;
    effectsMenu->clear();
    
    if (currentItem()) {
        current = currentItem()->text(0);
        if (currentItem()->parent())
            currentFolder = currentItem()->parent()->text(0);
        else if (currentItem()->data(0, TypeRole) ==  EFFECT_FOLDER)
            currentFolder = currentItem()->text(0);
    }

    KIcon folderIcon("folder");

    QString effectCategory = KStandardDirs::locate("config", "kdenliveeffectscategory.rc");
    QDomDocument doc;
    QFile file(effectCategory);
    doc.setContent(&file, false);
    file.close();
    QList <QTreeWidgetItem *> folders;
    QStringList folderNames;
    QDomNodeList groups = doc.documentElement().elementsByTagName("group");
    for (int i = 0; i < groups.count(); i++) {
        folderNames << i18n(groups.at(i).firstChild().firstChild().nodeValue().toUtf8().constData());
    }
    for (int i = 0; i < topLevelItemCount(); i++) {
        topLevelItem(i)->takeChildren();
        QString currentName = topLevelItem(i)->text(0);
        if (currentName != i18n("Misc") && currentName != i18n("Audio") && currentName != i18nc("Folder Name", "Custom") && !folderNames.contains(currentName)) {
            takeTopLevelItem(i);
            i--;
        }
    }

    for (int i = 0; i < groups.count(); i++) {
        item = findFolder(folderNames.at(i));
        if (item) {
            item->setData(0, IdRole, groups.at(i).toElement().attribute("list"));
        } else {
            item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(folderNames.at(i)));
            item->setIcon(0, folderIcon);
            item->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
            item->setData(0, IdRole, groups.at(i).toElement().attribute("list"));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            insertTopLevelItem(0, item);
        }
        folders.append(item);
    }

    QTreeWidgetItem *misc = findFolder(i18n("Misc"));
    if (misc == NULL) {
        misc = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18n("Misc")));
        misc->setIcon(0, folderIcon);
        misc->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        misc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, misc);
    }

    QTreeWidgetItem *audio = findFolder(i18n("Audio"));
    if (audio == NULL) {
        audio = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18n("Audio")));
        audio->setIcon(0, folderIcon);
        audio->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        audio->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, audio);
    }

    QTreeWidgetItem *custom = findFolder(i18nc("Folder Name", "Custom"));
    if (custom == NULL) {
        custom = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18nc("Folder Name", "Custom")));
        custom->setIcon(0, folderIcon);
        custom->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        custom->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, custom);
    }

    //insertTopLevelItems(0, folders);

    loadEffects(&MainWindow::videoEffects, KIcon("kdenlive-show-video"), misc, &folders, QString::number((int) EFFECT_VIDEO), current, &found);
    loadEffects(&MainWindow::audioEffects, KIcon("kdenlive-show-audio"), audio, &folders, QString::number((int) EFFECT_AUDIO), current, &found);
    loadEffects(&MainWindow::customEffects, KIcon("kdenlive-custom-effect"), custom, static_cast<QList<QTreeWidgetItem *> *>(0), QString::number((int) EFFECT_CUSTOM), current, &found);

    if (!found && !currentFolder.isEmpty()) {
        // previously selected effect was removed, focus on its parent folder
        for (int i = 0; i < topLevelItemCount(); i++) {
            if (topLevelItem(i)->text(0) == currentFolder) {
                setCurrentItem(topLevelItem(i));
                break;
            }
        }

    }
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    // populate effects menu
    for (int i = 0; i < topLevelItemCount(); i++) {
        QMenu *sub = new QMenu(topLevelItem(i)->text(0), effectsMenu);
        effectsMenu->addMenu(sub);
        for (int j = 0; j < topLevelItem(i)->childCount(); j++) {
                QTreeWidgetItem *item = topLevelItem(i)->child(j);
                KAction *a = new KAction(KIcon(item->icon(0)), item->text(0), sub);
                QStringList data = item->data(0, IdRole).toStringList();
                QString id = data.at(1);
                if (id.isEmpty()) id = data.at(0);
                a->setData(data);
                a->setIconVisibleInMenu(false);
                sub->addAction(a);
                effectActions->addAction("video_effect_" + id, a);
        }
    }
}

void EffectsListWidget::loadEffects(const EffectsList *effectlist, KIcon icon, QTreeWidgetItem *defaultFolder, const QList<QTreeWidgetItem *> *folders, const QString type, const QString current, bool *found)
{
    QStringList effectInfo, l;
    QTreeWidgetItem *parentItem;
    QTreeWidgetItem *item;
    int ct = effectlist->count();

    for (int ix = 0; ix < ct; ix ++) {
        effectInfo = effectlist->effectIdInfo(ix);
        effectInfo.append(type);
        parentItem = NULL;

        if (folders) {
            for (int i = 0; i < folders->count(); i++) {
                l = folders->at(i)->data(0, IdRole).toString().split(',', QString::SkipEmptyParts);
                if (l.contains(effectInfo.at(2))) {
                    parentItem = folders->at(i);
                    break;
                }
            }
        }
        if (parentItem == NULL)
            parentItem = defaultFolder;

        if (!effectInfo.isEmpty()) {
            item = new QTreeWidgetItem(parentItem, QStringList(effectInfo.takeFirst()));
            item->setIcon(0, icon);
            item->setData(0, TypeRole, type);
            item->setData(0, IdRole, effectInfo);
            item->setToolTip(0, effectlist->getInfo(effectInfo.at(0), effectInfo.at(1)));
            if (item->text(0) == current) {
                setCurrentItem(item);
                *found = true;
            }
        }
    }
}

QTreeWidgetItem *EffectsListWidget::findFolder(const QString name)
{
    QTreeWidgetItem *item = NULL;
    QList<QTreeWidgetItem *> result = findItems(name, Qt::MatchExactly);
    if (!result.isEmpty()) {
        for (int j = 0; j < result.count(); j++) {
            if (result.at(j)->data(0, TypeRole) ==  EFFECT_FOLDER) {
                item = result.at(j);
                break;
            }
        }
    }
    return item;
}

const QDomElement EffectsListWidget::currentEffect() const
{
    return itemEffect(currentItem());
}

const QDomElement EffectsListWidget::itemEffect(QTreeWidgetItem *item) const
{
    QDomElement effect;
    if (!item || item->data(0, TypeRole).toInt() == (int)EFFECT_FOLDER) return effect;
    QStringList effectInfo = item->data(0, IdRole).toStringList();
    kDebug() << "// EFFECT SELECTED: " << effectInfo;
    switch (item->data(0, TypeRole).toInt()) {
    case 1:
        effect =  MainWindow::videoEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    case 2:
        effect = MainWindow::audioEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    default:
        effect = MainWindow::customEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    }
    return effect;
}


QString EffectsListWidget::currentInfo()
{
    QTreeWidgetItem *item = currentItem();
    if (!item || item->data(0, TypeRole).toInt() == (int)EFFECT_FOLDER) return QString();
    QString info;
    QStringList effectInfo = item->data(0, IdRole).toStringList();
    switch (item->data(0, TypeRole).toInt()) {
    case 1:
        info = MainWindow::videoEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    case 2:
        info = MainWindow::audioEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    default:
        info = MainWindow::customEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    }
    return info;
}

//virtual
QMimeData * EffectsListWidget::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QDomDocument doc;
    foreach(QTreeWidgetItem *item, list) {
        if (item->flags() & Qt::ItemIsDragEnabled) {
            const QDomElement e = itemEffect(item);
            if (!e.isNull()) doc.appendChild(doc.importNode(e, true));
        }
    }
    QMimeData *mime = new QMimeData;
    QByteArray data;
    data.append(doc.toString().toUtf8());
    mime->setData("kdenlive/effectslist", data);
    return mime;
}

//virtual
void EffectsListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}


//virtual
void EffectsListWidget::contextMenuEvent(QContextMenuEvent * event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    if (item && item->data(0, TypeRole).toInt() == EFFECT_CUSTOM) m_menu->popup(event->globalPos());
}

#include "effectslistwidget.moc"

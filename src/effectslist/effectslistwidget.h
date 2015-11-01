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


#ifndef EFFECTSLISTWIDGET_H
#define EFFECTSLISTWIDGET_H

#include <QTreeWidget>
#include <QDomElement>

#include <KActionCategory>

class EffectsList;

class EffectsListWidget : public QTreeWidget
{
    Q_OBJECT

    public:
    static const int EFFECT_VIDEO = 1;
    static const int EFFECT_AUDIO = 2;
    static const int EFFECT_GPU = 3;
    static const int EFFECT_CUSTOM = 4;
    static const int EFFECT_FAVORITES = 5;
    static const int EFFECT_FOLDER = 6;
    static const int TRANSITION_TYPE= 7;

    static const int TypeRole = Qt::UserRole;
    static const int IdRole = TypeRole + 1;

    explicit EffectsListWidget(QWidget *parent = 0);
    virtual ~EffectsListWidget();
    const QDomElement currentEffect() const;
    QString currentInfo() const;
    static const QDomElement itemEffect(int type, QStringList effectInfo);
    void initList(QMenu *effectsMenu, KActionCategory *effectActions, QString categoryFile, bool transitionMode);
    void updatePalette();
    void setRootOnCustomFolder();
    void resetRoot();
    void createFavorites(QList <QTreeWidgetItem *>list);
    void resetFavorites();

protected:
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent * event);
    virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const;
    virtual void keyPressEvent(QKeyEvent *e);

private:
    QMenu *m_menu;
    /** @brief Returns the folder item whose name == @param name. */
    QTreeWidgetItem *findFolder(const QString &name);

    /** @brief Loads the effects from the given effectlist as item of this widget.
     * @param effectlist effectlist containing the effects that should be loaded
     * @param icon the icon to be used for the QTreeWidgetItems
     * @param defaultFolder parent item which will be used by default 
     * @param folders list of folders which might be used instead for specific effects
     * @param type type of the effects
     * @param current name of selected effect before reload; if an effect name matches this one it will become selected
     * @param found will be set to true if an effect name matches current
     */
    void loadEffects(const EffectsList *effectlist, QIcon icon, QTreeWidgetItem *defaultFolder, const QList<QTreeWidgetItem *> *folders, int type, const QString &current, bool *found);
    QIcon generateIcon(int size, const QString &name, const QString &group, QDomElement info);

private slots:
    void slotExpandItem(const QModelIndex & index);
    
signals:
    void applyEffect(const QDomElement);
    void displayMenu(QTreeWidgetItem *, const QPoint &);
};

#endif

/*
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

#ifndef PROJECTSORTPROXYMODEL_H
#define PROJECTSORTPROXYMODEL_H

#include <QSortFilterProxyModel>

class AbstractProjectItem;
class QItemSelectionModel;

/**
 * @class ProjectSortProxyModel
 * @brief Acts as an filtering proxy for the Bin Views, used when triggering the lineedit filter.
 */

class ProjectSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ProjectSortProxyModel(QObject *parent = 0);
    QItemSelectionModel* selectionModel();

public slots:
    /** @brief Set search string that will filter the view */
    void slotSetSearchString(const QString &str);
    /** @brief Relay datachanged signal from view's model  */
    void slotDataChanged(const QModelIndex &ix1, const QModelIndex &ix2);

private slots:
    /** @brief Called when a row change is detected by selection model */
    void onCurrentRowChanged(const QItemSelection& current, const QItemSelection& previous);

protected:
    /** @brief Decide which items should be displayed depending on the search string  */
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    /** @brief Reimplemented to show folders first  */
    bool lessThan(const QModelIndex & left, const QModelIndex & right) const;

private:
    QItemSelectionModel*m_selection;
    QString m_searchString;

signals:
    /** @brief Emitted when the row changes, used to prepare action for selected item  */
    void selectModel(const QModelIndex&);
 };
 
#endif

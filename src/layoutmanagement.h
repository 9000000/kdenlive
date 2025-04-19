/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QMap>
#include <QObject>
#include <QTimer>

class KSelectAction;
class KActionCategory;
class QAction;
class QButtonGroup;
class QAbstractButton;
class QHBoxLayout;
class QFrame;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);
    /** @brief Load a layout by its name. */
    bool loadLayout(const QString &layoutId, bool selectButton);

private Q_SLOTS:
    /** @brief Saves the widget layout. */
    void slotSaveLayout();
    /** @brief Loads a layout from its button. */
    void activateLayout(QAbstractButton *button);
    /** @brief Loads a saved widget layout. */
    void slotLoadLayout(QAction *action);
    /** @brief Manage layout. */
    void slotManageLayouts();
    /** @brief Arrange the Qt::DockWidgetAreas in rows. */
    void slotDockAreaRows();
    /** @brief Arrange the Qt::DockWidgetAreas in columns. */
    void slotDockAreaColumns();
    /** @brief Hide the autosave indicator . */
    void hideAutoSave();
    /** @brief Show the autosave indicator for 2 seconds. */
    void startAutoSave();

private:
    /** @brief Saves the given layout asking the user for a name.
     * @param layout
     * @param suggestedName name that is filled in to the save layout dialog
     * @return names of the saved layout. First is the visible name, second the internal name (they are different if the layout is a default one)
    */
    std::pair<QString, QString> saveLayout(const QString &layout, const QString &suggestedName);
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    const QString translatedName(const QString &name);
    QWidget *m_container;
    QButtonGroup *m_containerGrp;
    QHBoxLayout *m_containerLayout;
    QFrame *m_autosaveLabel{nullptr};
    QTimer m_autosaveDisplayTimer;
    KSelectAction *m_loadLayout;
    QList <QAction *> m_layoutActions;
    QMap <QString, QString> m_translatedNames;
    KActionCategory *m_layoutCategory;

Q_SIGNALS:
    /** @brief Layout changed, ensure title bars are correctly displayed. */
    void updateTitleBars();
    /** @brief Connect/disconnect stuff to update titlebars on dock location changed. */
    void connectDocks(bool doConnect);
};

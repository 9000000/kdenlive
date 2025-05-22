/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"

class QQuickWidget;
class Monitor;

/** @class QmlManager
    @brief Manages all Qml monitor overlays
    @author Jean-Baptiste Mardelle
 */
class QmlManager : public QObject
{
    Q_OBJECT

public:
    explicit QmlManager(QQuickWidget *view, Monitor *monitor);

    /** @brief return current active scene type */
    MonitorSceneType sceneType() const;
    /** @brief Set a property on the root item */
    void setProperty(const QString &name, const QVariant &value);
    /** @brief Load a monitor scene */
    bool setScene(Kdenlive::MonitorId id, MonitorSceneType type, QSize profile, double profileStretch, int duration);
    /** @brief reset stored scene type */
    void clearSceneType();

private:
    QQuickWidget *m_view;
    Monitor *m_monitor;
    MonitorSceneType m_sceneType;
    bool m_sceneChangeBlocked{false};

public Q_SLOTS:
    void blockSceneChange(bool block);

private Q_SLOTS:
    void effectPolygonChanged();
    void effectRotoChanged(const QVariant&,const QVariant&);

Q_SIGNALS:
    void effectPointsChanged(const QVariantList &);
    void activateTrack(int);
};

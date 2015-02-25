/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/


#include "jogmanager.h"
#include "jogshuttle.h"
#include "jogaction.h"
#include "jogshuttleconfig.h"
#include "kdenlivesettings.h"
#include "core.h"
#include "mainwindow.h"

JogManager::JogManager(QObject* parent) :
    QObject(parent),
    m_shuttle(0),
    m_shuttleAction(0)
{
    slotConfigurationChanged();

    connect(pCore->window(), SIGNAL(configurationChanged()), SLOT(slotConfigurationChanged()));
}

void JogManager::slotConfigurationChanged()
{
    delete m_shuttleAction;
    m_shuttleAction = NULL;
    delete m_shuttle;
    m_shuttle = NULL;

    if (KdenliveSettings::enableshuttle()) {
        m_shuttle = new JogShuttle(JogShuttle::canonicalDevice(KdenliveSettings::shuttledevice()));
        m_shuttleAction = new JogShuttleAction(m_shuttle, JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons()));

        connect(m_shuttleAction, SIGNAL(action(QString)), SLOT(slotDoAction(QString)));
    }
}

void JogManager::slotDoAction(const QString& actionName)
{
    QAction* action = pCore->window()->actionCollection()->action(actionName);
    if (!action) {
        fprintf(stderr, "%s", QString("shuttle action '%1' unknown\n").arg(actionName).toLatin1().constData());
        return;
    }
    action->trigger();
}


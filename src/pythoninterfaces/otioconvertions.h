/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/abstractpythoninterface.h"

#include <QDialog>
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>

class OtioConvertions: public AbstractPythonInterface
{
    Q_OBJECT
public:
    OtioConvertions();
    bool getOtioConverters();
    bool configureSetup();
    bool wellConfigured();
    bool runOtioconvert(const QString &inputFile, const QString &outputFile);

private:
    QString m_importAdapters;
    QString m_exportAdapters;

public Q_SLOTS:
    void slotExportProject();
    void slotImportProject();
};

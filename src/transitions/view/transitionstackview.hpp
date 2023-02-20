/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/view/assetparameterview.hpp"
#include "definitions.h"

class QComboBox;

class TransitionStackView : public AssetParameterView
{
    Q_OBJECT

public:
    TransitionStackView(QWidget *parent = nullptr);
    void setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer = false) override;
    void unsetModel();
    ObjectId stackOwner() const;
    void refreshTracks();

private Q_SLOTS:
    void updateTrack(int newTrack);
    void checkCompoTrack();

Q_SIGNALS:
    void seekToTransPos(int pos);

private:
    QComboBox *m_trackBox{nullptr};
};

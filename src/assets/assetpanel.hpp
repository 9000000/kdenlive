/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "definitions.h"

class KSqueezedTextLabel;
class KDualAction;
class KMessageWidget;
class QToolButton;
class QComboBox;
class QScrollArea;

class AssetParameterModel;
class AssetParameterView;
class EffectStackModel;
class EffectStackView;
class TransitionStackView;
class MaskManager;
class MixStackView;
class QLabel;

/** @class AssetPanel
    @brief This class is the widget that provides interaction with the asset currently selected.
    That is, it either displays an effectStack or the parameters of a transition
 */
class AssetPanel : public QWidget
{
    Q_OBJECT

public:
    AssetPanel(QWidget *parent);

    /** @brief Shows the parameters of the given transition model */
    void showTransition(int tid, const std::shared_ptr<AssetParameterModel> &transition_model);
    /** @brief Shows the parameters of the given mix model */
    void showMix(int cid, const std::shared_ptr<AssetParameterModel> &transitionModel, bool refreshOnly);

    /** @brief Shows the parameters of the given effect stack model */
    void showEffectStack(const QString &itemName, const std::shared_ptr<EffectStackModel> &effectsModel, QSize frameSize, bool showKeyframes);

    /** @brief Clear the panel so that it doesn't display anything */
    void clear();

    /** @brief Returns the object type / id of effectstack owner */
    ObjectId effectStackOwner();
    /** @brief Add an effect to the current stack owner */
    bool addEffect(const QString &effectId);
    /** @brief Used to pass a standard action like copy or paste to the effect stack widget */
    void sendStandardCommand(int command);
    /** @brief Project is closing, check if we have a running task */
    bool hasRunningTask() const;
    /** @brief Start mask creation mode */
    bool launchObjectMask();

public Q_SLOTS:
    /** @brief Clear panel if displaying itemId */
    void clearAssetPanel(int itemId);
    void assetPanelWarning(const QString &service, const QString &message, const QString &log = QString());
    void deleteCurrentEffect();
    /** @brief Collapse/expand current effect */
    void collapseCurrentEffect();
    void slotCheckWheelEventFilter();
    void slotAddRemoveKeyframe();
    void slotNextKeyframe();
    void slotPreviousKeyframe();
    /** @brief Update timelinbe position in keyframe views */
    void updateAssetPosition(int itemId, const QUuid uuid);

protected:
    QVBoxLayout *m_lay;
    KSqueezedTextLabel *m_assetTitle;
    QWidget *m_container;
    TransitionStackView *m_transitionWidget{nullptr};
    MixStackView *m_mixWidget{nullptr};
    EffectStackView *m_effectStackWidget{nullptr};
    MaskManager *m_maskManager{nullptr};

private:
    QAction *m_compositionHelpLink;
    QMenu *m_applyEffectGroups;
    QAction *m_saveEffectStack;
    QAction *m_showMaskPanel;
    QComboBox *m_switchCompoButton;
    QAction *m_titleAction;
    QAction *m_switchAction;
    KDualAction *m_splitButton;
    KDualAction *m_enableStackButton;
    KDualAction *m_timelineButton;
    QScrollArea *m_sc;
    KMessageWidget *m_infoMessage;
    QTimer m_dragScrollTimer;

private Q_SLOTS:
    void processSplitEffect(bool enable);
    /** Displays the owner clip keyframes in timeline */
    void showKeyframes(bool enable);
    /** Enable / disable effect stack */
    void enableStack(bool enable);
    /** Scroll effects view */
    void scrollTo(QRect rect);
    /** Check if view needs to be scrolled on drag move */
    void checkDragScroll();
    /** Show/hide mask panel */
    void slotShowMaskPanel();

Q_SIGNALS:
    void doSplitEffect(bool);
    void doSplitBinEffect(bool);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void switchCurrentComposition(int tid, const QString &compoId);
    void slotSaveStack();
    void slotSwitchCollapseAll();
};

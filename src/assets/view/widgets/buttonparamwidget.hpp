/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include <QWidget>

class QPushButton;
class QProgressBar;

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class ButtonParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    ButtonParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    /** @brief Returns the current value of the parameter
     */
    bool getValue();

public Q_SLOTS:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;
    QLabel *createLabel() override;

private:
    QPushButton *m_button;
    QProgressBar *m_progress;
    QString m_keyParam;
    QString m_buttonName;
    QString m_alternatebuttonName;
    QString m_conditionalText;
    bool m_displayConditional;
    bool m_animated;
};

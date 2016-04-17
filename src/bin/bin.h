/*
Copyright (C) 2012  Till Theato <root@ttill.de>
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

#ifndef KDENLIVE_BIN_H
#define KDENLIVE_BIN_H

#include "abstractprojectitem.h"
#include "timecode.h"

#include <KMessageWidget>

#include <QWidget>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDomElement>
#include <QPushButton>
#include <QUrl>
#include <QListView>
#include <QFuture>
#include <QMutex>
#include <QLineEdit>

class KdenliveDoc;
class QVBoxLayout;
class QScrollArea;
class ClipController;
class QDockWidget;
class QTimeLine;
class KToolBar;
class QMenu;
class QToolButton;
class QUndoCommand;
class ProjectItemModel;
class ProjectClip;
class ProjectFolder;
class AbstractProjectItem;
class Monitor;
class ProjectSortProxyModel;
class JobManager;
class ProjectFolderUp;
class InvalidDialog;

namespace Mlt {
  class Producer;
};


class MyListView: public QListView
{
    Q_OBJECT
public:
    explicit MyListView(QWidget *parent = 0);

protected:
    void focusInEvent(QFocusEvent *event);
signals:
    void focusView();
};

class MyTreeView: public QTreeView
{
    Q_OBJECT
public:
    explicit MyTreeView(QWidget *parent = 0);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent * event);
    void focusInEvent(QFocusEvent *event);

private:
    QPoint m_startPos;
    bool performDrag();

signals:
    void focusView();
};

class BinMessageWidget: public KMessageWidget
{
    Q_OBJECT
public:
    explicit BinMessageWidget(QWidget *parent = 0);
    BinMessageWidget(const QString &text, QWidget *parent = 0);

protected:
    bool event(QEvent* ev);

signals:
    void messageClosing();
};


class SmallJobLabel: public QPushButton
{
    Q_OBJECT
public:
    explicit SmallJobLabel(QWidget *parent = 0);
    static const QString getStyleSheet(const QPalette &p);
    void setAction(QAction *action);
private:
    enum ItemRole {
        NameRole = Qt::UserRole,
        DurationRole,
        UsageRole
    };

    QTimeLine* m_timeLine;
    QAction *m_action;

public slots:
    void slotSetJobCount(int jobCount);

private slots:
    void slotTimeLineChanged(qreal value);
    void slotTimeLineFinished();
};


/**
 * @class BinItemDelegate
 * @brief This class is responsible for drawing items in the QTreeView.
 */

class BinItemDelegate: public QStyledItemDelegate
{
public:
    explicit BinItemDelegate(QObject* parent = 0): QStyledItemDelegate(parent) 
    {
    }

    void updateEditorGeometry(QWidget * editor, const QStyleOptionViewItem & option, const QModelIndex & index) const
    {
        if (index.column() != 0) {
            return QStyledItemDelegate::updateEditorGeometry(editor, option, index);
        }
        QStyleOptionViewItemV4 opt = option;
        initStyleOption(&opt, index);
        QRect r1 = option.rect;
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        double factor = (double) opt.decorationSize.height() / r1.height();
        int decoWidth = 2 * textMargin;
        int mid = 0;
        if (factor != 0) {
            decoWidth += opt.decorationSize.width() / factor;
        }
        if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
            mid = (int)((r1.height() / 2));
        }
        r1.adjust(decoWidth, 0, 0, -mid);
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QRect r2 =fm.boundingRect(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString()).toRect();
        editor->setGeometry( r2 );
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        QString text = index.data(AbstractProjectItem::DataName).toString();
        QRectF r = option.rect;
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int width = fm.boundingRect(r, Qt::AlignLeft | Qt::AlignTop, text).width() + option.decorationSize.width() + 2 * textMargin;
        hint.setWidth(width);
        int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
        if (type == AbstractProjectItem::FolderItem || type == AbstractProjectItem::FolderUpItem) {
            return QSize(hint.width(), qMin(option.fontMetrics.lineSpacing() + 4, hint.height()));
        }
        if (type == AbstractProjectItem::ClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMax(hint.height(), option.decorationSize.height())));
        }
        if (type == AbstractProjectItem::SubClipItem) {
            return QSize(hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMin(hint.height(), (int) (option.decorationSize.height() / 1.5))));
        }
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QString line1 = index.data(Qt::DisplayRole).toString();
        QString line2 = index.data(Qt::UserRole).toString();

        int textW = qMax(option.fontMetrics.width(line1), option.fontMetrics.width(line2));
        QSize iconSize = icon.actualSize(option.decorationSize);
        return QSize(qMax(textW, iconSize.width()) + 4, option.fontMetrics.lineSpacing() * 2 + 4);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (index.column() == 0 && !index.data().isNull()) {
            QRect r1 = option.rect;
            painter->save();
            painter->setClipRect(r1);
            QStyleOptionViewItemV4 opt(option);
            initStyleOption(&opt, index);
            int type = index.data(AbstractProjectItem::ItemTypeRole).toInt();
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            //QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft, opt.decorationSize, r1);

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
            }
            else painter->setPen(option.palette.text().color());
            QRect r = r1;
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            if (type == AbstractProjectItem::ClipItem || type == AbstractProjectItem::SubClipItem) {
                double factor = (double) opt.decorationSize.height() / r1.height();
                int decoWidth = 2 * textMargin;
                if (factor != 0) {
                    r.setWidth(opt.decorationSize.width() / factor);
                    // Draw thumbnail
                    opt.icon.paint(painter, r);
                    decoWidth += r.width();
                }
                int mid = (int)((r1.height() / 2));
                r1.adjust(decoWidth, 0, 0, -mid);
                QRect r2 = option.rect;
                r2.adjust(decoWidth, mid, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
                font.setBold(false);
                painter->setFont(font);
                QString subText = index.data(AbstractProjectItem::DataDuration).toString();
                if (!subText.isEmpty()) {
                    r2.adjust(0, bounding.bottom() - r2.top(), 0, 0);
                    QColor subTextColor = painter->pen().color();
                    subTextColor.setAlphaF(.5);
                    painter->setPen(subTextColor);
                    painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop , subText, &bounding);
                    // Draw usage counter
                    int usage = index.data(AbstractProjectItem::UsageCount).toInt();
                    if (usage > 0) {
                        bounding.moveLeft(bounding.right() + (2 * textMargin));
                        QString us = QString().sprintf("[%d]", usage);
                        painter->drawText(bounding, Qt::AlignLeft | Qt::AlignTop , us, &bounding);
                    }
                }
                if (type == AbstractProjectItem::ClipItem) {
                    // Overlay icon if necessary
                    QVariant v = index.data(AbstractProjectItem::IconOverlay);
                    if (!v.isNull()) {
                        QIcon reload = QIcon::fromTheme(v.toString());
                        r.setTop(r.bottom() - bounding.height());
                        r.setWidth(bounding.height());
                        reload.paint(painter, r);
                    }

                    int jobProgress = index.data(AbstractProjectItem::JobProgress).toInt();
                    if (jobProgress > 0 || jobProgress == JobWaiting) {
                        // Draw job progress bar
                        QColor color = option.palette.alternateBase().color();
                        painter->setPen(Qt::NoPen);
                        color.setAlpha(180);
                        painter->setBrush(QBrush(color));
                        QRect progress(r1.x() + 1, opt.rect.bottom() - 12, r1.width() / 2, 8);
                        painter->drawRect(progress);
                        painter->setBrush(option.palette.text());
                        if (jobProgress > 0) {
                            progress.adjust(1, 1, 0, -1);
                            progress.setWidth((progress.width() - 4) * jobProgress / 100);
                            painter->drawRect(progress);
                        } else if (jobProgress == JobWaiting) {
                            // Draw kind of a pause icon
                            progress.adjust(1, 1, 0, -1);
                            progress.setWidth(2);
                            painter->drawRect(progress);
                            progress.moveLeft(progress.right() + 2);
                            painter->drawRect(progress);
                        }
                    } else if (jobProgress == JobCrashed) {
                        QString jobText = index.data(AbstractProjectItem::JobMessage).toString();
                        if (!jobText.isEmpty()) {
                            QRectF txtBounding = painter->boundingRect(r2, Qt::AlignRight | Qt::AlignVCenter, " " + jobText + " ");
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(option.palette.highlight());
                            painter->drawRoundedRect(txtBounding, 2, 2);
                            painter->setPen(option.palette.highlightedText().color());
                            painter->drawText(txtBounding, Qt::AlignCenter, jobText);
                        }
                    }
                }
            }
            else {
                // Folder or Folder Up items
                double factor = (double) opt.decorationSize.height() / r1.height();
                int decoWidth = 2 * textMargin;
                if (factor != 0) {
                    r.setWidth(opt.decorationSize.width() / factor);
                    // Draw thumbnail
                    opt.icon.paint(painter, r);
                    decoWidth += r.width();
                }
                r1.adjust(decoWidth, 0, 0, 0);
                QRectF bounding;
                painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(AbstractProjectItem::DataName).toString(), &bounding);
            }
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

class LineEventEater : public QObject
{
    Q_OBJECT
public:
    explicit LineEventEater(QObject *parent = 0);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

signals:
    void clearSearchLine();
};


/**
 * @class Bin
 * @brief The bin widget takes care of both item model and view upon project opening.
 */

class Bin : public QWidget
{
    Q_OBJECT

    /** @brief Defines the view types (icon view, tree view,...)  */
    enum BinViewType {BinTreeView, BinIconView };

public:
    explicit Bin(QWidget* parent = 0);
    ~Bin();

    bool isLoading;

    /** @brief Sets the document for the bin and initialize some stuff  */
    void setDocument(KdenliveDoc *project);

    /** @brief Returns the root folder, which is the parent for all items in the view */
    ProjectFolder *rootFolder();

    /** @brief Create a clip item from its xml description  */
    void createClip(QDomElement xml);

    /** @brief Used to notify the Model View that an item was updated */
    void emitItemUpdated(AbstractProjectItem* item);

    /** @brief Set monitor associated with this bin (clipmonitor) */
    void setMonitor(Monitor *monitor);

    /** @brief Returns the clip monitor */
    Monitor *monitor();

    /** @brief Open a producer in the clip monitor */
    void openProducer(ClipController *controller);
    void openProducer(ClipController *controller, int in, int out);

    /** @brief Trigger deletion of an item */
    void deleteClip(const QString &id);

    /** @brief Get a clip from it's id */
    ProjectClip *getBinClip(const QString &id);

    /** @brief Returns a list of selected clips  */
    QList <ProjectClip *> selectedClips();

    /** @brief Start a job of selected type for a clip  */
    void startJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Discard jobs from a chosen type, use NOJOBTYPE to discard all jobs for this clip */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Check if there is a job waiting / running for this clip  */
    bool hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Reload / replace a producer */
    void reloadProducer(const QString &id, QDomElement xml);
    
    /** @brief Current producer has changed, refresh monitor and timeline*/
    void refreshClip(const QString &id);

    /** @brief Some stuff used to notify the Item Model */
    void emitAboutToAddItem(AbstractProjectItem* item);
    void emitItemAdded(AbstractProjectItem* item);
    void emitAboutToRemoveItem(AbstractProjectItem* item);
    void emitItemRemoved(AbstractProjectItem* item);
    void setupMenu(QMenu *addMenu, QAction *defaultAction, QHash <QString, QAction*> actions);

    /** @brief The source file was modified, we will reload it soon, disable item in the meantime */
    void setWaitingStatus(const QString &id);


    const QString getDocumentProperty(const QString &key);

    /** @brief A proxy clip was just created, pass it to the responsible item  */
    void gotProxy(const QString &id);

    /** @brief Get the document's renderer frame size  */
    const QSize getRenderSize();

    /** @brief Give a number available for a clip id, used when adding a new clip to the project. Id must be unique */
    int getFreeClipId();
    
    /** @brief Give a number available for a folder id, used when adding a new folder to the project. Id must be unique */
    int getFreeFolderId();

    /** @brief Returns the id of the last inserted clip */
    int lastClipId() const;
    
    /** @brief Ask MLT to reload this clip's producer  */
    void reloadClip(const QString &id);
    
    /** @brief Delete a folder  */
    void doRemoveFolder(const QString &id);
    /** @brief Add a folder  */
    void doAddFolder(const QString &id, const QString &name, const QString &parentId);
    void removeFolder(const QString &id, QUndoCommand *deleteCommand);
    void removeSubClip(const QString &id, QUndoCommand *deleteCommand);
    void doMoveClip(const QString &id, const QString &newParentId);
    void doMoveFolder(const QString &id, const QString &newParentId);
    void setupGeneratorMenu();
    void startClipJob(const QStringList &params);
    void droppedUrls(QList <QUrl> urls);

    void addClipCut(const QString&id, int in, int out);
    void removeClipCut(const QString&id, int in, int out);

    /** @brief Create the subclips defined in the parent clip. */
    void loadSubClips(const QString&id, const QMap <QString,QString> data);

    /** @brief Set focus to the Bin view. */
    void focusBinView() const;
    /** @brief Get a string list of all clip ids that are inside a folder defined by id. */
    QStringList getBinFolderClipIds(const QString &id) const;
    /** @brief Build a rename folder command. */
    void renameFolderCommand(const QString &id, const QString &newName, const QString &oldName);
    /** @brief Rename a folder and store new name in MLT. */
    void renameFolder(const QString &id, const QString &name);
    /** @brief Build a rename subclip command. */
    void renameSubClipCommand(const QString &id, const QString &newName, const QString oldName, int in, int out);
    /** @brief Rename a clip zone (subclip). */
    void renameSubClip(const QString &id, const QString &newName, const QString oldName, int in, int out);
    /** @brief Returns current project's timecode. */
    Timecode projectTimecode() const;
    /** @brief Trigger timecode format refresh where needed. */
    void updateTimecodeFormat();
    /** @brief If clip monitor is displaying clip with id @param id, refresh markers. */
    void refreshClipMarkers(const QString &id);
    /** @brief Delete a clip marker. */
    void deleteClipMarker(const QString &comment, const QString &id, const GenTime &position);
    /** @brief Delete all markers from @param id clip. */
    void deleteAllClipMarkers(const QString &id);
    /** @brief Remove an effect from a bin clip. */
    void removeEffect(const QString &id, const QDomElement &effect);
    /** @brief Add an effect to a bin clip. */
    void addEffect(const QString &id, QDomElement &effect);
    /** @brief Edit an effect settings to a bin clip. */
    void editMasterEffect(ClipController *ctl);
    /** @brief An effect setting was changed, update stack if displayed. */
    void updateMasterEffect(ClipController *ctl);
    /** @brief Returns current project's folder for storing items. */
    QUrl projectFolder() const;
    /** @brief Display a message about an operation in status bar. */
    void emitMessage(const QString &, MessageType);
    void rebuildMenu();
    void refreshIcons();
    /** @brief Update status of disable effects action (when loading a document). */
    void setBinEffectsDisabledStatus(bool disabled);

    void requestAudioThumbs(const QString &id);
    /** @brief Proxy status for the project changed, update. */
    void refreshProxySettings();
    /** @brief A clip is ready, update its info panel if displayed. */
    void emitRefreshPanel(const QString &id);
    /** @brief Audio thumbs just finished creating, update on monitor display. */
    void emitRefreshAudioThumbs(const QString &id);
    /** @brief Returns true if there is no clip. */
    bool isEmpty() const;
    /** @brief Trigger reload of all clips. */
    void reloadAllProducers();
    /** @brief Remove all unused clip from project bin. */
    void cleanup();
    /** @brief Get usage stats for project bin. */
    void getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize);
    /** @brief Returns the clip properties dockwidget. */
    QDockWidget *clipPropertiesDock();

private slots:
    void slotAddClip();
    void slotReloadClip();
    /** @brief Set sorting column */
    void slotSetSorting();
    /** @brief Show/hide date column */
    void slotShowDateColumn(bool show);
    void slotShowDescColumn(bool show);

    /** @brief Setup the bin view type (icon view, tree view, ...).
    * @param action The action whose data defines the view type or NULL to keep default view */
    void slotInitView(QAction *action);

    /** @brief Update status for clip jobs  */
    void slotUpdateJobStatus(const QString&, int, int, const QString &label = QString(), const QString &actionName = QString(), const QString &details = QString());
    void slotSetIconSize(int size);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void rowsRemoved(const QModelIndex &parent, int start, int end);
    void selectProxyModel(const QModelIndex &id);
    void autoSelect();
    void slotSaveHeaders();
    void slotItemDropped(QStringList ids, const QModelIndex &parent);
    void slotItemDropped(const QList<QUrl>&urls, const QModelIndex &parent);
    void slotEffectDropped(QString effect, const QModelIndex &parent);
    void slotItemEdited(QModelIndex,QModelIndex,QVector<int>);
    void slotAddUrl(QString url, QMap <QString, QString> data = QMap <QString, QString>());
    void slotPrepareJobsMenu();
    void slotShowJobLog();
    /** @brief process clip job result. */
    void slotGotFilterJobResults(QString ,int , int, stringMap, stringMap);
    /** @brief Reset all text and log data from info message widget. */
    void slotResetInfoMessage();
    /** @brief Show dialog prompting for removal of invalid clips. */
    void slotQueryRemoval(const QString &id, QUrl url, const QString &errorMessage);
    /** @brief Request display of current clip in monitor. */
    void slotOpenCurrent();
    void slotZoomView(bool zoomIn);
    /** @brief Widget gained focus, make sure we display effects for master clip. */
    void slotGotFocus();
    /** @brief Dis/Enable all bin effects. */
    void slotDisableEffects(bool disable);
    /** @brief Rename a Bin Folder. */
    void slotRenameFolder();
    void slotCreateAudioThumbs();
    void doRefreshPanel(const QString &id);
    /** @brief Send audio thumb data to monitor for display. */
    void slotSendAudioThumb(QString id);
    void doRefreshAudioThumbs(const QString &id);
    /** @brief Enable item view and hide message */
    void slotMessageActionTriggered();
    /** @brief Request editing of title or slideshow clip */
    void slotEditClip();

public slots:
    void slotThumbnailReady(const QString &id, const QImage &img, bool fromFile = false);
    /** @brief The producer for this clip is ready.
     *  @param id the clip id
     *  @param controller The Controller for this clip
     */
    void slotProducerReady(requestClipInfo info, ClipController *controller);
    void slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage);
    /** @brief Create a folder when opening a document */
    void slotLoadFolders(QMap<QString,QString> foldersData);
    /** @brief Reload clip thumbnail - when frame for thumbnail changed */
    void slotRefreshClipThumbnail(const QString &id);
    void slotDeleteClip();
    void slotRefreshClipProperties();
    void slotItemDoubleClicked(const QModelIndex &ix, const QPoint pos);
    void slotSwitchClipProperties(const QModelIndex &ix);
    void slotSwitchClipProperties(bool display = true);
    /** @brief Creates a new folder with optional name, and returns new folder's id */
    QString slotAddFolder(const QString &folderName = QString());
    void slotCreateProjectClip();
    /** @brief Start a Cut Clip job on this clip (extract selected zone using FFmpeg) */
    void slotStartCutJob(const QString &id);
    /** @brief Triggered by a clip job action, start the job */
    void slotStartClipJob(bool enable);
    void slotEditClipCommand(const QString &id, QMap<QString, QString>oldProps, QMap<QString, QString>newProps);
    void slotCancelRunningJob(const QString &id, const QMap<QString, QString> &newProps);
    /** @brief Start a filter job requested by a filter applied in timeline */
    void slotStartFilterJob(const ItemInfo &info, const QString&id, QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams);
    /** @brief Add a sub clip */
    void slotAddClipCut(const QString&id, int in, int out);
    /** @brief Open current clip in an external editing application */
    void slotOpenClip();
    void slotAddClipMarker(const QString &id, QList <CommentedTime> newMarker, QUndoCommand *groupCommand = 0);
    void slotLoadClipMarkers(const QString &id);
    void slotSaveClipMarkers(const QString &id);
    void slotDuplicateClip();
    void slotDeleteEffect(const QString &id, QDomElement effect);
    /** @brief Request audio thumbnail for clip with id */
    void slotCreateAudioThumb(const QString &id);
    /** @brief Abort audio thumbnail for clip with id */
    void slotAbortAudioThumb(const QString &id);
    /** @brief Add extra data to a clip. */
    void slotAddClipExtraData(const QString &id, const QString &key, const QString &data = QString(), QUndoCommand *groupCommand = 0);
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties, bool refreshPropertiesPanel);
    /** @brief Pass some important properties to timeline track producers. */
    void updateTimelineProducers(const QString &id, QMap <QString, QString> passProperties);
    /** @brief Add effect to active Bin clip (used when double clicking an effect in list). */
    void slotEffectDropped(QString id, QDomElement);
    /** @brief Request current frame from project monitor. */
    void slotGetCurrentProjectImage();
    void slotExpandUrl(ItemInfo info, QUrl url, QUndoCommand *command);
    void abortAudioThumbs();
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, QList <QAction*> actions = QList <QAction*>());
    /** @brief Reset all clip usage to 0 */
    void resetUsageCount();
        /** @brief Select a clip in the Bin from its id. */
    void selectClipById(const QString &id, int frame = -1, const QPoint &zone = QPoint());
    void slotAddClipToProject(QUrl url);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
    ProjectFolder *m_rootFolder;
    /** @brief An "Up" item that is inserted in bin when using icon view so that user can navigate up */
    ProjectFolderUp *m_folderUp;
    BinItemDelegate *m_binTreeViewDelegate;
    ProjectSortProxyModel *m_proxyModel;
    JobManager *m_jobManager;
    KToolBar *m_toolbar;
    KdenliveDoc* m_doc;
    QToolButton *m_addButton;
    QMenu *m_extractAudioAction;
    QMenu *m_transcodeAction;
    QMenu *m_clipsActionsMenu;
    QAction *m_inTimelineAction;
    QAction *m_showDate;
    QAction *m_showDesc;
    /** @brief Holds an available unique id for a clip to be created */
    int m_clipCounter;
    /** @brief Holds an available unique id for a folder to be created */
    int m_folderCounter;
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
    QVBoxLayout *m_layout;
    QDockWidget *m_propertiesDock;
    QScrollArea *m_propertiesPanel;
    QSlider *m_slider;
    Monitor *m_monitor;
    QIcon m_blankThumb;
    QMenu *m_menu;
    QAction *m_openAction;
    QAction *m_reloadAction;
    QAction *m_duplicateAction;
    QAction *m_proxyAction;
    QAction *m_editAction;
    QAction *m_deleteAction;
    QAction *m_renameFolderAction;
    QMenu *m_jobsMenu;
    QAction *m_cancelJobs;
    QAction *m_discardCurrentClipJobs;
    SmallJobLabel *m_infoLabel;
    /** @brief The info widget for failed jobs. */
    BinMessageWidget *m_infoMessage;
    /** @brief The action that will trigger the log dialog. */
    QAction *m_logAction;
    QStringList m_errorLog;
    InvalidDialog *m_invalidClipDialog;
    /** @brief Set to true if widget just gained focus (means we have to update effect stack . */
    bool m_gainedFocus;
    /** @brief List of Clip Ids that want an audio thumb. */
    QStringList m_audioThumbsList;
    QString m_processingAudioThumb;
    QMutex m_audioThumbMutex;
    /** @brief Indicates whether audio thumbnail creation is running. */
    QFuture<void> m_audioThumbsThread;
    void showClipProperties(ProjectClip *clip, bool forceRefresh = false, bool openExternalDialog = true);
    const QStringList getFolderInfo(QModelIndex selectedIx = QModelIndex());
    /** @brief Get the QModelIndex value for an item in the Bin. */
    QModelIndex getIndexForId(const QString &id, bool folderWanted) const;
    /** @brief Get a Clip item from its id. */
    AbstractProjectItem *getClipForId(const QString &id) const;
    ProjectClip *getFirstSelectedClip();
    void showTitleWidget(ProjectClip *clip);
    void showSlideshowWidget(ProjectClip *clip);
    void processAudioThumbs();

signals:
    void itemUpdated(AbstractProjectItem*);
    void producerReady(const QString &id);
    /** @brief Save folder info into MLT. */
    void storeFolder(QString folderId, QString parentId, QString oldParentId, QString folderName);
    void gotFilterJobResults(QString,int,int,stringMap,stringMap);
    /** @brief The clip was changed and thumbnail needs a refresh. */
    void clipNeedsReload(const QString &,bool);
    /** @brief Trigger timecode format refresh where needed. */
    void refreshTimeCode();
    /** @brief Request display of effect stack for a Bin clip. */
    void masterClipSelected(ClipController *, Monitor *);
    /** @brief Request updating of the effect stack if currently displayed. */
    void masterClipUpdated(ClipController *, Monitor *);
    void displayBinMessage(const QString &, KMessageWidget::MessageType);
    void displayMessage(const QString &, MessageType);
    void requesteInvalidRemoval(const QString &, QUrl, const QString &);
    /** @brief Markers changed, refresh panel. */
    void refreshPanelMarkers();
    /** @brief Analysis data changed, refresh panel. */
    void updateAnalysisData(const QString &);
    void openClip(ClipController *c, int in = -1, int out = -1);
    /** @brief Fill context menu with occurences of this clip in timeline. */
    void findInTimeline(const QString &);
    void clipNameChanged(const QString &);
    /** @brief A clip was updated, request panel update. */
    void refreshPanel(const QString &id);
    /** @brief A clip audio data was updated, request refresh. */
    void refreshAudioThumbs(const QString &id);

};

#endif

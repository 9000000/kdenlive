/***************************************************************************
                          stopmotion.cpp  -  description
                             -------------------
    begin                : Feb 28 2008
    copyright            : (C) 2010 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "stopmotion.h"
#include "../blackmagic/devices.h"
#include "../v4l/v4lcapture.h"
#include "../slideshowclip.h"
#include "kdenlivesettings.h"


#include <KDebug>
#include <KGlobalSettings>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>
#include <kdeversion.h>
#include <KNotification>

#ifdef QIMAGEBLITZ
#include <qimageblitz/qimageblitz.h>
#endif

#include <QtConcurrentRun>
#include <QInputDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QAction>
#include <QWheelEvent>
#include <QMenu>

MyLabel::MyLabel(QWidget *parent) :
    QLabel(parent)
{
}

void MyLabel::setImage(QImage img)
{
    m_img = img;
}

//virtual
void MyLabel::wheelEvent(QWheelEvent * event)
{
    if (event->delta() > 0) emit seek(true);
    else emit seek(false);
}

//virtual
void MyLabel::mousePressEvent(QMouseEvent *)
{
    emit switchToLive();
}

//virtual
void MyLabel::paintEvent(QPaintEvent * event)
{
    Q_UNUSED(event);

    QRect r(0, 0, width(), height());
    QPainter p(this);
    p.fillRect(r, QColor(KdenliveSettings::window_background()));
    double aspect_ratio = (double) m_img.width() / m_img.height();
    int pictureHeight = height();
    int pictureWidth = width();
    int calculatedWidth = aspect_ratio * height();
    if (calculatedWidth > width()) pictureHeight = width() / aspect_ratio;
    else {
        int calculatedHeight = width() / aspect_ratio;
        if (calculatedHeight > height()) pictureWidth = height() * aspect_ratio;
    }
    p.drawImage(QRect((width() - pictureWidth) / 2, (height() - pictureHeight) / 2, pictureWidth, pictureHeight), m_img, QRect(0, 0, m_img.width(), m_img.height()));
    p.end();
}


StopmotionWidget::StopmotionWidget(KUrl projectFolder, const QList< QAction * > actions, QWidget *parent) :
    QDialog(parent)
    , Ui::Stopmotion_UI()
    , m_projectFolder(projectFolder)
    , m_bmCapture(NULL)
    , m_sequenceFrame(0)
    , m_animatedIndex(-1)
{
    //setAttribute(Qt::WA_DeleteOnClose);
    addActions(actions);
    setupUi(this);
    setWindowTitle(i18n("Stop Motion Capture"));
    setFont(KGlobalSettings::toolBarFont());

    live_button->setIcon(KIcon("camera-photo"));

    m_captureAction = actions.at(0);
    connect(m_captureAction, SIGNAL(triggered()), this, SLOT(slotCaptureFrame()));
    capture_button->setDefaultAction(m_captureAction);

    connect(actions.at(1), SIGNAL(triggered()), this, SLOT(slotSwitchLive()));

    preview_button->setIcon(KIcon("media-playback-start"));
    capture_button->setEnabled(false);

    // Build config menu
    QMenu *confMenu = new QMenu;
    m_showOverlay = actions.at(2);
    connect(m_showOverlay, SIGNAL(triggered(bool)), this, SLOT(slotShowOverlay(bool)));
    confMenu->addAction(m_showOverlay);

#ifdef QIMAGEBLITZ
    m_effectIndex = KdenliveSettings::blitzeffect();
    QMenu *effectsMenu = new QMenu(i18n("Overlay effect"));
    QActionGroup *effectGroup = new QActionGroup(this);
    QAction *noEffect = new QAction(i18n("No Effect"), effectGroup);
    noEffect->setData(1);
    QAction *contrastEffect = new QAction(i18n("Contrast"), effectGroup);
    contrastEffect->setData(2);
    QAction *edgeEffect = new QAction(i18n("Edge detect"), effectGroup);
    edgeEffect->setData(3);
    QAction *brightEffect = new QAction(i18n("Brighten"), effectGroup);
    brightEffect->setData(4);
    QAction *invertEffect = new QAction(i18n("Invert"), effectGroup);
    invertEffect->setData(5);
    QAction *thresEffect = new QAction(i18n("Threshold"), effectGroup);
    thresEffect->setData(6);

    effectsMenu->addAction(noEffect);
    effectsMenu->addAction(contrastEffect);
    effectsMenu->addAction(edgeEffect);
    effectsMenu->addAction(brightEffect);
    effectsMenu->addAction(invertEffect);
    effectsMenu->addAction(thresEffect);
    QList <QAction *> list = effectsMenu->actions();
    for (int i = 0; i < list.count(); i++) {
        list.at(i)->setCheckable(true);
        if (list.at(i)->data().toInt() == m_effectIndex) {
            list.at(i)->setChecked(true);
        }
    }
    connect(effectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotUpdateOverlayEffect(QAction*)));
    confMenu->addMenu(effectsMenu);
#endif

    QAction *showThumbs = new QAction(KIcon("image-x-generic"), i18n("Show sequence thumbnails"), this);
    showThumbs->setCheckable(true);
    showThumbs->setChecked(KdenliveSettings::showstopmotionthumbs());
    connect(showThumbs, SIGNAL(triggered(bool)), this, SLOT(slotShowThumbs(bool)));

    QAction *removeCurrent = new QAction(KIcon("edit-delete"), i18n("Delete current frame"), this);
    removeCurrent->setShortcut(Qt::Key_Delete);
    connect(removeCurrent, SIGNAL(triggered()), this, SLOT(slotRemoveFrame()));

    QAction *capInterval = new QAction(KIcon(), i18n("Set capture interval"), this);
    connect(capInterval, SIGNAL(triggered()), this, SLOT(slotSetCaptureInterval()));

    confMenu->addAction(showThumbs);
    confMenu->addAction(capInterval);
    confMenu->addAction(removeCurrent);
    config_button->setIcon(KIcon("configure"));
    config_button->setMenu(confMenu);

    // Build capture menu
    QMenu *capMenu = new QMenu;
    m_intervalCapture = new QAction(KIcon("edit-redo"), i18n("Interval capture"), this);
    m_intervalCapture->setCheckable(true);
    m_intervalCapture->setChecked(false);
    connect(m_intervalCapture, SIGNAL(triggered(bool)), this, SLOT(slotIntervalCapture(bool)));
    capMenu->addAction(m_intervalCapture);
    capture_button->setMenu(capMenu);

    connect(sequence_name, SIGNAL(textChanged(const QString &)), this, SLOT(sequenceNameChanged(const QString &)));
    connect(sequence_name, SIGNAL(currentIndexChanged(int)), live_button, SLOT(setFocus()));

    m_layout = new QVBoxLayout;
    if (BMInterface::getBlackMagicDeviceList(capture_device, NULL)) {
        // Found a BlackMagic device
        m_bmCapture = new BmdCaptureHandler(m_layout);
        connect(m_bmCapture, SIGNAL(gotMessage(const QString &)), this, SLOT(slotGotHDMIMessage(const QString &)));
    }
    if (QFile::exists(KdenliveSettings::video4vdevice())) {
#if !defined(Q_WS_MAC) && !defined(Q_OS_FREEBSD)
        V4lCaptureHandler v4l(NULL);
        // Video 4 Linux device detection
        for (int i = 0; i < 10; i++) {
            QString path = "/dev/video" + QString::number(i);
            if (QFile::exists(path)) {
                QStringList deviceInfo = v4l.getDeviceName(path);
                if (!deviceInfo.isEmpty()) {
                    capture_device->addItem(deviceInfo.at(0), "v4l");
                    capture_device->setItemData(capture_device->count() - 1, path, Qt::UserRole + 1);
                    capture_device->setItemData(capture_device->count() - 1, deviceInfo.at(1), Qt::UserRole + 2);
                    if (path == KdenliveSettings::video4vdevice()) capture_device->setCurrentIndex(capture_device->count() - 1);
                }
            }
        }

        /*V4lCaptureHandler v4lhandler(NULL);
        QStringList deviceInfo = v4lhandler.getDeviceName(KdenliveSettings::video4vdevice());
            capture_device->addItem(deviceInfo.at(0), "v4l");
        capture_device->setItemData(capture_device->count() - 1, deviceInfo.at(3), Qt::UserRole + 1);*/
        if (m_bmCapture == NULL) {
            m_bmCapture = new V4lCaptureHandler(m_layout);
            m_bmCapture->setDevice(capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString(), capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 2).toString());
        }
#endif
    }

    connect(capture_device, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateHandler()));
    if (m_bmCapture) {
        connect(m_bmCapture, SIGNAL(frameSaved(const QString)), this, SLOT(slotNewThumb(const QString)));
    } else live_button->setEnabled(false);
    m_frame_preview = new MyLabel(this);
    connect(m_frame_preview, SIGNAL(seek(bool)), this, SLOT(slotSeekFrame(bool)));
    connect(m_frame_preview, SIGNAL(switchToLive()), this, SLOT(slotSwitchLive()));
    m_layout->addWidget(m_frame_preview);
    m_frame_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    video_preview->setLayout(m_layout);
    live_button->setChecked(false);
    button_addsequence->setEnabled(false);
    connect(live_button, SIGNAL(clicked(bool)), this, SLOT(slotLive(bool)));
    connect(button_addsequence, SIGNAL(clicked(bool)), this, SLOT(slotAddSequence()));
    connect(preview_button, SIGNAL(clicked(bool)), this, SLOT(slotPlayPreview(bool)));
    connect(frame_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotShowSelectedFrame()));
    connect(frame_list, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(slotShowSelectedFrame()));
    connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));

    frame_list->addAction(removeCurrent);
    frame_list->setContextMenuPolicy(Qt::ActionsContextMenu);
    frame_list->setHidden(!KdenliveSettings::showstopmotionthumbs());
    parseExistingSequences();
}

StopmotionWidget::~StopmotionWidget()
{
    if (m_bmCapture)
        m_bmCapture->stopPreview();
}

void StopmotionWidget::slotUpdateOverlayEffect(QAction *act)
{
#ifdef QIMAGEBLITZ
    if (act) m_effectIndex = act->data().toInt();
    KdenliveSettings::setBlitzeffect(m_effectIndex);
    if (m_showOverlay->isChecked()) slotUpdateOverlay();
#endif
}

void StopmotionWidget::closeEvent(QCloseEvent *e)
{
    slotLive(false);
    QDialog::closeEvent(e);
}

void StopmotionWidget::slotSetCaptureInterval()
{
    int interval = QInputDialog::getInteger(this, i18n("Set Capture Interval"), i18n("Interval (in seconds)"), KdenliveSettings::captureinterval(), 1);
    if (interval > 0 && interval != KdenliveSettings::captureinterval())
        KdenliveSettings::setCaptureinterval(interval);
}

void StopmotionWidget::slotShowThumbs(bool show)
{
    KdenliveSettings::setShowstopmotionthumbs(show);
    if (show) {
        frame_list->clear();
        sequenceNameChanged(sequence_name->currentText());
    } else {
        m_filesList.clear();
        frame_list->clear();
    }
    frame_list->setHidden(!show);
}

void StopmotionWidget::slotIntervalCapture(bool capture)
{
    if (capture) slotCaptureFrame();
}


void StopmotionWidget::slotUpdateHandler()
{
    QString data = capture_device->itemData(capture_device->currentIndex()).toString();
    slotLive(false);
    if (m_bmCapture) {
        delete m_bmCapture;
    }
    m_layout->removeWidget(m_frame_preview);
    if (data == "v4l") {
#if !defined(Q_WS_MAC) && !defined(Q_OS_FREEBSD)
        m_bmCapture = new V4lCaptureHandler(m_layout);
        m_bmCapture->setDevice(capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 1).toString(), capture_device->itemData(capture_device->currentIndex(), Qt::UserRole + 2).toString());
#endif
    } else {
        m_bmCapture = new BmdCaptureHandler(m_layout);
        if (m_bmCapture) connect(m_bmCapture, SIGNAL(gotMessage(const QString &)), this, SLOT(slotGotHDMIMessage(const QString &)));
    }
    live_button->setEnabled(m_bmCapture != NULL);
    m_layout->addWidget(m_frame_preview);
}

void StopmotionWidget::slotGotHDMIMessage(const QString &message)
{
    log_box->insertItem(0, message);
}

void StopmotionWidget::parseExistingSequences()
{
    sequence_name->clear();
    sequence_name->addItem(QString());
    QDir dir(m_projectFolder.path());
    QStringList filters;
    filters << "*_0000.png";
    //dir.setNameFilters(filters);
    QStringList sequences = dir.entryList(filters, QDir::Files, QDir::Name);
    //kDebug()<<"PF: "<<<<", sm: "<<sequences;
    foreach(QString sequencename, sequences) {
        sequence_name->addItem(sequencename.section("_", 0, -2));
    }
}

void StopmotionWidget::slotSwitchLive()
{
    setUpdatesEnabled(false);
    if (m_frame_preview->isHidden()) {
        if (m_bmCapture) m_bmCapture->hidePreview(true);
        m_frame_preview->setHidden(false);
    } else {
        m_frame_preview->setHidden(true);
        if (m_bmCapture) m_bmCapture->hidePreview(false);
        capture_button->setEnabled(true);
    }
    setUpdatesEnabled(true);
}

void StopmotionWidget::slotLive(bool isOn)
{
    if (isOn && m_bmCapture) {
        //m_frame_preview->setImage(QImage());
        m_frame_preview->setHidden(true);
        m_bmCapture->startPreview(KdenliveSettings::hdmi_capturedevice(), KdenliveSettings::hdmi_capturemode(), false);
        capture_button->setEnabled(true);
    } else {
        if (m_bmCapture) m_bmCapture->stopPreview();
        m_frame_preview->setHidden(false);
        capture_button->setEnabled(false);
        live_button->setChecked(false);
    }
}

void StopmotionWidget::slotShowOverlay(bool isOn)
{
    if (isOn) {
        if (live_button->isChecked() && m_sequenceFrame > 0) {
            slotUpdateOverlay();
        }
    } else if (m_bmCapture) {
        m_bmCapture->hideOverlay();
    }
}

void StopmotionWidget::slotUpdateOverlay()
{
    if (m_bmCapture == NULL) return;
    QString path = getPathForFrame(m_sequenceFrame - 1);
    if (!QFile::exists(path)) return;
    QImage img(path);
    if (img.isNull()) {
        QTimer::singleShot(1000, this, SLOT(slotUpdateOverlay()));
        return;
    }

#ifdef QIMAGEBLITZ
    //img = Blitz::convolveEdge(img, 0, Blitz::Low);
    switch (m_effectIndex) {
    case 2:
        img = Blitz::contrast(img, true, 6);
        break;
    case 3:
        img = Blitz::edge(img);
        break;
    case 4:
        img = Blitz::intensity(img, 0.5);
        break;
    case 5:
        Blitz::invert(img);
        break;
    case 6:
        img = Blitz::threshold(img, 120, Blitz::Grayscale, qRgba(0, 0, 0, 0), qRgba(255, 0, 0, 255));
        //img = Blitz::flatten(img, QColor(255, 0, 0, 255), QColor(0, 0, 0, 0));
        break;
    default:
        break;

    }
#endif
    m_bmCapture->showOverlay(img);
}

void StopmotionWidget::sequenceNameChanged(const QString &name)
{
    // Get rid of frames from previous sequence
    disconnect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
    m_filesList.clear();
    m_future.waitForFinished();
    frame_list->clear();
    if (name.isEmpty()) {
        button_addsequence->setEnabled(false);
    } else {
        // Check if we are editing an existing sequence
        QString pattern = SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->currentText()), false, QString(), &m_filesList);
        m_sequenceFrame = m_filesList.isEmpty() ? 0 : SlideshowClip::getFrameNumberFromPath(m_filesList.last()) + 1;
        if (!m_filesList.isEmpty()) {
            m_sequenceName = sequence_name->currentText();
            connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
            m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
            button_addsequence->setEnabled(true);
        } else {
            // new sequence
            connect(this, SIGNAL(doCreateThumbs(QImage, int)), this, SLOT(slotCreateThumbs(QImage, int)));
            button_addsequence->setEnabled(false);
        }
        capture_button->setEnabled(live_button->isChecked());
    }
}

void StopmotionWidget::slotCaptureFrame()
{
    if (m_bmCapture == NULL) return;
    if (sequence_name->currentText().isEmpty()) {
        QString seqName = QInputDialog::getText(this, i18n("Create New Sequence"), i18n("Enter sequence name"));
        if (seqName.isEmpty()) return;
        sequence_name->blockSignals(true);
        sequence_name->setItemText(sequence_name->currentIndex(), seqName);
        sequence_name->blockSignals(false);
    }
    if (m_sequenceName != sequence_name->currentText()) {
        m_sequenceName = sequence_name->currentText();
        m_sequenceFrame = 0;
    }
    //capture_button->setEnabled(false);
    QString currentPath = getPathForFrame(m_sequenceFrame);
    m_bmCapture->captureFrame(currentPath);
    KNotification::event("FrameCaptured");
    m_sequenceFrame++;
    button_addsequence->setEnabled(true);
    if (m_intervalCapture->isChecked()) QTimer::singleShot(KdenliveSettings::captureinterval() * 1000, this, SLOT(slotCaptureFrame()));
}


void StopmotionWidget::slotNewThumb(const QString path)
{
    if (!KdenliveSettings::showstopmotionthumbs()) return;
    m_filesList.append(path);
    if (m_showOverlay->isChecked()) slotUpdateOverlay();
    if (!m_future.isRunning()) m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
}

void StopmotionWidget::slotPrepareThumbs()
{
    if (m_filesList.isEmpty()) return;
    QString path = m_filesList.takeFirst();
    emit doCreateThumbs(QImage(path), SlideshowClip::getFrameNumberFromPath(path));

}

void StopmotionWidget::slotCreateThumbs(QImage img, int ix)
{
    if (img.isNull()) {
        m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
        return;
    }
    int height = 90;
    int width = height * img.width() / img.height();
    frame_list->setIconSize(QSize(width, height));
    QPixmap pix = QPixmap::fromImage(img).scaled(width, height);
    QString nb = QString::number(ix);
    QPainter p(&pix);
    QFontInfo finfo(font());
    p.fillRect(0, 0, finfo.pixelSize() * nb.count() + 6, finfo.pixelSize() + 6, QColor(80, 80, 80, 150));
    p.setPen(Qt::white);
    p.drawText(QPoint(3, finfo.pixelSize() + 3), nb);
    p.end();
    QIcon icon(pix);
    QListWidgetItem *item = new QListWidgetItem(icon, QString(), frame_list);
    item->setToolTip(getPathForFrame(ix, sequence_name->currentText()));
    item->setData(Qt::UserRole, ix);
    frame_list->blockSignals(true);
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
    m_future = QtConcurrent::run(this, &StopmotionWidget::slotPrepareThumbs);
}

QString StopmotionWidget::getPathForFrame(int ix, QString seqName)
{
    if (seqName.isEmpty()) seqName = m_sequenceName;
    return m_projectFolder.path(KUrl::AddTrailingSlash) + seqName + "_" + QString::number(ix).rightJustified(4, '0', false) + ".png";
}

void StopmotionWidget::slotShowFrame(const QString &path)
{
    //slotLive(false);
    QImage img(path);
    capture_button->setEnabled(false);
    if (!img.isNull()) {
        if (m_bmCapture) m_bmCapture->hidePreview(true);
        m_frame_preview->setImage(img);
        m_frame_preview->setHidden(false);
        m_frame_preview->update();
    }
}

void StopmotionWidget::slotShowSelectedFrame()
{
    QListWidgetItem *item = frame_list->currentItem();
    if (item) {
        //int ix = item->data(Qt::UserRole).toInt();;
        slotShowFrame(item->toolTip());
    }
}

void StopmotionWidget::slotAddSequence()
{
    emit addOrUpdateSequence(getPathForFrame(0));
}

void StopmotionWidget::slotPlayPreview(bool animate)
{
    if (!animate) {
        // stop animation
        m_animationList.clear();
        return;
    }
    if (KdenliveSettings::showstopmotionthumbs()) {
        frame_list->setCurrentRow(0);
        QTimer::singleShot(200, this, SLOT(slotAnimate()));
    } else {
        SlideshowClip::selectedPath(getPathForFrame(0, sequence_name->currentText()), false, QString(), &m_animationList);
        slotAnimate();
    }
}

void StopmotionWidget::slotAnimate()
{
    //slotShowFrame(m_animatedIndex);
    if (KdenliveSettings::showstopmotionthumbs()) {
        //TODO: loop
        if (frame_list->currentRow() < (frame_list->count() - 1)) {
            frame_list->setCurrentRow(frame_list->currentRow() + 1);
            QTimer::singleShot(100, this, SLOT(slotAnimate()));
        } else preview_button->setChecked(false);
    } else if (!m_animationList.isEmpty()) {
        slotShowFrame(m_animationList.takeFirst());
        QTimer::singleShot(100, this, SLOT(slotAnimate()));
    } else preview_button->setChecked(false);

}

QListWidgetItem *StopmotionWidget::getFrameFromIndex(int ix)
{
    QListWidgetItem *item = NULL;
    int pos = ix;
    if (ix >= frame_list->count()) {
        pos = frame_list->count() - 1;
    }
    if (ix < 0) pos = 0;
    item = frame_list->item(pos);

    int value = item->data(Qt::UserRole).toInt();
    if (value == ix) return item;
    else if (value < ix) {
        pos++;
        while (pos < frame_list->count()) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos++;
        }
    } else {
        pos --;
        while (pos >= 0) {
            item = frame_list->item(pos);
            value = item->data(Qt::UserRole).toInt();
            if (value == ix) return item;
            pos --;
        }
    }
    return NULL;
}


void StopmotionWidget::selectFrame(int ix)
{
    frame_list->blockSignals(true);
    QListWidgetItem *item = getFrameFromIndex(ix);
    if (!item) return;
    frame_list->setCurrentItem(item);
    frame_list->blockSignals(false);
}

void StopmotionWidget::slotSeekFrame(bool forward)
{
    int ix = frame_list->currentRow();
    if (forward) {
        if (ix < frame_list->count() - 1) frame_list->setCurrentRow(ix + 1);
    } else if (ix > 0) frame_list->setCurrentRow(ix - 1);
}

void StopmotionWidget::slotRemoveFrame()
{
    if (frame_list->currentItem() == NULL) return;
    QString path = frame_list->currentItem()->toolTip();
    if (KMessageBox::questionYesNo(this, i18n("Delete frame %1 from disk?", path), i18n("Delete Frame")) != KMessageBox::Yes) return;
    QFile f(path);
    if (f.remove()) {
        QListWidgetItem *item = frame_list->takeItem(frame_list->currentRow());
        delete item;
    }
}

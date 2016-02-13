/***************************************************************************
 *   Copyright (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "parametercontainer.h"

#include "complexparameter.h"
#include "geometryval.h"
#include "positionedit.h"
#include "dragvalue.h"
#include "widgets/kis_curve_widget.h"
#include "widgets/kis_cubic_curve.h"
#include "widgets/choosecolorwidget.h"
#include "widgets/geometrywidget.h"
#include "widgets/doubleparameterwidget.h"
#include "widgets/cornerswidget.h"
#include "widgets/bezier/beziersplinewidget.h"
#include "effectstack/widgets/lumaliftgain.h"
#include "effectstack/widgets/animationwidget.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "colortools.h"
#include "dialogs/clipcreationdialog.h"
#include "mltcontroller/effectscontroller.h"
#include "onmonitoritems/rotoscoping/rotowidget.h"

#include "ui_listval_ui.h"
#include "ui_boolval_ui.h"
#include "ui_wipeval_ui.h"
#include "ui_urlval_ui.h"
#include "ui_keywordval_ui.h"
#include "ui_fontval_ui.h"

#include <KUrlRequester>
#include "klocalizedstring.h"

#include <QMap>
#include <QString>
#include <QImage>
#include <QDebug>

MySpinBox::MySpinBox(QWidget * parent):
    QSpinBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void MySpinBox::focusInEvent(QFocusEvent *e)
{
     setFocusPolicy(Qt::WheelFocus);
     e->accept();
}

void MySpinBox::focusOutEvent(QFocusEvent *e)
{
     setFocusPolicy(Qt::StrongFocus);
     e->accept();
}

class Boolval: public QWidget, public Ui::Boolval_UI
{
};

class Listval: public QWidget, public Ui::Listval_UI
{
};

class Wipeval: public QWidget, public Ui::Wipeval_UI
{
};

class Urlval: public QWidget, public Ui::Urlval_UI
{
};

class Keywordval: public QWidget, public Ui::Keywordval_UI
{
};

class Fontval: public QWidget, public Ui::Fontval_UI
{
};


ParameterContainer::ParameterContainer(const QDomElement &effect, const ItemInfo &info, EffectMetaInfo *metaInfo, QWidget * parent) :
        m_keyframeEditor(NULL),
        m_geometryWidget(NULL),
        m_animationWidget(NULL),
        m_metaInfo(metaInfo),
        m_effect(effect),
        m_monitorEffectScene(MonitorSceneDefault)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    m_in = info.cropStart.frames(KdenliveSettings::project_fps());
    m_out = (info.cropStart + info.cropDuration).frames(KdenliveSettings::project_fps()) - 1;

    QDomNodeList namenode = effect.childNodes(); //elementsByTagName("parameter");
    QDomElement e = effect.toElement();

    int minFrame = e.attribute(QStringLiteral("start")).toInt();
    int maxFrame = e.attribute(QStringLiteral("end")).toInt();
    // In transitions, maxFrame is in fact one frame after the end of transition
    if (maxFrame > 0) maxFrame --;

    bool disable = effect.attribute(QStringLiteral("disable")) == QLatin1String("1") && KdenliveSettings::disable_effect_parameters();
    parent->setEnabled(!disable);

    bool stretch = true;
    m_vbox = new QVBoxLayout(parent);
    m_vbox->setContentsMargins(4, 0, 4, 0);
    m_vbox->setSpacing(2);

    if (effect.attribute(QStringLiteral("id")) == QLatin1String("movit.lift_gamma_gain") || effect.attribute(QStringLiteral("id")) == QLatin1String("lift_gamma_gain") ) {
        // We use a special custom widget here
        LumaLiftGain *gainWidget = new LumaLiftGain(namenode, parent);
        m_vbox->addWidget(gainWidget);
        m_valueItems[effect.attribute(QStringLiteral("id"))] = gainWidget;
        connect(gainWidget, SIGNAL(valueChanged()), this, SLOT(slotCollectAllParameters()));
    }
    else for (int i = 0; i < namenode.count() ; ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.tagName() != QLatin1String("parameter")) continue;
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QDomElement commentElem = pa.firstChildElement(QStringLiteral("comment"));
        QString type = pa.attribute(QStringLiteral("type"));
        QString paramName = na.isNull() ? pa.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
        QString comment;
        if (!commentElem.isNull())
            comment = i18n(commentElem.text().toUtf8().data());
        QWidget * toFillin = new QWidget(parent);
        QString value = pa.attribute(QStringLiteral("value")).isNull() ?
                        pa.attribute(QStringLiteral("default")) : pa.attribute(QStringLiteral("value"));


        /** See effects/README for info on the different types */
        //qDebug()<<"-------------------------------------\n"<<effect.ownerDocument().toString()<<"\n-------------------";
        if (type == QLatin1String("double") || type == QLatin1String("constant")) {
            double min;
            double max;
            if (pa.attribute(QStringLiteral("min")).contains('%'))
                min = EffectsController::getStringEval(m_metaInfo->monitor->profileInfo(), pa.attribute(QStringLiteral("min")), m_metaInfo->frameSize);
            else
                min = locale.toDouble(pa.attribute(QStringLiteral("min")));
            if (pa.attribute(QStringLiteral("max")).contains('%'))
                max = EffectsController::getStringEval(m_metaInfo->monitor->profileInfo(), pa.attribute(QStringLiteral("max")), m_metaInfo->frameSize);
            else
                max = locale.toDouble(pa.attribute(QStringLiteral("max")));

            DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, locale.toDouble(value), min, max,
                    locale.toDouble(pa.attribute(QStringLiteral("default"))), comment, -1, pa.attribute(QStringLiteral("suffix")), pa.attribute(QStringLiteral("decimals")).toInt(), false, parent);
	    doubleparam->setFocusPolicy(Qt::StrongFocus);
            m_vbox->addWidget(doubleparam);
            m_valueItems[paramName] = doubleparam;
            connect(doubleparam, SIGNAL(valueChanged(double)), this, SLOT(slotCollectAllParameters()));
            connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
        } else if (type == QLatin1String("list")) {
            Listval *lsval = new Listval;
            lsval->setupUi(toFillin);
	    lsval->list->setFocusPolicy(Qt::StrongFocus);
            QStringList listitems = pa.attribute(QStringLiteral("paramlist")).split(';');
            if (listitems.count() == 1) {
                // probably custom effect created before change to ';' as separator
                listitems = pa.attribute(QStringLiteral("paramlist")).split(',');
            }
            QDomElement list = pa.firstChildElement(QStringLiteral("paramlistdisplay"));
            QStringList listitemsdisplay;
            if (!list.isNull()) {
                listitemsdisplay = i18n(list.text().toUtf8().data()).split(',');
            } else {
                listitemsdisplay = i18n(pa.attribute("paramlistdisplay").toUtf8().data()).split(',');
            }
            if (listitemsdisplay.count() != listitems.count())
                listitemsdisplay = listitems;
            lsval->list->setIconSize(QSize(30, 30));
            for (int i = 0; i < listitems.count(); ++i) {
                lsval->list->addItem(listitemsdisplay.at(i), listitems.at(i));
                QString entry = listitems.at(i);
                if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
                    if (!MainWindow::m_lumacache.contains(entry)) {
                        QImage pix(entry);
                        MainWindow::m_lumacache.insert(entry, pix.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
		    lsval->list->setItemIcon(i, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                }
            }
            if (!value.isEmpty()) lsval->list->setCurrentIndex(listitems.indexOf(value));
            lsval->name->setText(paramName);
	    lsval->setToolTip(comment);
            lsval->labelComment->setText(comment);
            lsval->widgetComment->setHidden(true);
            m_valueItems[paramName] = lsval;
            connect(lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT(slotCollectAllParameters()));
            if (!comment.isEmpty())
                connect(this, SIGNAL(showComments(bool)), lsval->widgetComment, SLOT(setVisible(bool)));
            m_uiItems.append(lsval);
        } else if (type == QLatin1String("bool")) {
            Boolval *bval = new Boolval;
            bval->setupUi(toFillin);
            bval->checkBox->setCheckState(value == QLatin1String("0") ? Qt::Unchecked : Qt::Checked);
            bval->name->setText(paramName);
	    bval->name->setToolTip(comment);
            bval->labelComment->setText(comment);
            bval->widgetComment->setHidden(true);
            m_valueItems[paramName] = bval;
            connect(bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT(slotCollectAllParameters()));
            if (!comment.isEmpty())
                connect(this, SIGNAL(showComments(bool)), bval->widgetComment, SLOT(setVisible(bool)));
            m_uiItems.append(bval);
        } else if (type == QLatin1String("switch")) {
            Boolval *bval = new Boolval;
            bval->setupUi(toFillin);
            bval->checkBox->setCheckState(value == pa.attribute("min") ? Qt::Unchecked : Qt::Checked);
            bval->name->setText(paramName);
            bval->name->setToolTip(comment);
            bval->labelComment->setText(comment);
            bval->widgetComment->setHidden(true);
            m_valueItems[paramName] = bval;
            connect(bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT(slotCollectAllParameters()));
            if (!comment.isEmpty())
                connect(this, SIGNAL(showComments(bool)), bval->widgetComment, SLOT(setVisible(bool)));
            m_uiItems.append(bval);
        } else if (type == QLatin1String("animated")) {
            if (m_animationWidget) {
                m_animationWidget->addParameter(pa);
            } else {
                m_animationWidget = new AnimationWidget(m_metaInfo, info.startPos.frames(KdenliveSettings::project_fps()), m_in, m_out, effect.attribute(QStringLiteral("id")), pa, e.attribute(QStringLiteral("active_keyframe"), QStringLiteral("-1")).toInt(), parent);
                connect(m_animationWidget, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(syncEffectsPos(int)), m_animationWidget, SLOT(slotSyncPosition(int)));
                connect(m_animationWidget, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
                m_valueItems["animated"] = m_animationWidget;
                m_vbox->addWidget(m_animationWidget);
            }
        } else if (type == QLatin1String("complex")) {
            ComplexParameter *pl = new ComplexParameter;
            pl->setupParam(effect, pa.attribute(QStringLiteral("name")), 0, 100);
            m_vbox->addWidget(pl);
            m_valueItems[paramName+"complex"] = pl;
            connect(pl, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
        } else if (type == QLatin1String("geometry")) {
            if (true /*KdenliveSettings::on_monitor_effects()*/) {
                m_monitorEffectScene = MonitorSceneGeometry;
                m_geometryWidget = new GeometryWidget(m_metaInfo, info.startPos.frames(KdenliveSettings::project_fps()), effect.hasAttribute(QStringLiteral("showrotation")), parent);
                connect(m_geometryWidget, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
                if (minFrame == maxFrame) {
                    m_geometryWidget->setupParam(pa, m_in, m_out);
                    connect(this, SIGNAL(updateRange(int,int)), m_geometryWidget, SLOT(slotUpdateRange(int,int)));
                }
                else
                    m_geometryWidget->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(m_geometryWidget);
                m_valueItems[paramName+"geometry"] = m_geometryWidget;
                connect(m_geometryWidget, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(m_geometryWidget, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
                connect(this, SIGNAL(syncEffectsPos(int)), m_geometryWidget, SLOT(slotSyncPosition(int)));
                connect(this, SIGNAL(initScene(int)), m_geometryWidget, SLOT(slotInitScene(int)));
            } else {
                Geometryval *geo = new Geometryval(m_metaInfo->monitor->profile(), m_metaInfo->monitor->timecode(), m_metaInfo->frameSize, 0);
                if (minFrame == maxFrame) {
                    geo->setupParam(pa, m_in, m_out);
		    connect(this, SIGNAL(updateRange(int,int)), geo, SLOT(slotUpdateRange(int,int)));
		}
                else
                    geo->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"geometry"] = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(syncEffectsPos(int)), geo, SLOT(slotSyncPosition(int)));
            }
        } else if (type == QLatin1String("addedgeometry")) {
            // this is a parameter that should be linked to the geometry widget, for example rotation, shear, ...
            if (m_geometryWidget) m_geometryWidget->addParameter(pa);
        } else if (type == QLatin1String("keyframe") || type == QLatin1String("simplekeyframe")) {
            // keyframe editor widget
            if (m_keyframeEditor == NULL) {
                KeyframeEdit *geo;
                if (pa.attribute(QStringLiteral("widget")) == QLatin1String("corners")) {
                    // we want a corners-keyframe-widget
                    int relativePos = (m_metaInfo->monitor->position() - info.startPos).frames(KdenliveSettings::project_fps());
                    CornersWidget *corners = new CornersWidget(m_metaInfo->monitor, pa, m_in, m_out, relativePos, m_metaInfo->monitor->timecode(), e.attribute(QStringLiteral("active_keyframe"), QStringLiteral("-1")).toInt(), parent);
		    connect(this, SIGNAL(updateRange(int,int)), corners, SLOT(slotUpdateRange(int,int)));
		    m_monitorEffectScene = MonitorSceneCorners;
                    connect(this, &ParameterContainer::updateFrameInfo, corners, &CornersWidget::setFrameSize);
                    connect(this, SIGNAL(syncEffectsPos(int)), corners, SLOT(slotSyncPosition(int)));
                    geo = static_cast<KeyframeEdit *>(corners);
                } else {
                    geo = new KeyframeEdit(pa, m_in, m_out, m_metaInfo->monitor->timecode(), e.attribute(QStringLiteral("active_keyframe"), QStringLiteral("-1")).toInt());
		    connect(this, SIGNAL(updateRange(int,int)), geo, SLOT(slotUpdateRange(int,int)));
                }
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"keyframe"] = geo;
                m_keyframeEditor = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(showComments(bool)), geo, SIGNAL(showComments(bool)));
            } else {
                // we already have a keyframe editor, so just add another column for the new param
                m_keyframeEditor->addParameter(pa);
            }
        } else if (type == QLatin1String("color")) {
	    if (pa.hasAttribute(QStringLiteral("paramprefix"))) value.remove(0, pa.attribute(QStringLiteral("paramprefix")).size());
            if (value.startsWith('#'))
                value = value.replace('#', QLatin1String("0x"));
            ChooseColorWidget *choosecolor = new ChooseColorWidget(paramName, value, pa.hasAttribute(QStringLiteral("alpha")), parent);
	    choosecolor->setToolTip(comment);
            m_vbox->addWidget(choosecolor);
            m_valueItems[paramName] = choosecolor;
            connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
            connect(choosecolor, SIGNAL(modified()) , this, SLOT(slotCollectAllParameters()));
	    connect(choosecolor, SIGNAL(disableCurrentFilter(bool)) , this, SIGNAL(disableCurrentFilter(bool)));
        } else if (type == QLatin1String("position")) {
            int pos = value.toInt();
            if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadein") || effect.attribute(QStringLiteral("id")) == QLatin1String("fade_from_black")) {
                pos = pos - m_in;
            } else if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadeout") || effect.attribute(QStringLiteral("id")) == QLatin1String("fade_to_black")) {
                // fadeout position starts from clip end
                pos = m_out - pos;
            }
            PositionEdit *posedit = new PositionEdit(paramName, pos, 0, m_out - m_in, m_metaInfo->monitor->timecode());
	    posedit->setToolTip(comment);
	    connect(this, SIGNAL(updateRange(int,int)), posedit, SLOT(setRange(int,int)));
            m_vbox->addWidget(posedit);
            m_valueItems[paramName+"position"] = posedit;
            connect(posedit, SIGNAL(parameterChanged()), this, SLOT(slotCollectAllParameters()));
        } else if (type == QLatin1String("curve")) {
            KisCurveWidget *curve = new KisCurveWidget(parent);
            curve->setMaxPoints(pa.attribute(QStringLiteral("max")).toInt());
            QList<QPointF> points;
            int number;
            double version = 0;
            QDomElement namenode = effect.firstChildElement(QStringLiteral("version"));
            if (!namenode.isNull()) {
                version = locale.toDouble(namenode.text());
            }
	    if (version > 0.2) {
		// Rounding gives really weird results. (int) (10 * 0.3) gives 2! So for now, add 0.5 to get correct result
                number = locale.toDouble(EffectsList::parameter(e, pa.attribute(QStringLiteral("number")))) * 10 + 0.5;
            } else {
                number = EffectsList::parameter(e, pa.attribute(QStringLiteral("number"))).toInt();
            }
            QString inName = pa.attribute(QStringLiteral("inpoints"));
            QString outName = pa.attribute(QStringLiteral("outpoints"));
            int start = pa.attribute(QStringLiteral("min")).toInt();
            for (int j = start; j <= number; ++j) {
                QString in = inName;
                in.replace(QLatin1String("%i"), QString::number(j));
                QString out = outName;
                out.replace(QLatin1String("%i"), QString::number(j));
                points << QPointF(locale.toDouble(EffectsList::parameter(e, in)), locale.toDouble(EffectsList::parameter(e, out)));
            }
            if (!points.isEmpty()) {
                curve->setCurve(KisCubicCurve(points));
            }
            MySpinBox *spinin = new MySpinBox();
            spinin->setRange(0, 1000);
            MySpinBox *spinout = new MySpinBox();
            spinout->setRange(0, 1000);
            curve->setupInOutControls(spinin, spinout, 0, 1000);
            m_vbox->addWidget(curve);
            m_vbox->addWidget(spinin);
            m_vbox->addWidget(spinout);

            connect(curve, SIGNAL(modified()), this, SLOT(slotCollectAllParameters()));
            m_valueItems[paramName] = curve;

            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(e, depends));
        } else if (type == QLatin1String("bezier_spline")) {
            BezierSplineWidget *widget = new BezierSplineWidget(value, parent);
            stretch = false;
            m_vbox->addWidget(widget);
            m_valueItems[paramName] = widget;
            connect(widget, SIGNAL(modified()), this, SLOT(slotCollectAllParameters()));
            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(e, depends));
        } else if (type == QLatin1String("roto-spline")) {
            m_monitorEffectScene = MonitorSceneRoto;
            RotoWidget *roto = new RotoWidget(value.toLatin1(), m_metaInfo->monitor, info, m_metaInfo->monitor->timecode(), parent);
            connect(roto, SIGNAL(valueChanged()), this, SLOT(slotCollectAllParameters()));
            connect(roto, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
            connect(this, SIGNAL(syncEffectsPos(int)), roto, SLOT(slotSyncPosition(int)));
            m_vbox->addWidget(roto);
            m_valueItems[paramName] = roto;
        } else if (type == QLatin1String("wipe")) {
            Wipeval *wpval = new Wipeval;
            wpval->setupUi(toFillin);
            // Make sure button shows its pressed state even if widget loses focus
            QColor bg = QPalette().highlight().color();
            toFillin->setStyleSheet(QStringLiteral("QPushButton:checked {background-color:rgb(%1,%2,%3);}").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
            wipeInfo w = getWipeInfo(value);
            switch (w.start) {
            case UP:
                wpval->start_up->setChecked(true);
                break;
            case DOWN:
                wpval->start_down->setChecked(true);
                break;
            case RIGHT:
                wpval->start_right->setChecked(true);
                break;
            case LEFT:
                wpval->start_left->setChecked(true);
                break;
            default:
                wpval->start_center->setChecked(true);
                break;
            }
            switch (w.end) {
            case UP:
                wpval->end_up->setChecked(true);
                break;
            case DOWN:
                wpval->end_down->setChecked(true);
                break;
            case RIGHT:
                wpval->end_right->setChecked(true);
                break;
            case LEFT:
                wpval->end_left->setChecked(true);
                break;
            default:
                wpval->end_center->setChecked(true);
                break;
            }
            wpval->start_transp->setValue(w.startTransparency);
            wpval->end_transp->setValue(w.endTransparency);
            m_valueItems[paramName] = wpval;
            connect(wpval->end_up, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->end_down, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->end_left, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->end_right, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->end_center, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_up, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_down, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_left, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_right, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_center, SIGNAL(clicked()), this, SLOT(slotCollectAllParameters()));
            connect(wpval->start_transp, SIGNAL(valueChanged(int)), this, SLOT(slotCollectAllParameters()));
            connect(wpval->end_transp, SIGNAL(valueChanged(int)), this, SLOT(slotCollectAllParameters()));
            //wpval->title->setTitle(na.toElement().text());
            m_uiItems.append(wpval);
        } else if (type == QLatin1String("url")) {
            Urlval *cval = new Urlval;
            cval->setupUi(toFillin);
            cval->label->setText(paramName);
	    cval->setToolTip(comment);
            cval->urlwidget->setFilter(ClipCreationDialog::getExtensions().join(' '));
            m_valueItems[paramName] = cval;
            cval->urlwidget->setUrl(QUrl(value));
            connect(cval->urlwidget, SIGNAL(returnPressed()) , this, SLOT(slotCollectAllParameters()));
            connect(cval->urlwidget, SIGNAL(urlSelected(QUrl)) , this, SLOT(slotCollectAllParameters()));
            m_uiItems.append(cval);
	} else if (type == QLatin1String("keywords")) {
            Keywordval* kval = new Keywordval;
            kval->setupUi(toFillin);
            kval->label->setText(paramName);
            kval->lineeditwidget->setText(value);
	    kval->setToolTip(comment);
            QDomElement klistelem = pa.firstChildElement(QStringLiteral("keywords"));
            QDomElement kdisplaylistelem = pa.firstChildElement(QStringLiteral("keywordsdisplay"));
            QStringList keywordlist;
            QStringList keyworddisplaylist;
            if (!klistelem.isNull()) {
                keywordlist = klistelem.text().split(';');
                keyworddisplaylist = i18n(kdisplaylistelem.text().toUtf8().data()).split(';');
            }
            if (keyworddisplaylist.count() != keywordlist.count()) {
                keyworddisplaylist = keywordlist;
            }
            for (int i = 0; i < keywordlist.count(); ++i) {
                kval->comboboxwidget->addItem(keyworddisplaylist.at(i), keywordlist.at(i));
            }
            // Add disabled user prompt at index 0
            kval->comboboxwidget->insertItem(0, i18n("<select a keyword>"), "");
            kval->comboboxwidget->model()->setData( kval->comboboxwidget->model()->index(0,0), QVariant(Qt::NoItemFlags), Qt::UserRole -1);
            kval->comboboxwidget->setCurrentIndex(0);
            m_valueItems[paramName] = kval;
            connect(kval->lineeditwidget, SIGNAL(editingFinished()) , this, SLOT(slotCollectAllParameters()));
            connect(kval->comboboxwidget, SIGNAL(activated(QString)), this, SLOT(slotCollectAllParameters()));
            m_uiItems.append(kval);
        } else if (type == QLatin1String("fontfamily")) {
            Fontval* fval = new Fontval;
            fval->setupUi(toFillin);
            fval->name->setText(paramName);
            fval->fontfamilywidget->setCurrentFont(QFont(value));
            m_valueItems[paramName] = fval;
            connect(fval->fontfamilywidget, SIGNAL(currentFontChanged(QFont)), this, SLOT(slotCollectAllParameters())) ;
            m_uiItems.append(fval);
        } else if (type == QLatin1String("filterjob")) {
	    QVBoxLayout *l= new QVBoxLayout(toFillin);
	    QPushButton *button = new QPushButton(paramName, toFillin);
	    l->addWidget(button);
            m_valueItems[paramName] = button;
            connect(button, SIGNAL(pressed()), this, SLOT(slotStartFilterJobAction()));   
        } else {
            delete toFillin;
            toFillin = NULL;
        }

        if (toFillin)
            m_vbox->addWidget(toFillin);
    }

    if (stretch)
        m_vbox->addStretch();

    if (m_keyframeEditor)
        m_keyframeEditor->checkVisibleParam();
    
    if (m_animationWidget) {
        m_animationWidget->finishSetup();
    }

    // Make sure all doubleparam spinboxes have the same width, looks much better
    QList<DoubleParameterWidget *> allWidgets = findChildren<DoubleParameterWidget *>();
    int minSize = 0;
    for (int i = 0; i < allWidgets.count(); ++i) {
        if (minSize < allWidgets.at(i)->spinSize()) minSize = allWidgets.at(i)->spinSize();
    }
    for (int i = 0; i < allWidgets.count(); ++i) {
        allWidgets.at(i)->setSpinSize(minSize);
    }
}

ParameterContainer::~ParameterContainer()
{
    clearLayout(m_vbox);
    delete m_vbox;
}

void ParameterContainer::meetDependency(const QString& name, const QString &type, const QString &value)
{
    if (type == QLatin1String("curve")) {
        KisCurveWidget *curve = static_cast<KisCurveWidget*>(m_valueItems[name]);
        if (curve) {
            const int color = value.toInt();
            curve->setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(curve->size(), (ColorTools::ColorsRGB)(color == 3 ? 4 : color), 0.8)));
        }
    } else if (type == QLatin1String("bezier_spline")) {
        BezierSplineWidget *widget = static_cast<BezierSplineWidget*>(m_valueItems[name]);
        if (widget) {
            QLocale locale;
            widget->setMode((BezierSplineWidget::CurveModes)((int)(locale.toDouble(value) * 10 + 0.5)));
        }
    }
}

wipeInfo ParameterContainer::getWipeInfo(QString value)
{
    wipeInfo info;
    // Convert old geometry values that used a comma as separator
    if (value.contains(',')) value.replace(',','/');
    QString start = value.section(';', 0, 0);
    QString end = value.section(';', 1, 1).section('=', 1, 1);
    if (start.startsWith(QLatin1String("-100%/0")))
        info.start = LEFT;
    else if (start.startsWith(QLatin1String("100%/0")))
        info.start = RIGHT;
    else if (start.startsWith(QLatin1String("0%/100%")))
        info.start = DOWN;
    else if (start.startsWith(QLatin1String("0%/-100%")))
        info.start = UP;
    else
        info.start = CENTER;

    if (start.count(':') == 2)
        info.startTransparency = start.section(':', -1).toInt();
    else
        info.startTransparency = 100;

    if (end.startsWith(QLatin1String("-100%/0")))
        info.end = LEFT;
    else if (end.startsWith(QLatin1String("100%/0")))
        info.end = RIGHT;
    else if (end.startsWith(QLatin1String("0%/100%")))
        info.end = DOWN;
    else if (end.startsWith(QLatin1String("0%/-100%")))
        info.end = UP;
    else
        info.end = CENTER;

    if (end.count(':') == 2)
        info.endTransparency = end.section(':', -1).toInt();
    else
        info.endTransparency = 100;

    return info;
}

void ParameterContainer::updateTimecodeFormat()
{
    if (m_keyframeEditor)
        m_keyframeEditor->updateTimecodeFormat();

    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count() ; ++i) {
        QDomNode pa = namenode.item(i);
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QString type = pa.attributes().namedItem(QStringLiteral("type")).nodeValue();
        QString paramName = na.isNull() ? pa.attributes().namedItem(QStringLiteral("name")).nodeValue() : i18n(na.text().toUtf8().data());

        if (type == QLatin1String("geometry")) {
            if (KdenliveSettings::on_monitor_effects()) {
                if (m_geometryWidget) m_geometryWidget->updateTimecodeFormat();
            } else {
                Geometryval *geom = static_cast<Geometryval*>(m_valueItems[paramName+"geometry"]);
                geom->updateTimecodeFormat();
            }
            break;
        } else if (type == QLatin1String("position")) {
            PositionEdit *posi = static_cast<PositionEdit*>(m_valueItems[paramName+"position"]);
            posi->updateTimecodeFormat();
            break;
        } else if (type == QLatin1String("roto-spline")) {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems[paramName]);
            widget->updateTimecodeFormat();
        } else if (type == QLatin1String("animated")) {
            if (m_animationWidget) m_animationWidget->updateTimecodeFormat();
        }
    }
}

void ParameterContainer::slotCollectAllParameters()
{
    if (m_valueItems.isEmpty() || m_effect.isNull()) return;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    const QDomElement oldparam = m_effect.cloneNode().toElement();
    //QDomElement newparam = oldparam.cloneNode().toElement();

    if (m_effect.attribute(QStringLiteral("id")) == QLatin1String("movit.lift_gamma_gain") || m_effect.attribute(QStringLiteral("id")) == QLatin1String("lift_gamma_gain") ) {
        LumaLiftGain *gainWidget = static_cast<LumaLiftGain*>(m_valueItems.value(m_effect.attribute(QStringLiteral("id"))));
        gainWidget->updateEffect(m_effect);
        emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
        return;
    }

    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));

    // special case, m_animationWidget can hold several parameters
    if (m_animationWidget) {
        QMap <QString, QString> values = m_animationWidget->getAnimation();
        for (int i = 0; i < namenode.count() ; ++i) {
            QDomElement pa = namenode.item(i).toElement();
            QString paramName = pa.attribute(QStringLiteral("name"));
            if (values.count() > 1) {
                pa.setAttribute(QStringLiteral("intimeline"), m_animationWidget->isActive(paramName) ? "1" : "0");
            }
            if (values.contains(paramName)) {
                pa.setAttribute(QStringLiteral("value"), values.value(paramName));
            }
        }
    }

    for (int i = 0; i < namenode.count() ; ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QString type = pa.attribute(QStringLiteral("type"));
        QString paramName = na.isNull() ? pa.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
        if (type == QLatin1String("complex"))
            paramName.append("complex");
        else if (type == QLatin1String("position"))
            paramName.append("position");
        else if (type == QLatin1String("geometry"))
            paramName.append("geometry");
        else if (type == QLatin1String("keyframe"))
            paramName.append("keyframe");
        else if (type == QLatin1String("animated"))
            continue;
        if (type != QLatin1String("simplekeyframe") && type != QLatin1String("fixed") && type != QLatin1String("addedgeometry") && !m_valueItems.contains(paramName)) {
            qDebug() << "// Param: " << paramName << " NOT FOUND";
            continue;
        }

        QString setValue;
        if (type == QLatin1String("double") || type == QLatin1String("constant")) {
            DoubleParameterWidget *doubleparam = static_cast<DoubleParameterWidget*>(m_valueItems.value(paramName));
            if (doubleparam) setValue = locale.toString(doubleparam->getValue());
        } else if (type == QLatin1String("list")) {
	    Listval* val = static_cast<Listval*>(m_valueItems.value(paramName));
            if (val) {
	        KComboBox *box = val->list;
		setValue = box->itemData(box->currentIndex()).toString();
                // special case, list value is allowed to be empty
                pa.setAttribute(QStringLiteral("value"), setValue);
                setValue.clear();
	    }
        } else if (type == QLatin1String("bool")) {
	    Boolval* val = static_cast<Boolval*>(m_valueItems.value(paramName));
            if (val) {
		QCheckBox *box = val->checkBox;
		setValue = box->checkState() == Qt::Checked ? "1" : "0" ;
	    }
	} else if (type == QLatin1String("switch")) {
            Boolval* val = static_cast<Boolval*>(m_valueItems.value(paramName));
            if (val) {
                QCheckBox *box = val->checkBox;
                setValue = box->checkState() == Qt::Checked ? pa.attribute("max") : pa.attribute("min") ;
            }
        } else if (type == QLatin1String("color")) {
            ChooseColorWidget *choosecolor = static_cast<ChooseColorWidget*>(m_valueItems.value(paramName));
            if (choosecolor) setValue = choosecolor->getColor();
	    if (pa.hasAttribute(QStringLiteral("paramprefix"))) setValue.prepend(pa.attribute(QStringLiteral("paramprefix")));
        } else if (type == QLatin1String("complex")) {
            ComplexParameter *complex = static_cast<ComplexParameter*>(m_valueItems.value(paramName));
            if (complex) namenode.item(i) = complex->getParamDesc();
        } else if (type == QLatin1String("geometry")) {
            /*if (KdenliveSettings::on_monitor_effects())*/ {
                if (m_geometryWidget) namenode.item(i).toElement().setAttribute(QStringLiteral("value"), m_geometryWidget->getValue());
            }/* else {
                Geometryval *geom = static_cast<Geometryval*>(m_valueItems.value(paramName));
                namenode.item(i).toElement().setAttribute("value", geom->getValue());
            }*/
        } else if (type == QLatin1String("addedgeometry")) {
            if (m_geometryWidget) namenode.item(i).toElement().setAttribute(QStringLiteral("value"), m_geometryWidget->getExtraValue(namenode.item(i).toElement().attribute(QStringLiteral("name"))));
        } else if (type == QLatin1String("position")) {
            PositionEdit *pedit = static_cast<PositionEdit*>(m_valueItems.value(paramName));
            int pos = 0; if (pedit) pos = pedit->getPosition();
            setValue = QString::number(pos);
            if (m_effect.attribute(QStringLiteral("id")) == QLatin1String("fadein") || m_effect.attribute(QStringLiteral("id")) == QLatin1String("fade_from_black")) {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                EffectsList::setParameter(m_effect, QStringLiteral("in"), QString::number(m_in));
                EffectsList::setParameter(m_effect, QStringLiteral("out"), QString::number(m_in + pos));
                setValue.clear();
            } else if (m_effect.attribute(QStringLiteral("id")) == QLatin1String("fadeout") || m_effect.attribute(QStringLiteral("id")) == QLatin1String("fade_to_black")) {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                EffectsList::setParameter(m_effect, QStringLiteral("in"), QString::number(m_out - pos));
                EffectsList::setParameter(m_effect, QStringLiteral("out"), QString::number(m_out));
                setValue.clear();
            }
        } else if (type == QLatin1String("curve")) {
            KisCurveWidget *curve = static_cast<KisCurveWidget*>(m_valueItems.value(paramName));
            if (curve) {
                QList<QPointF> points = curve->curve().points();
                QString number = pa.attribute(QStringLiteral("number"));
                QString inName = pa.attribute(QStringLiteral("inpoints"));
                QString outName = pa.attribute(QStringLiteral("outpoints"));
                int off = pa.attribute(QStringLiteral("min")).toInt();
                int end = pa.attribute(QStringLiteral("max")).toInt();
                double version = 0;
                QDomElement namenode = m_effect.firstChildElement(QStringLiteral("version"));
                if (!namenode.isNull()) {
                    version = locale.toDouble(namenode.text());
                }
                if (version > 0.2) {
                    EffectsList::setParameter(m_effect, number, locale.toString(points.count() / 10.));
                } else {
                    EffectsList::setParameter(m_effect, number, QString::number(points.count()));
                }
                for (int j = 0; (j < points.count() && j + off <= end); ++j) {
                    QString in = inName;
                    in.replace(QLatin1String("%i"), QString::number(j + off));
                    QString out = outName;
                    out.replace(QLatin1String("%i"), QString::number(j + off));
                    EffectsList::setParameter(m_effect, in, locale.toString(points.at(j).x()));
                    EffectsList::setParameter(m_effect, out, locale.toString(points.at(j).y()));
                }
            }
            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(m_effect, depends));
        } else if (type == QLatin1String("bezier_spline")) {
            BezierSplineWidget *widget = static_cast<BezierSplineWidget*>(m_valueItems.value(paramName));
            if (widget) setValue = widget->spline();
            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(m_effect, depends));
        } else if (type == QLatin1String("roto-spline")) {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems.value(paramName));
            if (widget) setValue = widget->getSpline();
        } else if (type == QLatin1String("wipe")) {
            Wipeval *wp = static_cast<Wipeval*>(m_valueItems.value(paramName));
            if (wp) {
                wipeInfo info;
                if (wp->start_left->isChecked())
                    info.start = LEFT;
                else if (wp->start_right->isChecked())
                    info.start = RIGHT;
                else if (wp->start_up->isChecked())
                    info.start = UP;
                else if (wp->start_down->isChecked())
                    info.start = DOWN;
                else if (wp->start_center->isChecked())
                    info.start = CENTER;
                else
                    info.start = LEFT;
                info.startTransparency = wp->start_transp->value();

                if (wp->end_left->isChecked())
                    info.end = LEFT;
                else if (wp->end_right->isChecked())
                    info.end = RIGHT;
                else if (wp->end_up->isChecked())
                    info.end = UP;
                else if (wp->end_down->isChecked())
                    info.end = DOWN;
                else if (wp->end_center->isChecked())
                    info.end = CENTER;
                else
                    info.end = RIGHT;
                info.endTransparency = wp->end_transp->value();

                setValue = getWipeString(info);
            }
        } else if ((type == QLatin1String("simplekeyframe") || type == QLatin1String("keyframe")) && m_keyframeEditor) {
            QString realName = i18n(na.toElement().text().toUtf8().data());
            QString val = m_keyframeEditor->getValue(realName);
            pa.setAttribute(m_keyframeEditor->getTag(), val);

            if (m_keyframeEditor->isVisibleParam(realName)) {
                pa.setAttribute(QStringLiteral("intimeline"), QStringLiteral("1"));
	    }
            else if (pa.hasAttribute(QStringLiteral("intimeline")))
                pa.setAttribute(QStringLiteral("intimeline"), QStringLiteral("0"));
        } else if (type == QLatin1String("url")) {
            KUrlRequester *req = static_cast<Urlval*>(m_valueItems.value(paramName))->urlwidget;
            if (req) setValue = req->url().path();
        } else if (type == QLatin1String("keywords")) {
            Keywordval* val = static_cast<Keywordval*>(m_valueItems.value(paramName));
            if (val) {
                QLineEdit *line = val->lineeditwidget;
                KComboBox *combo = val->comboboxwidget;
                if(combo->currentIndex()) {
                    QString comboval = combo->itemData(combo->currentIndex()).toString();
                    line->insert(comboval);
                    combo->setCurrentIndex(0);
                }
                setValue = line->text();
            }
        } else if (type == QLatin1String("fontfamily")) {
            Fontval* val = static_cast<Fontval*>(m_valueItems.value(paramName));
            if (val) {
                QFontComboBox* fontfamily = val->fontfamilywidget;
                setValue = fontfamily->currentFont().family();
            }
        }
        if (!setValue.isNull()) {
            pa.setAttribute(QStringLiteral("value"), setValue);
        }
    }
    emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
}

QString ParameterContainer::getWipeString(wipeInfo info)
{

    QString start;
    QString end;
    switch (info.start) {
    case LEFT:
        start = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        start = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        start = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        start = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        start = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    start.append(':' + QString::number(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        end = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        end = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        end = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        end = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    end.append(':' + QString::number(info.endTransparency));
    return QString(start + ";-1=" + end);
}

void ParameterContainer::updateParameter(const QString &key, const QString &value)
{
    m_effect.setAttribute(key, value);
}

void ParameterContainer::slotStartFilterJobAction()
{
    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count() ; ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QString type = pa.attribute(QStringLiteral("type"));
        if (type == QLatin1String("filterjob")) {
	    QMap <QString, QString> filterParams;
	    QMap <QString, QString> consumerParams;
	    filterParams.insert(QStringLiteral("filter"), pa.attribute(QStringLiteral("filtertag")));
	    consumerParams.insert(QStringLiteral("consumer"), pa.attribute(QStringLiteral("consumer")));
	    QString filterattributes = pa.attribute(QStringLiteral("filterparams"));
	    if (filterattributes.contains(QStringLiteral("%position"))) {
		if (m_geometryWidget) filterattributes.replace(QLatin1String("%position"), QString::number(m_geometryWidget->currentPosition()));
	    }

	    // Fill filter params
	    QStringList filterList = filterattributes.split(' ');
	    QString param;
	    for (int i = 0; i < filterList.size(); ++i) {
		param = filterList.at(i);
		if (param != QLatin1String("%params")) {
		    filterParams.insert(param.section('=', 0, 0), param.section('=', 1));
		}
	    }
	    if (filterattributes.contains(QStringLiteral("%params"))) {
		// Replace with current geometry
		EffectsParameterList parameters;
		QDomNodeList params = m_effect.elementsByTagName(QStringLiteral("parameter"));
		EffectsController::adjustEffectParameters(parameters, params, m_metaInfo->monitor->profileInfo());
		QString paramData;
		for (int j = 0; j < parameters.count(); ++j) {
		    filterParams.insert(parameters.at(j).name(), parameters.at(j).value());
		}
	    }
	    // Fill consumer params
	    QString consumerattributes = pa.attribute(QStringLiteral("consumerparams"));
	    QStringList consumerList = consumerattributes.split(' ');
	    for (int i = 0; i < consumerList.size(); ++i) {
		param = consumerList.at(i);
		if (param != QLatin1String("%params")) {
		    consumerParams.insert(param.section('=', 0, 0), param.section('=', 1));
		}
	    }

	    // Fill extra params
	    QMap <QString, QString> extraParams;
	    QDomNodeList jobparams = pa.elementsByTagName(QStringLiteral("jobparam"));
	    for (int j = 0; j < jobparams.count(); ++j) {
                QDomElement e = jobparams.item(j).toElement();
		extraParams.insert(e.attribute(QStringLiteral("name")), e.text().toUtf8());
	    }
	    extraParams.insert(QStringLiteral("offset"), QString::number(m_in));
            emit startFilterJob(filterParams, consumerParams, extraParams);
            break;
        }
    }
}


void ParameterContainer::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

MonitorSceneType ParameterContainer::needsMonitorEffectScene() const
{
    return m_monitorEffectScene;
}

void ParameterContainer::setKeyframes(const QString &data, int maximum)
{
    if (!m_geometryWidget) {
	//qDebug()<<" / / NO GEOMETRY WIDGET FOUND FOR IMPORTING DATA";
	return;
    }
    m_geometryWidget->importKeyframes(data, maximum);
}

void ParameterContainer::setRange(int inPoint, int outPoint)
{
    m_in = inPoint;
    m_out = outPoint;
    emit updateRange(m_in, m_out);
}

int ParameterContainer::contentHeight() const
{
    return m_vbox->sizeHint().height();
}

void ParameterContainer::refreshFrameInfo()
{
    emit updateFrameInfo(m_metaInfo->frameSize, m_metaInfo->stretchFactor);
}

void ParameterContainer::setActiveKeyframe(int frame)
{
    if (m_animationWidget) m_animationWidget->setActiveKeyframe(frame);
}


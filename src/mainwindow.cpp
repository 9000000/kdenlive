/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "mainwindow.h"
#include "mainwindowadaptor.h"
#include "core.h"
#include "bin/projectclip.h"
#include "bin/generators/generators.h"
#include "library/librarywidget.h"
#include "monitor/scopes/audiographspectrum.h"
#include "mltcontroller/clipcontroller.h"
#include "kdenlivesettings.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "dialogs/clipcreationdialog.h"
#include "effectslist/initeffects.h"
#include "project/dialogs/projectsettings.h"
#include "project/clipmanager.h"
#include "monitor/monitor.h"
#include "monitor/recmonitor.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline/timeline.h"
#include "timeline/track.h"
#include "timeline/customtrackview.h"
#include "effectslist/effectslistview.h"
#include "effectslist/effectbasket.h"
#include "effectstack/effectstackview2.h"
#include "project/transitionsettings.h"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/producerqueue.h"
#include "dialogs/renderwidget.h"
#include "renderer.h"
#include "dialogs/wizard.h"
#include "project/projectcommands.h"
#include "titler/titlewidget.h"
#include "timeline/markerdialog.h"
#include "timeline/clipitem.h"
#include "interfaces.h"
#include "project/cliptranscode.h"
#include "scopes/scopemanager.h"
#include "project/dialogs/archivewidget.h"
#include "utils/resourcewidget.h"
#include "layoutmanagement.h"
#include "hidetitlebars.h"
#include "mltconnection.h"
#include "project/projectmanager.h"
#include "timeline/timelinesearch.h"
#include <config-kdenlive.h>
#include "utils/thememanager.h"
#include "utils/KoIconUtils.h"
#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogmanager.h"
#endif

#include <KActionCollection>
#include <KActionCategory>
#include <KActionMenu>
#include <KStandardAction>
#include <KShortcutsDialog>
#include <KMessageBox>
#include <KConfigDialog>
#include <KXMLGUIFactory>
#include <KIconTheme>
#include <KColorSchemeManager>
#include <KRecentDirs>
#include <KUrlRequesterDialog>
#include <ktogglefullscreenaction.h>
#include <KNotifyConfigWidget>
#include <kns3/downloaddialog.h>
#include <kns3/knewstuffaction.h>
#include <KToolBar>
#include <KColorScheme>
#include <klocalizedstring.h>

#include <QAction>
#include <QDebug>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QMenu>
#include <QDesktopWidget>
#include <QBitmap>
#include <QUndoGroup>
#include <QFileDialog>
#include <QStyleFactory>

#include <stdlib.h>
#include <QStandardPaths>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

static const char version[] = KDENLIVE_VERSION;
namespace Mlt
{
class Producer;
};

EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

QMap <QString,QImage> MainWindow::m_lumacache;
QMap <QString,QStringList> MainWindow::m_lumaFiles;

/*static bool sortByNames(const QPair<QString, QAction *> &a, const QPair<QString, QAction*> &b)
{
    return a.first < b.first;
}*/

// determine the the default KDE style as defined BY THE USER
// (as opposed to whatever style KDE considers default)
static QString defaultStyle(const char *fallback=Q_NULLPTR)
{
    KSharedConfigPtr kdeGlobals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::NoGlobals);
    KConfigGroup cg(kdeGlobals, "KDE");
    return cg.readEntry("widgetStyle", fallback);
}

MainWindow::MainWindow(const QString &MltPath, const QUrl &Url, const QString & clipsToLoad, QWidget *parent) :
    KXmlGuiWindow(parent),
    m_timelineArea(NULL),
    m_stopmotion(NULL),
    m_effectStack(NULL),
    m_exitCode(EXIT_SUCCESS),
    m_effectList(NULL),
    m_transitionList(NULL),
    m_clipMonitor(NULL),
    m_projectMonitor(NULL),
    m_recMonitor(NULL),
    m_renderWidget(NULL),
    m_themeInitialized(false),
    m_isDarkTheme(false)
{
    qRegisterMetaType<audioShortVector> ("audioShortVector");
    qRegisterMetaType< QVector<double> > ("QVector<double>");
    qRegisterMetaType<MessageType> ("MessageType");
    qRegisterMetaType<stringMap> ("stringMap");
    qRegisterMetaType<audioByteArray> ("audioByteArray");
    qRegisterMetaType< QVector <int> > ();
    qRegisterMetaType<QDomElement> ("QDomElement");
    qRegisterMetaType<requestClipInfo> ("requestClipInfo");
    qRegisterMetaType<MltVideoProfile> ("MltVideoProfile");

    Core::build(this);

    // Widget themes for non KDE users
    KActionMenu *stylesAction= new KActionMenu(i18n("Style"), this);
    QActionGroup *stylesGroup = new QActionGroup(stylesAction);


    // GTK theme does not work well with Kdenlive, and does not support color theming, so avoid it
    QStringList availableStyles = QStyleFactory::keys();
    QString desktopStyle = QApplication::style()->objectName();
    if (QString::compare(desktopStyle, QLatin1String("GTK+"), Qt::CaseInsensitive) == 0 && KdenliveSettings::widgetstyle().isEmpty()) {
        if (availableStyles.contains(QLatin1String("breeze"), Qt::CaseInsensitive)) {
            // Auto switch to Breeze theme
            KdenliveSettings::setWidgetstyle(QStringLiteral("Breeze"));
        } else if (availableStyles.contains(QLatin1String("fusion"), Qt::CaseInsensitive)) {
            KdenliveSettings::setWidgetstyle(QStringLiteral("Fusion"));
        }
    }

    // Add default style action
    QAction *defaultStyle = new QAction(i18n("Default"), stylesGroup);
    defaultStyle->setCheckable(true);
    stylesAction->addAction(defaultStyle);
    if (KdenliveSettings::widgetstyle().isEmpty()) {
        defaultStyle->setChecked(true);
    }

    foreach(const QString &style, availableStyles) {
        QAction *a = new QAction(style, stylesGroup);
        a->setCheckable(true);
        a->setData(style);
        if (KdenliveSettings::widgetstyle() == style) {
            a->setChecked(true);
        }
        stylesAction->addAction(a);
    }
    connect(stylesGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeStyle);

    // Color schemes
    KActionMenu *themeAction= new KActionMenu(i18n("Theme"), this);
    ThemeManager::instance()->setThemeMenuAction(themeAction);
    ThemeManager::instance()->setCurrentTheme(KdenliveSettings::colortheme());
    connect(ThemeManager::instance(), SIGNAL(signalThemeChanged(const QString &)), this, SLOT(slotThemeChanged(const QString &)), Qt::DirectConnection);

    if (!KdenliveSettings::widgetstyle().isEmpty() && QString::compare(desktopStyle, KdenliveSettings::widgetstyle(), Qt::CaseInsensitive) != 0) {
        // User wants a custom widget style, init
        doChangeStyle();
    }
    else ThemeManager::instance()->slotChangePalette();

    new RenderingAdaptor(this);
    pCore->initialize();
    MltConnection::locateMeltAndProfilesPath(MltPath);

    KdenliveSettings::setCurrent_profile(KdenliveSettings::default_profile());

    // If using a custom profile, make sure the file exists or fallback to default
    if (KdenliveSettings::current_profile().startsWith(QStringLiteral("/")) && !QFile::exists(KdenliveSettings::current_profile())) {
        KMessageBox::sorry(this, i18n("Cannot find your default profile, switching to ATSC 1080p 25"));
        KdenliveSettings::setCurrent_profile("atsc_1080p_25");
        KdenliveSettings::setDefault_profile("atsc_1080p_25");
    }
    m_commandStack = new QUndoGroup;
    m_timelineArea = new QTabWidget(this);
    //m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_timelineArea->setMinimumHeight(200);
    // Hide tabbar
    QTabBar *bar = m_timelineArea->findChild<QTabBar *>();
    bar->setHidden(true);

    m_gpuAllowed = initEffects::parseEffectFiles(pCore->binController()->mltRepository());
    //initEffects::parseCustomEffectsFile();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(m_shortcutRemoveFocus, SIGNAL(activated()), this, SLOT(slotRemoveFocus()));

    /// Add Widgets
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, KdenliveSettings::verticaltabs() ? QTabWidget::East : QTabWidget::North);
    QToolBar *timelineTb = new QToolBar(this);//pCore->window()->toolBar("timelineToolBar");
    QWidget *ctn = new QWidget(this);
    QVBoxLayout *ctnLay = new QVBoxLayout;
    ctnLay->setSpacing(0);
    ctnLay->setContentsMargins(0, 0, 0, 0);
    ctn->setLayout(ctnLay);
    ctnLay->addWidget(timelineTb);
    QFrame *fr = new QFrame(this);
    fr->setFrameShape(QFrame::HLine);
    fr->setMaximumHeight(1);
    fr->setLineWidth(1);
    ctnLay->addWidget(fr);
    ctnLay->addWidget(m_timelineArea);
    ctn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setCentralWidget(ctn);

    m_projectBinDock = addDock(i18n("Project Bin"), QStringLiteral("project_bin"), pCore->bin());
    QDockWidget * libraryDock = addDock(i18n("Library"), QStringLiteral("library"), pCore->library());

    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), this);
    pCore->bin()->setMonitor(m_clipMonitor);
    connect(m_clipMonitor, &Monitor::showConfigDialog, this, &MainWindow::slotPreferences);
    connect(m_clipMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_clipMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_clipMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_clipMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);

    connect(pCore->bin(), SIGNAL(clipNeedsReload(QString,bool)),this, SLOT(slotUpdateClip(QString,bool)));
    connect(pCore->bin(), SIGNAL(findInTimeline(QString)), this, SLOT(slotClipInTimeline(QString)));

    //TODO deprecated, replace with Bin methods if necessary
    /*connect(m_projectList, SIGNAL(loadingIsOver()), this, SLOT(slotElapsedTime()));
    connect(m_projectList, SIGNAL(displayMessage(QString,int,MessageType)), this, SLOT(slotGotProgressInfo(QString,int,MessageType)));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
    connect(m_projectList, SIGNAL(updateProfile(QString)), this, SLOT(slotUpdateProjectProfile(QString)));
    connect(m_projectList, SIGNAL(refreshClip(QString,bool)), pCore->monitorManager(), SLOT(slotRefreshCurrentMonitor(QString)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));*/
    connect(m_clipMonitor, SIGNAL(extractZone(QString)), pCore->bin(), SLOT(slotStartCutJob(QString)));
    connect(m_clipMonitor, SIGNAL(passKeyPress(QKeyEvent*)), this, SLOT(triggerKey(QKeyEvent*)));

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), this);
    connect(m_projectMonitor, SIGNAL(passKeyPress(QKeyEvent*)), this, SLOT(triggerKey(QKeyEvent*)));
    connect(m_projectMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_projectMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_projectMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_projectMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    
    connect(m_projectMonitor, SIGNAL(updateGuide(int, QString)), this, SLOT(slotEditGuide(int, QString)));

/*
    //TODO disabled until ported to qml
    m_recMonitor = new RecMonitor(Kdenlive::RecordMonitor, pCore->monitorManager(), this);
    connect(m_recMonitor, SIGNAL(addProjectClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    connect(m_recMonitor, SIGNAL(addProjectClipList(QList<QUrl>)), this, SLOT(slotAddProjectClipList(QList<QUrl>)));

*/
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor, m_recMonitor);
    connect(m_clipMonitor, SIGNAL(addMasterEffect(QString,QDomElement)), pCore->bin(), SLOT(slotEffectDropped(QString,QDomElement)));

    // Audio spectrum scope
    m_audioSpectrum = new AudioGraphSpectrum(pCore->monitorManager());
    QDockWidget * spectrumDock = addDock(i18n("Audio Spectrum"), QStringLiteral("audiospectrum"), m_audioSpectrum);
    connect(this, &MainWindow::reloadTheme, m_audioSpectrum, &AudioGraphSpectrum::refreshPixmap);
    // Close library and audiospectrum on first run
    libraryDock->close();
    spectrumDock->close();

    m_effectStack = new EffectStackView2(m_projectMonitor, this);
    connect(m_effectStack, SIGNAL(startFilterJob(const ItemInfo&,const QString&,QMap<QString,QString>&,QMap<QString,QString>&,QMap<QString,QString>&)), pCore->bin(), SLOT(slotStartFilterJob(const ItemInfo &,const QString&,QMap<QString,QString>&,QMap<QString,QString>&,QMap<QString,QString>&)));
    connect(pCore->bin(), SIGNAL(masterClipSelected(ClipController *, Monitor *)), m_effectStack, SLOT(slotMasterClipItemSelected(ClipController *, Monitor *)));
    connect(pCore->bin(), SIGNAL(masterClipUpdated(ClipController *, Monitor *)), m_effectStack, SLOT(slotRefreshMasterClipEffects(ClipController *, Monitor *)));
    connect(m_effectStack, SIGNAL(addMasterEffect(QString,QDomElement)), pCore->bin(), SLOT(slotEffectDropped(QString,QDomElement)));
    connect(m_effectStack, SIGNAL(removeMasterEffect(QString,QDomElement)), pCore->bin(), SLOT(slotDeleteEffect(QString,QDomElement)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(QString,const QList <int>,int)), pCore->bin(), SLOT(slotMoveEffect(QString,const QList <int>,int)));
    connect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    connect(m_effectStack, SIGNAL(displayMessage(QString,int)), this, SLOT(slotGotProgressInfo(QString,int)));
    m_effectStackDock = addDock(i18n("Properties"), QStringLiteral("effect_stack"), m_effectStack);

    m_effectList = new EffectsListView();
    m_effectListDock = addDock(i18n("Effects"), QStringLiteral("effect_list"), m_effectList);

    m_transitionList = new EffectsListView(EffectsListView::TransitionMode);
    m_transitionListDock = addDock(i18n("Transitions"), QStringLiteral("transition_list"), m_transitionList);

    setupActions();
    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), QStringLiteral("clip_monitor"), m_clipMonitor);
    m_projectMonitorDock = addDock(i18n("Project Monitor"), QStringLiteral("project_monitor"), m_projectMonitor);
    if (m_recMonitor) {
        m_recMonitorDock = addDock(i18n("Record Monitor"), QStringLiteral("record_monitor"), m_recMonitor);
    }

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(KoIconUtils::themedIcon(QStringLiteral("edit-clear")));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoViewDock = addDock(i18n("Undo History"), QStringLiteral("undo_history"), m_undoView);

    // Color and icon theme stuff
    addAction(QStringLiteral("themes_menu"), themeAction);
    connect(m_commandStack, SIGNAL(cleanChanged(bool)), m_saveAction, SLOT(setDisabled(bool)));
    addAction(QStringLiteral("styles_menu"), stylesAction);

    // Close non-general docks for the initial layout
    // only show important ones
    m_undoViewDock->close();



    /// Tabify Widgets ///
    tabifyDockWidget(m_transitionListDock, m_effectListDock);
    tabifyDockWidget(pCore->bin()->clipPropertiesDock(), m_effectStackDock);
    //tabifyDockWidget(m_effectListDock, m_effectStackDock);

    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
    if (m_recMonitor) {
        tabifyDockWidget(m_clipMonitorDock, m_recMonitorDock);
    }

    readOptions();

    QAction *action;
    // Stop motion actions. Beware of the order, we MUST use the same order in stopmotion/stopmotion.cpp
    m_stopmotion_actions = new KActionCategory(i18n("Stop Motion"), actionCollection());
    action = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-record")), i18n("Capture frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_capture"), action);
    action = new QAction(i18n("Switch live / captured frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_switch"), action);
    action = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-paste")), i18n("Show last frame over video"), this);
    action->setCheckable(true);
    action->setChecked(false);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_overlay"), action);

    // Monitor Record action
    addAction(QStringLiteral("switch_monitor_rec"), m_clipMonitor->recAction());

    // Build effects menu
    m_effectsMenu = new QMenu(i18n("Add Effect"), this);
    m_effectActions = new KActionCategory(i18n("Effects"), actionCollection());
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);

    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    m_transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    m_transitionList->reloadEffectList(m_transitionsMenu, m_transitionActions);

    ScopeManager *scmanager = new ScopeManager(this);

    new LayoutManagement(this);
    new HideTitleBars(this);
    new TimelineSearch(this);
    m_extraFactory = new KXMLGUIClient(this);
    buildDynamicActions();

    // Add shortcut to action tooltips
    QList< KActionCollection * > collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        foreach( QAction* tempAction, coll->actions()) {
            // find the shortcut pattern and delete (note the preceding space in the RegEx)
            QString strippedTooltip = tempAction->toolTip().remove(QRegExp("\\s\\(.*\\)"));
            // append shortcut if it exists for action
            if (tempAction->shortcut() == QKeySequence(0))
                tempAction->setToolTip( strippedTooltip);
            else
                tempAction->setToolTip( strippedTooltip + " (" + tempAction->shortcut().toString() + ")");
        }
    }

    // Create Effect Basket (dropdown list of favorites)
    m_effectBasket = new EffectBasket(m_effectList);
    connect(m_effectBasket, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    QWidgetAction *widgetlist = new QWidgetAction(this);
    widgetlist->setDefaultWidget(m_effectBasket);
    //widgetlist->setText(i18n("Favorite Effects"));
    widgetlist->setToolTip(i18n("Favorite Effects"));
    widgetlist->setIcon(KoIconUtils::themedIcon("favorite"));
    QMenu *menu = new QMenu(this);
    menu->addAction(widgetlist);

    QToolButton *basketButton = new QToolButton(this);
    basketButton->setMenu(menu);
    basketButton->setToolButtonStyle(toolBar()->toolButtonStyle());
    basketButton->setDefaultAction(widgetlist);
    basketButton->setPopupMode(QToolButton::InstantPopup);
    //basketButton->setText(i18n("Favorite Effects"));
    basketButton->setToolTip(i18n("Favorite Effects"));
    basketButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("favorite")));

    QWidgetAction* toolButtonAction = new QWidgetAction(this);
    //toolButtonAction->setText(i18n("Favorite Effects"));
    toolButtonAction->setIcon(KoIconUtils::themedIcon("favorite"));
    toolButtonAction->setDefaultWidget(basketButton);

    //addAction(QStringLiteral("favorite_effects"), toolButtonAction);
    connect(toolButtonAction, SIGNAL(triggered(bool)), basketButton, SLOT(showMenu()));
    setupGUI();

    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/

    loadGenerators();
    loadDockActions();
    loadClipActions();

    // Connect monitor overlay info menu.
    QMenu *monitorOverlay = static_cast<QMenu*>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    connect(monitorOverlay, SIGNAL(triggered(QAction*)), this, SLOT(slotSwitchMonitorOverlay(QAction*)));

    m_projectMonitor->setupMenu(static_cast<QMenu*>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, NULL, m_loopClip);
    m_clipMonitor->setupMenu(static_cast<QMenu*>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, static_cast<QMenu*>(factory()->container(QStringLiteral("marker_menu"), this)));

    QMenu *clipInTimeline = static_cast<QMenu*>(factory()->container(QStringLiteral("clip_in_timeline"), this));
    clipInTimeline->setIcon(KoIconUtils::themedIcon(QStringLiteral("go-jump")));
    pCore->bin()->setupGeneratorMenu();

    connect(pCore->monitorManager(), SIGNAL(updateOverlayInfos(int,int)), this, SLOT(slotUpdateMonitorOverlays(int,int)));

    // Setup and fill effects and transitions menus.
    QMenu *m = static_cast<QMenu*>(factory()->container(QStringLiteral("video_effects_menu"), this));
    connect(m, SIGNAL(triggered(QAction*)), this, SLOT(slotAddVideoEffect(QAction*)));
    connect(m_effectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotAddVideoEffect(QAction*)));
    connect(m_transitionsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotAddTransition(QAction*)));

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);

    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("insert_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("delete_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("clip_in_project_tree")));
    //m_timelineContextClipMenu->addAction(actionCollection()->action("clip_to_project_tree"));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("delete_timeline_clip")));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("group_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("ungroup_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("split_audio")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("set_audio_align_ref")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("align_audio")));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("cut_timeline_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("paste_effects")));
    m_timelineContextClipMenu->addSeparator();

    QMenu *markersMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("marker_menu"), this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addMenu(m_transitionsMenu);
    m_timelineContextClipMenu->addMenu(m_effectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action(QStringLiteral("delete_timeline_clip")));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    m_timelineContextTransitionMenu->addAction(actionCollection()->action(QStringLiteral("auto_transition")));

    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    slotConnectMonitors();

    timelineTb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    KSelectAction *sceneMode = new KSelectAction(this);
    sceneMode->addAction(m_normalEditTool);
    sceneMode->addAction(m_overwriteEditTool);
    sceneMode->addAction(m_insertEditTool);
    sceneMode->setCurrentItem(0);
    timelineTb->addAction(sceneMode);
    timelineTb->addSeparator();
    timelineTb->addAction(m_buttonSelectTool);
    timelineTb->addAction(m_buttonRazorTool);
    timelineTb->addAction(m_buttonSpacerTool);

    timelineTb->addSeparator();
    m_timeFormatButton = new KSelectAction(QStringLiteral("00:00:00:00 / 00:00:00:00"), this);
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_timeFormatButton->setFont(fixedFont);
    m_timeFormatButton->addAction(i18n("hh:mm:ss:ff"));
    m_timeFormatButton->addAction(i18n("Frames"));
    if (KdenliveSettings::frametimecode()) m_timeFormatButton->setCurrentItem(1);
    else m_timeFormatButton->setCurrentItem(0);
    connect(m_timeFormatButton, SIGNAL(triggered(int)), this, SLOT(slotUpdateTimecodeFormat(int)));
    m_timeFormatButton->setToolBarMode(KSelectAction::MenuMode);
    m_timeFormatButton->setToolButtonPopupMode(QToolButton::InstantPopup);
    const QFontMetrics metric(fixedFont);
    int requiredWidth = metric.boundingRect(QStringLiteral("00:00:00:00 / 00:00:00:00")).width() + 20;
    timelineTb->addAction(m_timeFormatButton);
    QWidget *actionWidget = timelineTb->widgetForAction(m_timeFormatButton);
    actionWidget->setObjectName(QStringLiteral("timecode"));
    actionWidget->setMinimumWidth(requiredWidth);
    timelineTb->addSeparator();
    timelineTb->addAction(actionCollection()->action(QStringLiteral("insert_to_in_point")));
    timelineTb->addAction(actionCollection()->action(QStringLiteral("overwrite_to_in_point")));
    timelineTb->addAction(actionCollection()->action(QStringLiteral("remove_extract")));
    timelineTb->addAction(actionCollection()->action(QStringLiteral("remove_lift")));

    QToolButton *timelinePreview = new QToolButton(this);
    QMenu *tlMenu = new QMenu(this);
    timelinePreview->setMenu(tlMenu);
    QAction *prevRender = actionCollection()->action(QStringLiteral("prerender_timeline_zone"));
    tlMenu->addAction(prevRender);
    tlMenu->addAction(actionCollection()->action(QStringLiteral("set_render_timeline_zone")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("unset_render_timeline_zone")));
    timelinePreview->setDefaultAction(prevRender);
    timelineTb->addWidget(timelinePreview);

    timelineTb->addAction(toolButtonAction);
    QWidget *sep = new QWidget(this);
    sep->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    timelineTb->addWidget(sep);
    timelineTb->addAction(m_buttonAutomaticSplitAudio);
    timelineTb->addAction(m_buttonVideoThumbs);
    timelineTb->addAction(m_buttonAudioThumbs);
    timelineTb->addAction(m_buttonShowMarkers);
    timelineTb->addAction(m_buttonSnap);
    timelineTb->addSeparator();
    timelineTb->addAction(m_buttonFitZoom);
    timelineTb->addAction(m_zoomOut);
    timelineTb->addWidget(m_zoomSlider);
    timelineTb->addAction(m_zoomIn);

    // Populate encoding profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::DataLocation);
    if (KdenliveSettings::proxyparams().isEmpty() || KdenliveSettings::proxyextension().isEmpty()) {
        KConfigGroup group(&conf, "proxy");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setProxyparams(data.section(';', 0, 0));
            KdenliveSettings::setProxyextension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::v4l_parameters().isEmpty() || KdenliveSettings::v4l_extension().isEmpty()) {
        KConfigGroup group(&conf, "video4linux");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setV4l_parameters(data.section(';', 0, 0));
            KdenliveSettings::setV4l_extension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::tl_parameters().isEmpty() || KdenliveSettings::tl_extension().isEmpty()) {
        KConfigGroup group(&conf, "timelinepreview");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setTl_parameters(data.section(';', 0, 0));
            KdenliveSettings::setTl_extension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::grab_parameters().isEmpty() || KdenliveSettings::grab_extension().isEmpty()) {
        KConfigGroup group(&conf, "screengrab");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setGrab_parameters(data.section(';', 0, 0));
            KdenliveSettings::setGrab_extension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::decklink_parameters().isEmpty() || KdenliveSettings::decklink_extension().isEmpty()) {
        KConfigGroup group(&conf, "decklink");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setDecklink_parameters(data.section(';', 0, 0));
            KdenliveSettings::setDecklink_extension(data.section(';', 1, 1));
        }
    }
    statusBar()->show();
    emit GUISetupDone();
    pCore->projectManager()->init(Url, clipsToLoad);
    QTimer::singleShot(0, pCore->projectManager(), SLOT(slotLoadOnOpen()));
    connect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()), Qt::UniqueConnection);

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
    scmanager->slotCheckActiveScopes();
    //KMessageBox::information(this, "Warning, development version for testing only. we are currently working on core functionnalities,\ndo not save any project or your project files might be corrupted.");
}

void MainWindow::slotThemeChanged(const QString &theme)
{
    disconnect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()));
    KSharedConfigPtr config = KSharedConfig::openConfig(theme);
    setPalette(KColorScheme::createApplicationPalette(config));
    qApp->setPalette(palette());
    QPalette plt = palette();

    KdenliveSettings::setColortheme(theme);
    if (m_effectStack) {
        m_effectStack->updatePalette();
        m_effectStack->transitionConfig()->updatePalette();
    }
    if (m_effectList) m_effectList->updatePalette();
    if (m_transitionList) m_transitionList->updatePalette();
    if (m_clipMonitor) m_clipMonitor->setPalette(plt);
    if (m_projectMonitor) m_projectMonitor->setPalette(plt);
    setStatusBarStyleSheet(plt);
    if (pCore->projectManager() && pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->updatePalette();
    }
    if (m_timelineArea) {
        m_timelineArea->setPalette(plt);
    }
    /*if (statusBar()) {
        const QObjectList children = statusBar()->children();
        foreach(QObject * child, children) {
            if (child->isWidgetType())
                ((QWidget*)child)->setPalette(plt);
            const QObjectList subchildren = child->children();
            foreach(QObject * subchild, subchildren) {
                if (subchild->isWidgetType())
                    ((QWidget*)subchild)->setPalette(plt);
            }
        }
    }*/

#if KXMLGUI_VERSION_MINOR < 23 && KXMLGUI_VERSION_MAJOR == 5
    // Not required anymore with auto colored icons since KF5 5.23
    QColor background = plt.window().color();
    bool useDarkIcons = background.value() < 100;
    if (m_themeInitialized && useDarkIcons != m_isDarkTheme) {
        if (pCore->bin()) {
            pCore->bin()->refreshIcons();
        }
        if (m_clipMonitor) m_clipMonitor->refreshIcons();
        if (m_projectMonitor) m_projectMonitor->refreshIcons();
        if (pCore->monitorManager()) pCore->monitorManager()->refreshIcons();
        if (m_effectStack && m_effectStack->transitionConfig()) m_effectStack->transitionConfig()->refreshIcons();
        if (m_effectList) m_effectList->refreshIcons();
        if (m_effectStack) m_effectStack->refreshIcons();
        if (pCore->projectManager() && pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->refreshIcons();
        }

        foreach(QAction* action, actionCollection()->actions()) {
            QIcon icon = action->icon();
            if (icon.isNull()) continue;
            QString iconName = icon.name();
            QIcon newIcon = KoIconUtils::themedIcon(iconName);
            if (newIcon.isNull()) continue;
            action->setIcon(newIcon);
        }
    }
    m_themeInitialized = true;
    m_isDarkTheme = useDarkIcons;
#endif
    connect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()), Qt::UniqueConnection);
}

bool MainWindow::event(QEvent *e) {
    switch (e->type()) {
        case QEvent::ApplicationPaletteChange:
            emit reloadTheme();
            e->accept();
            break;
        default:
            break;
    }
    return KXmlGuiWindow::event(e);
}

void MainWindow::slotReloadTheme()
{
    ThemeManager::instance()->slotSettingsChanged();
}

MainWindow::~MainWindow()
{
    delete m_stopmotion;
    delete m_audioSpectrum;
    m_effectStack->slotClipItemSelected(NULL, m_projectMonitor);
    m_effectStack->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    if (m_projectMonitor) m_projectMonitor->stop();
    if (m_clipMonitor) m_clipMonitor->stop();
    delete pCore;
    delete m_effectStack;
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_shortcutRemoveFocus;
    qDeleteAll(m_transitions);
    Mlt::Factory::close();
}

//virtual
bool MainWindow::queryClose()
{
    if (m_renderWidget) {
        int waitingJobs = m_renderWidget->waitingJobsCount();
        if (waitingJobs > 0) {
            switch (KMessageBox::warningYesNoCancel(this, i18np("You have 1 rendering job waiting in the queue.\nWhat do you want to do with this job?", "You have %1 rendering jobs waiting in the queue.\nWhat do you want to do with these jobs?", waitingJobs), QString(), KGuiItem(i18n("Start them now")), KGuiItem(i18n("Delete them")))) {
            case KMessageBox::Yes :
                // create script with waiting jobs and start it
                if (m_renderWidget->startWaitingRenderJobs() == false) return false;
                break;
            case KMessageBox::No :
                // Don't do anything, jobs will be deleted
                break;
           default:
                return false;
            }
        }
    }
    saveOptions();

    // WARNING: According to KMainWindow::queryClose documentation we are not supposed to close the document here?
    return pCore->projectManager()->closeCurrentDocument(true, true);
}

void MainWindow::loadGenerators()
{
    QMenu *addMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("generators"), this));
    Generators::getGenerators(KdenliveSettings::producerslist(), addMenu);
    connect(addMenu, SIGNAL(triggered(QAction *)), this, SLOT(buildGenerator(QAction *)));
}

void MainWindow::buildGenerator(QAction *action)
{
    Generators gen(m_clipMonitor, action->data().toString(), this);
    if (gen.exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipToProject(gen.getSavedClip());
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here
    KXmlGuiWindow::saveProperties(config);
    //TODO: fix session management
    if (qApp->isSavingSession()) {
	if (pCore->projectManager()->current() && !pCore->projectManager()->current()->url().isEmpty()) {
	    config.writeEntry("kdenlive_lastUrl", pCore->projectManager()->current()->url().path());
	}
    }
}

void MainWindow::readProperties(const KConfigGroup &config)
{
    // read properties here
    KXmlGuiWindow::readProperties(config);
    //TODO: fix session management
    /*if (qApp->isSessionRestored()) {
	pCore->projectManager()->openFile(QUrl::fromLocalFile(config.readEntry("kdenlive_lastUrl", QString())));
    }*/
}

void MainWindow::saveNewToolbarConfig()
{
    KXmlGuiWindow::saveNewToolbarConfig();
    //TODO for some reason all dynamically inserted actions are removed by the save toolbar
    // So we currently re-add them manually....
    loadDockActions();
    loadClipActions();
    pCore->bin()->rebuildMenu();
}

void MainWindow::slotReloadEffects()
{
    initEffects::parseCustomEffectsFile();
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotFullScreen()
{
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action(QStringLiteral("fullscreen"))->isChecked());
}

void MainWindow::slotAddEffect(const QDomElement &effect)
{
    if (effect.isNull()) {
        //qDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    EFFECTMODE status = m_effectStack->effectStatus();
    if (status == TIMELINE_TRACK) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTrackEffect(effectToAdd, pCore->projectManager()->currentTimeline()->tracksCount() - m_effectStack->trackIndex());
    }
    else if (status == TIMELINE_CLIP) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddEffectToCurrentItem(effectToAdd);
    }
    else if (status == MASTER_CLIP) {
        pCore->bin()->slotEffectDropped(QString(), effectToAdd);
    }
}

void MainWindow::slotUpdateClip(const QString &id, bool reload)
{
    ProjectClip *clip = pCore->bin()->getBinClip(id);
    if (!clip) {
        return;
    }
    pCore->projectManager()->currentTimeline()->projectView()->slotUpdateClip(id, reload);
}

void MainWindow::slotConnectMonitors()
{
    //connect(m_projectList, SIGNAL(deleteProjectClips(QStringList,QMap<QString,QString>)), this, SLOT(slotDeleteProjectClips(QStringList,QMap<QString,QString>)));
    connect(m_projectMonitor->render, SIGNAL(gotFileProperties(requestClipInfo,ClipController *)), pCore->bin(), SLOT(slotProducerReady(requestClipInfo,ClipController *)), Qt::DirectConnection);

    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(QString)), pCore->bin(), SLOT(slotRefreshClipThumbnail(QString)));
    connect(m_projectMonitor, SIGNAL(requestFrameForAnalysis(bool)), this, SLOT(slotMonitorRequestRenderFrame(bool)));
    connect(m_projectMonitor, &Monitor::createSplitOverlay, this, &MainWindow::createSplitOverlay);
    connect(m_projectMonitor, &Monitor::removeSplitOverlay, this, &MainWindow::removeSplitOverlay);
}

void MainWindow::createSplitOverlay(Mlt::Filter *filter)
{
    if (pCore->projectManager()->currentTimeline()) {
        if (pCore->projectManager()->currentTimeline()->projectView()->createSplitOverlay(filter)) {
            m_projectMonitor->activateSplit();
        }
    }
}

void MainWindow::removeSplitOverlay()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->removeSplitOverlay();
    }
}

void MainWindow::addAction(const QString &name, QAction *action)
{
    m_actionNames.append(name);
    actionCollection()->addAction(name, action);
    actionCollection()->setDefaultShortcut(action, action->shortcut()); //Fix warning about setDefaultShortcut
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QObject *receiver,
                           const char *member, const QIcon &icon, const QKeySequence &shortcut)
{
    QAction *action = new QAction(text, this);
    if (!icon.isNull()) {
        action->setIcon(icon);
    }
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    addAction(name, action);
    connect(action, SIGNAL(triggered(bool)), receiver, member);

    return action;
}

void MainWindow::setupActions()
{
    m_statusProgressBar = new QProgressBar(this);
    m_statusProgressBar->setMinimum(0);
    m_statusProgressBar->setMaximum(100);
    m_statusProgressBar->setMaximumWidth(150);
    m_statusProgressBar->setVisible(false);

    /*KToolBar *toolbar = new KToolBar(QStringLiteral("statusToolBar"), this, Qt::BottomToolBarArea);
    toolbar->setMovable(false);

    setStatusBarStyleSheet(palette());
    QString styleBorderless = QStringLiteral("QToolButton { border-width: 0px;margin: 1px 3px 0px;padding: 0px;}");*/

    //create edit mode buttons
    m_normalEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-normal-edit")), i18n("Normal mode"), this);
    m_normalEditTool->setShortcut(i18nc("Normal editing", "n"));
    //toolbar->addAction(m_normalEditTool);
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    m_overwriteEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-overwrite-edit")), i18n("Overwrite mode"), this);
    //m_overwriteEditTool->setShortcut(i18nc("Overwrite mode shortcut", "o"));
    //toolbar->addAction(m_overwriteEditTool);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    m_insertEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-insert-edit")), i18n("Insert mode"), this);
    //m_insertEditTool->setShortcut(i18nc("Insert mode shortcut", "i"));
    //toolbar->addAction(m_insertEditTool);
    m_insertEditTool->setCheckable(true);
    m_insertEditTool->setChecked(false);

    QActionGroup *editGroup = new QActionGroup(this);
    editGroup->addAction(m_normalEditTool);
    editGroup->addAction(m_overwriteEditTool);
    editGroup->addAction(m_insertEditTool);
    editGroup->setExclusive(true);
    connect(editGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeEdit(QAction*)));
    //toolbar->addSeparator();

    // create tools buttons
    m_buttonSelectTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("cursor-arrow")), i18n("Selection tool"), this);
    m_buttonSelectTool->setShortcut(i18nc("Selection tool shortcut", "s"));
    //toolbar->addAction(m_buttonSelectTool);
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-cut")), i18n("Razor tool"), this);
    m_buttonRazorTool->setShortcut(i18nc("Razor tool shortcut", "x"));
    //toolbar->addAction(m_buttonRazorTool);
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_buttonSpacerTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("distribute-horizontal-x")), i18n("Spacer tool"), this);
    m_buttonSpacerTool->setShortcut(i18nc("Spacer tool shortcut", "m"));
    //toolbar->addAction(m_buttonSpacerTool);
    m_buttonSpacerTool->setCheckable(true);
    m_buttonSpacerTool->setChecked(false);
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->addAction(m_buttonSelectTool);
    toolGroup->addAction(m_buttonRazorTool);
    toolGroup->addAction(m_buttonSpacerTool);
    toolGroup->setExclusive(true);
    //toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    /*QWidget * actionWidget;
    int max = toolbar->iconSizeDefault() + 2;
    actionWidget = toolbar->widgetForAction(m_normalEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_insertEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_overwriteEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSelectTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonRazorTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSpacerTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);*/

    connect(toolGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeTool(QAction*)));

    //toolbar->addSeparator();
    m_buttonFitZoom = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-best")), i18n("Fit zoom to project"), this);
    //toolbar->addAction(m_buttonFitZoom);
    m_buttonFitZoom->setCheckable(false);

    m_zoomOut = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-out")), i18n("Zoom Out"), this);
    //toolbar->addAction(m_zoomOut);
    m_zoomOut->setShortcut(Qt::CTRL + Qt::Key_Minus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setInvertedAppearance(true);
    m_zoomSlider->setInvertedControls(true);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);
    //toolbar->addWidget(m_zoomSlider);

    m_zoomIn = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-in")), i18n("Zoom In"), this);
    //toolbar->addAction(m_zoomIn);
    m_zoomIn->setShortcut(Qt::CTRL + Qt::Key_Plus);

    /*actionWidget = toolbar->widgetForAction(m_buttonFitZoom);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);

    actionWidget = toolbar->widgetForAction(m_zoomIn);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);

    actionWidget = toolbar->widgetForAction(m_zoomOut);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);*/

    connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSetZoom(int)));
    connect(m_zoomSlider, SIGNAL(sliderMoved(int)), this, SLOT(slotShowZoomSliderToolTip(int)));
    connect(m_buttonFitZoom, SIGNAL(triggered()), this, SLOT(slotFitZoom()));
    connect(m_zoomIn, SIGNAL(triggered(bool)), this, SLOT(slotZoomIn()));
    connect(m_zoomOut, SIGNAL(triggered(bool)), this, SLOT(slotZoomOut()));

    //toolbar->addSeparator();

    //create automatic audio split button
    m_buttonAutomaticSplitAudio = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-split-audio")), i18n("Split audio and video automatically"), this);
    //toolbar->addAction(m_buttonAutomaticSplitAudio);
    m_buttonAutomaticSplitAudio->setCheckable(true);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    connect(m_buttonAutomaticSplitAudio, &QAction::toggled, this, &MainWindow::slotSwitchSplitAudio);

    m_buttonVideoThumbs = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-videothumb")), i18n("Show video thumbnails"), this);
    //toolbar->addAction(m_buttonVideoThumbs);
    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchVideoThumbs()));

    m_buttonAudioThumbs = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-audiothumb")), i18n("Show audio thumbnails"), this);
    //toolbar->addAction(m_buttonAudioThumbs);
    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchAudioThumbs()));

    m_buttonShowMarkers = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-markers")), i18n("Show markers comments"), this);
    //toolbar->addAction(m_buttonShowMarkers);
    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, SIGNAL(triggered()), this, SLOT(slotSwitchMarkersComments()));
    m_buttonSnap = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-snap")), i18n("Snap"), this);
    //toolbar->addAction(m_buttonSnap);
    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, SIGNAL(triggered()), this, SLOT(slotSwitchSnap()));

    /*actionWidget = toolbar->widgetForAction(m_buttonAutomaticSplitAudio);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonVideoThumbs);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonAudioThumbs);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonShowMarkers);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSnap);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);*/

    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    statusBar()->addWidget(m_messageLabel, 10);
    statusBar()->addWidget(m_statusProgressBar, 0);
    //statusBar()->addPermanentWidget(toolbar);

    addAction(QStringLiteral("normal_mode"), m_normalEditTool);
    addAction(QStringLiteral("overwrite_mode"), m_overwriteEditTool);
    addAction(QStringLiteral("insert_mode"), m_insertEditTool);
    addAction(QStringLiteral("select_tool"), m_buttonSelectTool);
    addAction(QStringLiteral("razor_tool"), m_buttonRazorTool);
    addAction(QStringLiteral("spacer_tool"), m_buttonSpacerTool);

    addAction(QStringLiteral("automatic_split_audio"), m_buttonAutomaticSplitAudio);
    addAction(QStringLiteral("show_video_thumbs"), m_buttonVideoThumbs);
    addAction(QStringLiteral("show_audio_thumbs"), m_buttonAudioThumbs);
    addAction(QStringLiteral("show_markers"), m_buttonShowMarkers);
    addAction(QStringLiteral("snap"), m_buttonSnap);
    addAction(QStringLiteral("zoom_fit"), m_buttonFitZoom);
    addAction(QStringLiteral("zoom_in"), m_zoomIn);
    addAction(QStringLiteral("zoom_out"), m_zoomOut);

    KNS3::standardAction(i18n("Download New Wipes..."),            this, SLOT(slotGetNewLumaStuff()),       actionCollection(), "get_new_lumas");
    KNS3::standardAction(i18n("Download New Render Profiles..."),  this, SLOT(slotGetNewRenderStuff()),     actionCollection(), "get_new_profiles");
    KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");
    KNS3::standardAction(i18n("Download New Title Templates..."),  this, SLOT(slotGetNewTitleStuff()),      actionCollection(), "get_new_titles");

    addAction(QStringLiteral("run_wizard"), i18n("Run Config Wizard"), this, SLOT(slotRunWizard()), KoIconUtils::themedIcon(QStringLiteral("tools-wizard")));
    addAction(QStringLiteral("project_settings"), i18n("Project Settings"), this, SLOT(slotEditProjectSettings()), KoIconUtils::themedIcon(QStringLiteral("configure")));
    addAction(QStringLiteral("project_render"), i18n("Render"), this, SLOT(slotRenderProject()), KoIconUtils::themedIcon(QStringLiteral("media-record")), Qt::CTRL + Qt::Key_Return);

    addAction(QStringLiteral("project_clean"), i18n("Clean Project"), this, SLOT(slotCleanProject()), KoIconUtils::themedIcon(QStringLiteral("edit-clear")));
    //TODO
    //addAction("project_adjust_profile", i18n("Adjust Profile to Current Clip"), pCore->bin(), SLOT(adjustProjectProfileToItem()));

    m_playZone = addAction(QStringLiteral("monitor_play_zone"), i18n("Play Zone"), pCore->monitorManager(), SLOT(slotPlayZone()),
                           KoIconUtils::themedIcon(QStringLiteral("media-playback-start")), Qt::CTRL + Qt::Key_Space);
    m_loopZone = addAction(QStringLiteral("monitor_loop_zone"), i18n("Loop Zone"), pCore->monitorManager(), SLOT(slotLoopZone()),
                           KoIconUtils::themedIcon(QStringLiteral("media-playback-start")), Qt::ALT + Qt::Key_Space);
    m_loopClip = addAction(QStringLiteral("monitor_loop_clip"), i18n("Loop selected clip"), m_projectMonitor, SLOT(slotLoopClip()), KoIconUtils::themedIcon(QStringLiteral("media-playback-start")));
    m_loopClip->setEnabled(false);

    addAction(QStringLiteral("dvd_wizard"), i18n("DVD Wizard"), this, SLOT(slotDvdWizard()), KoIconUtils::themedIcon(QStringLiteral("media-optical")));
    addAction(QStringLiteral("transcode_clip"), i18n("Transcode Clips"), this, SLOT(slotTranscodeClip()), KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    addAction(QStringLiteral("archive_project"), i18n("Archive Project"), this, SLOT(slotArchiveProject()), KoIconUtils::themedIcon(QStringLiteral("document-save-all")));
    addAction(QStringLiteral("switch_monitor"), i18n("Switch monitor"), this, SLOT(slotSwitchMonitors()), QIcon(), Qt::Key_T);
    addAction(QStringLiteral("expand_timeline_clip"), i18n("Expand Clip"), pCore->projectManager(), SLOT(slotExpandClip()), KoIconUtils::themedIcon(QStringLiteral("document-open")));

    QAction *overlayInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Info Overlay"), this);
    addAction(QStringLiteral("monitor_overlay"), overlayInfo);
    overlayInfo->setCheckable(true);
    overlayInfo->setData(0x01);

    QAction *overlayTCInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Timecode"), this);
    addAction(QStringLiteral("monitor_overlay_tc"), overlayTCInfo);
    overlayTCInfo->setCheckable(true);
    overlayTCInfo->setData(0x02);

    QAction *overlayMarkerInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Markers"), this);
    addAction(QStringLiteral("monitor_overlay_markers"), overlayMarkerInfo);
    overlayMarkerInfo->setCheckable(true);
    overlayMarkerInfo->setData(0x04);

    QAction *overlaySafeInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Safe Zones"), this);
    addAction(QStringLiteral("monitor_overlay_safezone"), overlaySafeInfo);
    overlaySafeInfo->setCheckable(true);
    overlaySafeInfo->setData(0x08);
    
    QAction *overlayAudioInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Audio Waveform"), this);
    addAction(QStringLiteral("monitor_overlay_audiothumb"), overlayAudioInfo);
    overlayAudioInfo->setCheckable(true);
    overlayAudioInfo->setData(0x10);

    QAction *dropFrames = new QAction(QIcon(), i18n("Real Time (drop frames)"), this);
    dropFrames->setCheckable(true);
    dropFrames->setChecked(KdenliveSettings::monitor_dropframes());
    connect(dropFrames, SIGNAL(toggled(bool)), this, SLOT(slotSwitchDropFrames(bool)));

    KSelectAction *monitorGamma = new KSelectAction(i18n("Monitor Gamma"), this);
    monitorGamma->addAction(i18n("sRGB (computer)"));
    monitorGamma->addAction(i18n("Rec. 709 (TV)"));
    addAction(QStringLiteral("mlt_gamma"), monitorGamma);
    monitorGamma->setCurrentItem(KdenliveSettings::monitor_gamma());
    connect(monitorGamma, SIGNAL(triggered(int)), this, SLOT(slotSetMonitorGamma(int)));

    addAction(QStringLiteral("insert_project_tree"), i18n("Insert Zone in Project Bin"), this, SLOT(slotInsertZoneToTree()), QIcon(), Qt::CTRL + Qt::Key_I);
    addAction(QStringLiteral("insert_timeline"), i18n("Insert Zone in Timeline"), this, SLOT(slotInsertZoneToTimeline()), QIcon(), Qt::SHIFT + Qt::CTRL + Qt::Key_I);

    QAction *resizeStart =  new QAction(QIcon(), i18n("Resize Item Start"), this);
    addAction(QStringLiteral("resize_timeline_clip_start"), resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemStart()));

    QAction *resizeEnd =  new QAction(QIcon(), i18n("Resize Item End"), this);
    addAction(QStringLiteral("resize_timeline_clip_end"), resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemEnd()));

    addAction(QStringLiteral("monitor_seek_snap_backward"), i18n("Go to Previous Snap Point"), this, SLOT(slotSnapRewind()),
              KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), Qt::ALT + Qt::Key_Left);
    addAction(QStringLiteral("seek_clip_start"), i18n("Go to Clip Start"), this, SLOT(slotClipStart()), KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), Qt::Key_Home);
    addAction(QStringLiteral("seek_clip_end"), i18n("Go to Clip End"), this, SLOT(slotClipEnd()), KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), Qt::Key_End);
    addAction(QStringLiteral("monitor_seek_snap_forward"), i18n("Go to Next Snap Point"), this, SLOT(slotSnapForward()),
              KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), Qt::ALT + Qt::Key_Right);
    addAction(QStringLiteral("delete_timeline_clip"), i18n("Delete Selected Item"), this, SLOT(slotDeleteItem()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")), Qt::Key_Delete);
    addAction(QStringLiteral("align_playhead"), i18n("Align Playhead to Mouse Position"), this, SLOT(slotAlignPlayheadToMousePos()), QIcon(), Qt::Key_P);

    QAction *stickTransition = new QAction(i18n("Automatic Transition"), this);
    stickTransition->setData(QStringLiteral("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    addAction(QStringLiteral("auto_transition"), stickTransition);
    connect(stickTransition, SIGNAL(triggered(bool)), this, SLOT(slotAutoTransition()));

    addAction(QStringLiteral("group_clip"), i18n("Group Clips"), this, SLOT(slotGroupClips()), KoIconUtils::themedIcon(QStringLiteral("object-group")), Qt::CTRL + Qt::Key_G);

    QAction * ungroupClip = addAction(QStringLiteral("ungroup_clip"), i18n("Ungroup Clips"), this, SLOT(slotUnGroupClips()), KoIconUtils::themedIcon(QStringLiteral("object-ungroup")), Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");

    addAction(QStringLiteral("edit_item_duration"), i18n("Edit Duration"), this, SLOT(slotEditItemDuration()), KoIconUtils::themedIcon(QStringLiteral("measure")));
    addAction(QStringLiteral("clip_in_project_tree"), i18n("Clip in Project Bin"), this, SLOT(slotClipInProjectTree()), KoIconUtils::themedIcon(QStringLiteral("go-jump-definition")));
    addAction(QStringLiteral("overwrite_to_in_point"), i18n("Insert Clip Zone in Timeline (Overwrite)"), this, SLOT(slotInsertClipOverwrite()), KoIconUtils::themedIcon(QStringLiteral("insert-horizontal-rule")), Qt::Key_B);
    addAction(QStringLiteral("insert_to_in_point"), i18n("Insert Clip Zone in Timeline (Insert)"), this, SLOT(slotInsertClipInsert()), KoIconUtils::themedIcon(QStringLiteral("insert-horizontal-rule")), Qt::Key_V);
    addAction(QStringLiteral("remove_extract"), i18n("Extract Timeline Zone"), this, SLOT(slotExtractZone()), KoIconUtils::themedIcon(QStringLiteral("list-remove")), Qt::SHIFT + Qt::Key_X);
    addAction(QStringLiteral("remove_lift"), i18n("Lift Timeline Zone"), this, SLOT(slotLiftZone()), KoIconUtils::themedIcon(QStringLiteral("list-remove")), Qt::Key_Z);
    addAction(QStringLiteral("set_render_timeline_zone"), i18n("Add Preview Zone"), this, SLOT(slotDefinePreviewRender()), KoIconUtils::themedIcon(QStringLiteral("insert-horizontal-rule")));
    addAction(QStringLiteral("unset_render_timeline_zone"), i18n("Unset Preview Zone"), this, SLOT(slotRemovePreviewRender()), KoIconUtils::themedIcon(QStringLiteral("insert-horizontal-rule")));
    addAction(QStringLiteral("prerender_timeline_zone"), i18n("Start Preview Render"), this, SLOT(slotPreviewRender()), KoIconUtils::themedIcon(QStringLiteral("player-time")), QKeySequence(Qt::SHIFT + Qt::Key_Return));

    addAction(QStringLiteral("select_timeline_clip"), i18n("Select Clip"), this, SLOT(slotSelectTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_clip"), i18n("Deselect Clip"), this, SLOT(slotDeselectTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_clip"), i18n("Add Clip To Selection"), this, SLOT(slotSelectAddTimelineClip()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::ALT + Qt::Key_Plus);
    addAction(QStringLiteral("select_timeline_transition"), i18n("Select Transition"), this, SLOT(slotSelectTimelineTransition()),
          KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::SHIFT + Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_transition"), i18n("Deselect Transition"), this, SLOT(slotDeselectTimelineTransition()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::SHIFT + Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_transition"), i18n("Add Transition To Selection"), this, SLOT(slotSelectAddTimelineTransition()),
          KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    addAction(QStringLiteral("cut_timeline_clip"), i18n("Cut Clip"), this, SLOT(slotCutTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-cut")), Qt::SHIFT + Qt::Key_R);
    addAction(QStringLiteral("add_clip_marker"), i18n("Add Marker"), this, SLOT(slotAddClipMarker()), KoIconUtils::themedIcon(QStringLiteral("bookmark-new")));
    addAction(QStringLiteral("delete_clip_marker"), i18n("Delete Marker"), this, SLOT(slotDeleteClipMarker()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("delete_all_clip_markers"), i18n("Delete All Markers"), this, SLOT(slotDeleteAllClipMarkers()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));

    QAction * editClipMarker = addAction(QStringLiteral("edit_clip_marker"), i18n("Edit Marker"), this, SLOT(slotEditClipMarker()), KoIconUtils::themedIcon(QStringLiteral("document-properties")));
    editClipMarker->setData(QStringLiteral("edit_marker"));

    addAction(QStringLiteral("add_marker_guide_quickly"), i18n("Add Marker/Guide quickly"), this, SLOT(slotAddMarkerGuideQuickly()),
              KoIconUtils::themedIcon(QStringLiteral("bookmark-new")), Qt::Key_Asterisk);

    QAction * splitAudio = addAction(QStringLiteral("split_audio"), i18n("Split Audio"), this, SLOT(slotSplitAudio()), KoIconUtils::themedIcon(QStringLiteral("document-new")));
    // "A+V" as data means this action should only be available for clips with audio AND video
    splitAudio->setData("A+V");

    QAction * setAudioAlignReference = addAction(QStringLiteral("set_audio_align_ref"), i18n("Set Audio Reference"), this, SLOT(slotSetAudioAlignReference()));
    // "A" as data means this action should only be available for clips with audio
    setAudioAlignReference->setData("A");

    QAction * alignAudio = addAction(QStringLiteral("align_audio"), i18n("Align Audio to Reference"), this, SLOT(slotAlignAudio()), QIcon());
    // "A" as data means this action should only be available for clips with audio
    alignAudio->setData("A");

    QAction * audioOnly = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Audio Only"), this);
    addAction(QStringLiteral("clip_audio_only"), audioOnly);
    audioOnly->setData(PlaylistState::AudioOnly);
    audioOnly->setCheckable(true);

    QAction * videoOnly = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Video Only"), this);
    addAction(QStringLiteral("clip_video_only"), videoOnly);
    videoOnly->setData(PlaylistState::VideoOnly);
    videoOnly->setCheckable(true);

    QAction * audioAndVideo = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Audio and Video"), this);
    addAction(QStringLiteral("clip_audio_and_video"), audioAndVideo);
    audioAndVideo->setData(PlaylistState::Original);
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotUpdateClipType(QAction*)));
    m_clipTypeGroup->setEnabled(false);

    addAction(QStringLiteral("insert_space"), i18n("Insert Space"), this, SLOT(slotInsertSpace()));
    addAction(QStringLiteral("delete_space"), i18n("Remove Space"), this, SLOT(slotRemoveSpace()));

    KActionCategory *timelineActions = new KActionCategory(i18n("Tracks"), actionCollection());
    QAction *insertTrack = new QAction(QIcon(), i18n("Insert Track"), this);
    connect(insertTrack, &QAction::triggered, this, &MainWindow::slotInsertTrack);
    timelineActions->addAction(QStringLiteral("insert_track"), insertTrack);

    QAction *deleteTrack = new QAction(QIcon(), i18n("Delete Track"), this);
    connect(deleteTrack, &QAction::triggered, this, &MainWindow::slotDeleteTrack);
    timelineActions->addAction(QStringLiteral("delete_track"), deleteTrack);
    deleteTrack->setData("delete_track");

    QAction *configTracks = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure Tracks"), this);
    connect(configTracks, &QAction::triggered, this, &MainWindow::slotConfigTrack);
    timelineActions->addAction(QStringLiteral("config_tracks"), configTracks);

    QAction *selectTrack = new QAction(QIcon(), i18n("Select All in Current Track"), this);
    connect(selectTrack, &QAction::triggered, this, &MainWindow::slotSelectTrack);
    timelineActions->addAction(QStringLiteral("select_track"), selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), this);
    selectAll->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-select-all")));
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("select_all_tracks"), selectAll);

    kdenliveCategoryMap.insert(QStringLiteral("timeline"), timelineActions);

    addAction(QStringLiteral("add_guide"), i18n("Add Guide"), this, SLOT(slotAddGuide()), KoIconUtils::themedIcon(QStringLiteral("document-new")));
    addAction(QStringLiteral("delete_guide"), i18n("Delete Guide"), this, SLOT(slotDeleteGuide()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("edit_guide"), i18n("Edit Guide"), this, SLOT(slotEditGuide()), KoIconUtils::themedIcon(QStringLiteral("document-properties")));
    addAction(QStringLiteral("delete_all_guides"), i18n("Delete All Guides"), this, SLOT(slotDeleteAllGuides()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));

    QAction *pasteEffects = addAction(QStringLiteral("paste_effects"), i18n("Paste Effects"), this, SLOT(slotPasteEffects()), KoIconUtils::themedIcon(QStringLiteral("edit-paste")));
    pasteEffects->setData("paste_effects");

    m_saveAction = KStandardAction::save(pCore->projectManager(), SLOT(saveFile()), actionCollection());
    m_saveAction->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-save")));

    addAction(QStringLiteral("save_selection"), i18n("Save Selection"), pCore->projectManager(), SLOT(slotSaveSelection()), KoIconUtils::themedIcon(QStringLiteral("document-save")));

    QAction *sentToLibrary = addAction(QStringLiteral("send_library"), i18n("Add Selection to Library"), pCore->library(), SLOT(slotAddToLibrary()), KoIconUtils::themedIcon(QStringLiteral("bookmark-new")));
    
    pCore->library()->setupActions(QList <QAction *>() << sentToLibrary);

    QAction *a = KStandardAction::quit(this, SLOT(close()),                  actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("application-exit")));
    // TODO: make the following connection to slotEditKeys work
    //KStandardAction::keyBindings(this,            SLOT(slotEditKeys()),           actionCollection());
    a = KStandardAction::preferences(this, SLOT(slotPreferences()),        actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    a = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    a = KStandardAction::copy(this,                   SLOT(slotCopy()),               actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    a = KStandardAction::paste(this,                  SLOT(slotPaste()),              actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-paste")));
    a = KStandardAction::fullScreen(this,             SLOT(slotFullScreen()), this,   actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-fullscreen")));

    QAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-undo")));
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    QAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-redo")));
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    QMenu *addClips = new QMenu();

    QAction *addClip = addAction(QStringLiteral("add_clip"), i18n("Add Clip"), pCore->bin(), SLOT(slotAddClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-clip")));
    addClips->addAction(addClip);
    QAction *action = addAction(QStringLiteral("add_color_clip"), i18n("Add Color Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-color-clip")));
    action->setData((int) Color);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_slide_clip"), i18n("Add Slideshow Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-slide-clip")));
    action->setData((int) SlideShow);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_text_clip"), i18n("Add Title Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) Text);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_text_template_clip"), i18n("Add Template Title"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) TextTemplate);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_qtext_clip"), i18n("Add Simple Text Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) QText);
    addClips->addAction(action);

    QAction *addFolder = addAction(QStringLiteral("add_folder"), i18n("Create Folder"), pCore->bin(), SLOT(slotAddFolder()), KoIconUtils::themedIcon(QStringLiteral("folder-new")));
    addClips->addAction(addAction(QStringLiteral("download_resource"), i18n("Online Resources"), this, SLOT(slotDownloadResources()), KoIconUtils::themedIcon(QStringLiteral("edit-download"))));
    
    QAction *clipProperties = addAction(QStringLiteral("clip_properties"), i18n("Clip Properties"), pCore->bin(), SLOT(slotSwitchClipProperties(bool)), KoIconUtils::themedIcon(QStringLiteral("document-edit")));
    clipProperties->setCheckable(true);
    clipProperties->setData("clip_properties");

    QAction *openClip = addAction(QStringLiteral("edit_clip"), i18n("Edit Clip"), pCore->bin(), SLOT(slotOpenClip()), KoIconUtils::themedIcon(QStringLiteral("document-open")));
    openClip->setData("edit_clip");
    openClip->setEnabled(false);

    QAction *deleteClip = addAction(QStringLiteral("delete_clip"), i18n("Delete Clip"), pCore->bin(), SLOT(slotDeleteClip()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    deleteClip->setData("delete_clip");
    deleteClip->setEnabled(false);

    QAction *reloadClip = addAction(QStringLiteral("reload_clip"), i18n("Reload Clip"), pCore->bin(), SLOT(slotReloadClip()), KoIconUtils::themedIcon(QStringLiteral("view-refresh")));
    reloadClip->setData("reload_clip");
    reloadClip->setEnabled(false);

    QAction *disableEffects = addAction(QStringLiteral("disable_timeline_effects"), i18n("Disable Timeline Effects"), pCore->projectManager(), SLOT(slotDisableTimelineEffects(bool)), KoIconUtils::themedIcon(QStringLiteral("favorite")));
    disableEffects->setData("disable_timeline_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);

    QAction *duplicateClip = addAction(QStringLiteral("duplicate_clip"), i18n("Duplicate Clip"), pCore->bin(), SLOT(slotDuplicateClip()), KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    duplicateClip->setData("duplicate_clip");
    duplicateClip->setEnabled(false);

    QAction *proxyClip = new QAction(i18n("Proxy Clip"), this);
    addAction(QStringLiteral("proxy_clip"), proxyClip);
    proxyClip->setData(QStringList() << QString::number((int) AbstractClipJob::PROXYJOB));
    proxyClip->setCheckable(true);
    proxyClip->setChecked(false);

    //TODO: port stopmotion to new Monitor code
    //addAction("stopmotion", i18n("Stop Motion Capture"), this, SLOT(slotOpenStopmotion()), KoIconUtils::themedIcon("image-x-generic"));
    addAction(QStringLiteral("switch_track_lock"), i18n("Toggle Track Lock"), pCore->projectManager(), SLOT(slotSwitchTrackLock()), QIcon(), Qt::SHIFT + Qt::Key_L);
    addAction(QStringLiteral("switch_all_track_lock"), i18n("Toggle All Track Lock"), pCore->projectManager(), SLOT(slotSwitchAllTrackLock()), QIcon(), Qt::CTRL + Qt::SHIFT + Qt::Key_L);
    addAction(QStringLiteral("switch_track_target"), i18n("Toggle Track Target"), pCore->projectManager(), SLOT(slotSwitchTrackTarget()), QIcon(), Qt::SHIFT + Qt::Key_T);

    QHash <QString, QAction*> actions;
    actions.insert(QStringLiteral("reload"), reloadClip);
    actions.insert(QStringLiteral("duplicate"), duplicateClip);
    actions.insert(QStringLiteral("proxy"), proxyClip);
    actions.insert(QStringLiteral("properties"), clipProperties);
    actions.insert(QStringLiteral("open"), openClip);
    actions.insert(QStringLiteral("delete"), deleteClip);
    actions.insert(QStringLiteral("folder"), addFolder);
    pCore->bin()->setupMenu(addClips, addClip, actions);

    // Setup effects and transitions actions.
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    //m_transitions = new QAction*[transitions.count()];
    for (int i = 0; i < transitions.count(); ++i) {
        QStringList effectInfo = transitions.effectIdInfo(i);
        if (effectInfo.isEmpty()) continue;
        QAction *a = new QAction(effectInfo.at(0), this);
        a->setData(effectInfo);
        a->setIconVisibleInMenu(false);
        m_transitions << a;
        QString id = effectInfo.at(2);
        if (id.isEmpty()) id = effectInfo.at(1);
        transitionActions->addAction("transition_" + id, a);
    }
}

void MainWindow::setStatusBarStyleSheet(const QPalette &p)
{
    return;
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor buttonBg = scheme.background(KColorScheme::LinkBackground).color();
    QColor buttonBord = scheme.foreground(KColorScheme::LinkText).color();
    QColor buttonBord2 = scheme.shade(KColorScheme::LightShade);
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar QLabel {font-size:%1pt;} QStatusBar::item { border: 0px; font-size:%1pt;padding:0px; }").arg(statusBar()->font().pointSize()));
    QString style1 = QStringLiteral("QToolBar { border: 0px } QToolButton { border-style: inset; border:1px solid transparent;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton#timecode {padding-right:10px;} QToolButton:hover { background: %3;border-style: inset; border:1px solid %3;border-radius: 3px;} QToolButton:checked { background-color: %1; border-style: inset; border:1px solid %2;border-radius: 3px;}").arg(buttonBg.name(), buttonBord.name(), buttonBord2.name());
    statusBar()->setStyleSheet(style1);
}

void MainWindow::saveOptions()
{
    KdenliveSettings::self()->save();
}

void MainWindow::readOptions()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    pCore->projectManager()->recentFilesAction()->loadEntries(KConfigGroup(config, "Recent Files"));

    if (KdenliveSettings::defaultprojectfolder().isEmpty()) {
        QDir dir(QDir::homePath());
        if (!dir.mkdir(QStringLiteral("kdenlive"))) {
            qDebug() << "/// ERROR CREATING PROJECT FOLDER: ";
        } else {
            dir.cd("kdenlive");
            KdenliveSettings::setDefaultprojectfolder(dir.absolutePath());
        }
    }
    if (KdenliveSettings::trackheight() == 0) {
        QFontMetrics metrics(font());
        int trackHeight = 2 * metrics.height();
        QStyle *style = qApp->style();
        trackHeight += style->pixelMetric(QStyle::PM_ToolBarIconSize) + 2 * style->pixelMetric(QStyle::PM_ToolBarItemMargin) + style->pixelMetric(QStyle::PM_ToolBarItemSpacing) + 2;
        KdenliveSettings::setTrackheight(trackHeight);
    }
    if (KdenliveSettings::trackheight() == 0) {
        KdenliveSettings::setTrackheight(50);
    }

    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists() || KdenliveSettings::ffmpegpath().isEmpty() || KdenliveSettings::ffplaypath().isEmpty()) {
        // this is our first run, show Wizard
        QPointer<Wizard> w = new Wizard(false, this);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            delete w;
        } else {
            delete w;
            ::exit(1);
        }
    }
    initialGroup.writeEntry("version", version);
}

void MainWindow::slotRunWizard()
{
    QPointer<Wizard> w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::slotRefreshProfiles()
{
    KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
    if (d) {
        d->checkProfile();
    }
}

void MainWindow::slotEditProjectSettings()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    QPoint p = pCore->projectManager()->currentTimeline()->getTracksCount();

    QPointer<ProjectSettings> w = new ProjectSettings(project, project->metadata(), pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), p.x(), p.y(), project->projectFolder().path(), true, !project->isModified(), this);
    connect(w, SIGNAL(disableProxies()), this, SLOT(slotDisableProxies()));
    connect(w, SIGNAL(refreshProfiles()), this, SLOT(slotRefreshProfiles()));

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        project->setProjectFolder(w->selectedFolder());
        bool modified = false;
        if (m_recMonitor) {
            m_recMonitor->slotUpdateCaptureFolder(project->projectFolder().path() + QDir::separator());
        }
        if (m_renderWidget) {
            m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            slotSwitchAudioThumbs();
        }
        if (project->profilePath() != profile || project->profileChanged(profile)) {
            KdenliveSettings::setCurrent_profile(profile);
            pCore->projectManager()->slotResetProfiles();
            slotUpdateDocumentState(true);
        }
        if (project->getDocumentProperty(QStringLiteral("proxyparams")) != w->proxyParams()) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyparams"), w->proxyParams());
            if (pCore->binController()->clipCount() > 0 && KMessageBox::questionYesNo(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?")) == KMessageBox::Yes) {
                //TODO: rebuild all proxies
                //m_projectList->rebuildProxies();
            }
        }
        if (project->getDocumentProperty(QStringLiteral("proxyextension")) != w->proxyExtension()) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyextension"), w->proxyExtension());
        }
        if (project->getDocumentProperty(QStringLiteral("generateproxy")) != QString::number((int) w->generateProxy())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateproxy"), QString::number((int) w->generateProxy()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyminsize")) != QString::number(w->proxyMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyminsize"), QString::number(w->proxyMinSize()));
        }
        if (project->getDocumentProperty(QStringLiteral("generateimageproxy")) != QString::number((int) w->generateImageProxy())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateimageproxy"), QString::number((int) w->generateImageProxy()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyimageminsize")) != QString::number(w->proxyImageMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyimageminsize"), QString::number(w->proxyImageMinSize()));
        }
        if (QString::number((int) w->useProxy()) != project->getDocumentProperty(QStringLiteral("enableproxy"))) {
            project->setDocumentProperty(QStringLiteral("enableproxy"), QString::number((int) w->useProxy()));
            modified = true;
            slotUpdateProxySettings();
        }
        if (w->metadata() != project->metadata()) {
            project->setMetadata(w->metadata());
        }
        if (modified) project->setModified();
    }
    delete w;
}

void MainWindow::slotDisableProxies()
{
    pCore->projectManager()->current()->setDocumentProperty(QStringLiteral("enableproxy"), QString::number((int) false));
    pCore->projectManager()->current()->setModified();
    slotUpdateProxySettings();
}

void MainWindow::slotRenderProject()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (!m_renderWidget) {
        QString projectfolder = project ? project->projectFolder().path() + QDir::separator() : KdenliveSettings::defaultprojectfolder();
        MltVideoProfile profile;
        if (project) {
            profile = project->mltProfile();
            m_renderWidget = new RenderWidget(projectfolder, project->useProxy(), profile, this);
            connect(m_renderWidget, SIGNAL(shutdown()), this, SLOT(slotShutdown()));
            connect(m_renderWidget, SIGNAL(selectedRenderProfile(QMap<QString,QString>)), this, SLOT(slotSetDocumentRenderProfile(QMap<QString,QString>)));
            connect(m_renderWidget, SIGNAL(prepareRenderingData(bool,bool,QString)), this, SLOT(slotPrepareRendering(bool,bool,QString)));
            connect(m_renderWidget, SIGNAL(abortProcess(QString)), this, SIGNAL(abortRenderJob(QString)));
            connect(m_renderWidget, SIGNAL(openDvdWizard(QString)), this, SLOT(slotDvdWizard(QString)));
            m_renderWidget->setProfile(project->mltProfile());
            m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
            m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
            m_renderWidget->setRenderProfile(project->getRenderProperties());
        }
    }
    slotCheckRenderStatus();
    m_renderWidget->show();
    //m_renderWidget->showNormal();

    // What are the following lines supposed to do?
    //pCore->projectManager()->currentTimeline()->tracksNumber();
    //m_renderWidget->enableAudio(false);
    //m_renderWidget->export_audio;
}

void MainWindow::slotCheckRenderStatus()
{
    // Make sure there are no missing clips
    //TODO
    /*if (m_renderWidget)
        m_renderWidget->missingClips(pCore->bin()->hasMissingClips());*/
}

void MainWindow::setRenderingProgress(const QString &url, int progress)
{
    if (m_renderWidget)
        m_renderWidget->setRenderJob(url, progress);
}

void MainWindow::setRenderingFinished(const QString &url, int status, const QString &error)
{
    if (m_renderWidget)
        m_renderWidget->setRenderStatus(url, status, error);
}

void MainWindow::slotCleanProject()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will remove all unused clips from your project."), i18n("Clean up project")) == KMessageBox::Cancel) return;
    pCore->bin()->cleanup();
}

void MainWindow::slotUpdateMousePosition(int pos)
{
    if (pCore->projectManager()->current()) {
        switch (m_timeFormatButton->currentItem()) {
        case 0:
            m_timeFormatButton->setText(pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pos) + " / " + pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pCore->projectManager()->currentTimeline()->duration()));
            break;
        default:
            m_timeFormatButton->setText(QString::number(pos) + " / " + QString::number(pCore->projectManager()->currentTimeline()->duration()));
        }
    }
}

void MainWindow::slotUpdateProjectDuration(int pos)
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->setDuration(pos);
        slotUpdateMousePosition(pCore->projectManager()->currentTimeline()->projectView()->getMousePos());
    }
}

void MainWindow::slotUpdateDocumentState(bool modified)
{
    setWindowTitle(pCore->projectManager()->current()->description());
    setWindowModified(modified);
    m_saveAction->setEnabled(modified);
}

void MainWindow::connectDocument()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    Timeline *trackView = pCore->projectManager()->currentTimeline();
    connect(project, SIGNAL(startAutoSave()), pCore->projectManager(), SLOT(slotStartAutoSave()));
    connect(project, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    KdenliveSettings::setProject_fps(project->fps());
    m_clipMonitorDock->raise();
    m_effectStack->transitionConfig()->updateProjectFormat();

    connect(trackView, SIGNAL(configTrack()), this, SLOT(slotConfigTrack()));
    connect(trackView, SIGNAL(updateTracksInfo()), this, SLOT(slotUpdateTrackInfo()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(pCore->producerQueue(), SIGNAL(infoProcessingFinished()), trackView->projectView(), SLOT(slotInfoProcessingFinished()), Qt::DirectConnection);

    connect(trackView->projectView(), SIGNAL(importKeyframes(GraphicsRectItem,QString,QString)), this, SLOT(slotProcessImportKeyframes(GraphicsRectItem,QString,QString)));
    connect(m_projectMonitor, &Monitor::multitrackView, trackView, &Timeline::slotMultitrackView);
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));

    connect(project, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(trackView->projectView(), SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
    connect(project, SIGNAL(saveTimelinePreview(QString)), trackView, SLOT(slotSaveTimelinePreview(QString)));

    connect(trackView, SIGNAL(showTrackEffects(int,TrackInfo)), this, SLOT(slotTrackSelected(int,TrackInfo)));

    connect(trackView->projectView(), SIGNAL(clipItemSelected(ClipItem*,bool,bool)), this, SLOT(slotTimelineClipSelected(ClipItem*,bool,bool)), Qt::DirectConnection);
    connect(trackView->projectView(), &CustomTrackView::setActiveKeyframe, m_effectStack, &EffectStackView2::setActiveKeyframe);
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_effectStack, SLOT(slotTransitionItemSelected(Transition*,int,QPoint,bool)), Qt::DirectConnection);

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), this, SLOT(slotActivateTransitionView(Transition*)));
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(trackView, SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));
    connect(trackView->projectView(), SIGNAL(displayMessage(QString,MessageType)), m_messageLabel, SLOT(setMessage(QString,MessageType)));
    connect(pCore->bin(), SIGNAL(clipNameChanged(QString)), trackView->projectView(), SLOT(clipNameChanged(QString)));    
    connect(pCore->bin(), SIGNAL(displayMessage(QString,MessageType)), m_messageLabel, SLOT(setMessage(QString,MessageType)));

    connect(trackView->projectView(), SIGNAL(showClipFrame(const QString&,int)), pCore->bin(), SLOT(selectClipById(const QString&,int)));
    connect(trackView->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));
    connect(m_projectMonitor, &Monitor::addEffect, trackView->projectView(), &CustomTrackView::slotAddEffectToCurrentItem);

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_projectMonitor, SLOT(slotSetSelectedClip(Transition*)));

    connect(pCore->bin(), SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), trackView->projectView(), SLOT(slotGotFilterJobResults(QString,int,int,stringMap,stringMap)));

    //TODO
    //connect(m_projectList, SIGNAL(addMarkers(QString,QList<CommentedTime>)), trackView->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));

    // Effect stack signals
    connect(m_effectStack, SIGNAL(updateEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)));
    connect(m_effectStack, SIGNAL(updateClipRegion(ClipItem*,int,QString)), trackView->projectView(), SLOT(slotUpdateClipRegion(ClipItem*,int,QString)));
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem*,int,QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*,int,QDomElement)));
    connect(m_effectStack, SIGNAL(removeEffectGroup(ClipItem*,int,QDomDocument)), trackView->projectView(), SLOT(slotDeleteEffectGroup(ClipItem*,int,QDomDocument)));

    connect(m_effectStack, SIGNAL(addEffect(ClipItem*,QDomElement,int)), trackView->projectView(), SLOT(slotAddEffect(ClipItem*,QDomElement,int)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem*,int,QList<int>,bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*,int,QList<int>,bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*,int,QList<int>,int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*,int,QList<int>,int)));

    connect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(m_effectStack, SIGNAL(seekTimeline(int)), trackView->projectView(), SLOT(seekCursorPos(int)));
    connect(m_effectStack, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString,QString>)), trackView->projectView(), SLOT(slotImportClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString,QString>)));

    // Transition config signals
    connect(m_effectStack->transitionConfig(), SIGNAL(transitionUpdated(Transition*,QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition*,QDomElement)));
    connect(m_effectStack->transitionConfig(), SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(seekCursorPos(int)));

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()));
    connect(project, &KdenliveDoc::updateFps, trackView, &Timeline::updateProfile, Qt::DirectConnection);
    connect(trackView, SIGNAL(zoneMoved(int,int)), this, SLOT(slotZoneMoved(int,int)));
    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, static_cast<QMenu*>(factory()->container(QStringLiteral("marker_menu"), this)));
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(project->mltProfile());
        m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
        m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
        m_renderWidget->setRenderProfile(project->getRenderProperties());
    }
    m_zoomSlider->setValue(project->zoom().x());
    m_commandStack->setActiveStack(project->commandStack());
    KdenliveSettings::setProject_display_ratio(project->dar());

    setWindowTitle(project->description());
    setWindowModified(project->isModified());
    m_saveAction->setEnabled(project->isModified());
    m_normalEditTool->setChecked(true);
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), this, SLOT(slotUpdateProjectDuration(int)));
    pCore->monitorManager()->setDocument(project);
    trackView->updateProfile(false);
    if (m_recMonitor) {
        m_recMonitor->slotUpdateCaptureFolder(project->projectFolder().path() + QDir::separator());
    }
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);

    // Update guides info in render widget
    slotGuidesUpdated();

    // Make sure monitor is visible so that it is painted black on startup
    //show();
    //pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor, true);
    // set tool to select tool
    m_buttonSelectTool->setChecked(true);
    connect(m_projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(slotRefreshMonitor(bool)), Qt::UniqueConnection);
    connect(m_clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(slotRefreshMonitor(bool)), Qt::UniqueConnection);
}

void MainWindow::slotZoneMoved(int start, int end)
{
    pCore->projectManager()->current()->setZone(start, end);
    m_projectMonitor->slotZoneMoved(start, end);
}

void MainWindow::slotGuidesUpdated()
{
    QMap <double, QString> guidesData = pCore->projectManager()->currentTimeline()->projectView()->guidesData();
    if (m_renderWidget)
        m_renderWidget->setGuides(guidesData, pCore->projectManager()->current()->projectDuration());
    m_projectMonitor->setGuides(guidesData);
}

void MainWindow::slotEditKeys()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);
    dialog.addCollection(actionCollection(), i18nc("general keyboard shortcuts", "General"));
    dialog.configure();
}

void MainWindow::slotPreferences(int page, int option)
{
    /*
     * An instance of your dialog could be already created and could be
     * cached, in which case you want to display the cached dialog
     * instead of creating another one
     */
    if (m_stopmotion) {
        m_stopmotion->slotLive(false);
    }

    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists(QStringLiteral("settings")));
        if (page != -1) d->showPage(page, option);
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :

    // Get the mappable actions in localized form
    QMap<QString, QString> actions;
    KActionCollection* collection = actionCollection();
    QRegExp ampEx("&{1,1}");
    foreach (const QString& action_name, m_actionNames) {
	QString action_text = collection->action(action_name)->text();
	action_text.remove(ampEx);
        actions[action_text] = action_name;
    }

    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(actions, m_gpuAllowed, this);
    connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(updateConfiguration()));
    connect(dialog, SIGNAL(settingsChanged(QString)), SIGNAL(configurationChanged()));
    connect(dialog, SIGNAL(doResetProfile()), pCore->projectManager(), SLOT(slotResetProfiles()));
    connect(dialog, SIGNAL(checkTabPosition()), this, SLOT(slotCheckTabPosition()));
    connect(dialog, SIGNAL(restartKdenlive()), this, SLOT(slotRestart()));
    connect(dialog, SIGNAL(updateLibraryFolder()), pCore, SIGNAL(updateLibraryPath()));

    if (m_recMonitor) {
        connect(dialog, SIGNAL(updateCaptureFolder()), this, SLOT(slotUpdateCaptureFolder()));
        connect(dialog, SIGNAL(updateFullScreenGrab()), m_recMonitor, SLOT(slotUpdateFullScreenGrab()));
    }
    dialog->show();
    if (page != -1) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotCheckTabPosition()
{
    QTabWidget::TabPosition pos = tabPosition(Qt::LeftDockWidgetArea);
    bool reload = false;
    if (KdenliveSettings::verticaltabs() && pos != QTabWidget::East) {
        reload = true;
    } else if (!KdenliveSettings::verticaltabs() && pos != QTabWidget::North) {
        reload = true;
    }
    if (reload)
        setTabPosition(Qt::AllDockWidgetAreas, KdenliveSettings::verticaltabs() ? QTabWidget::East : QTabWidget::North);
}

void MainWindow::slotRestart()
{
    m_exitCode = EXIT_RESTART;
    QApplication::closeAllWindows();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    KXmlGuiWindow::closeEvent(event);
    if (event->isAccepted()) {
        QApplication::exit(m_exitCode);
        return;
    }
}

void MainWindow::slotUpdateCaptureFolder()
{
    if (m_recMonitor) {
        if (pCore->projectManager()->current())
            m_recMonitor->slotUpdateCaptureFolder(pCore->projectManager()->current()->projectFolder().path() + QDir::separator());
        else
            m_recMonitor->slotUpdateCaptureFolder(KdenliveSettings::defaultprojectfolder());
    }
}

void MainWindow::updateConfiguration()
{
    //TODO: we should apply settings to all projects, not only the current one
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
	    pCore->projectManager()->currentTimeline()->projectView()->checkAutoScroll();
        pCore->projectManager()->currentTimeline()->checkTrackHeight();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    slotSwitchSplitAudio(KdenliveSettings::splitaudio());

    // Update list of transcoding profiles
    buildDynamicActions();
    loadClipActions();
}

void MainWindow::slotSwitchSplitAudio(bool enable)
{
    KdenliveSettings::setSplitaudio(enable);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->updateHeaders();
    }
}

void MainWindow::slotSwitchVideoThumbs()
{
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotUpdateAllThumbs();
    }
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    pCore->binController()->checkAudioThumbs();
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
        pCore->projectManager()->currentTimeline()->projectView()->checkAutoScroll();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
    }
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
}


void MainWindow::slotDeleteItem()
{
    if (QApplication::focusWidget() &&
            QApplication::focusWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget() == pCore->bin()) {
        pCore->bin()->slotDeleteClip();

    } else {
        QWidget *widget = QApplication::focusWidget();
        while (widget && widget != this) {
            if (widget == m_effectStackDock) {
                m_effectStack->deleteCurrentEffect();
                return;
            }
            widget = widget->parentWidget();
        }

        // effect stack has no focus
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->deleteSelectedClips();
        }
    }
}


void MainWindow::slotAddClipMarker()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    ClipController *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = GenTime((int)((m_projectMonitor->position() - item->startPos() + item->cropStart()).frames(project->fps()) * item->speed() + 0.5), project->fps());
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
        return;
    }
    QString id = clip->clipId();
    CommentedTime marker(pos, i18n("Marker"), KdenliveSettings::default_marker_type());
    QPointer<MarkerDialog> d = new MarkerDialog(clip, marker,
                                                project->timecode(), i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() << d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) project->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(project->fps())), d->markerImage());
    }
    delete d;
}

void MainWindow::slotDeleteClipMarker(bool allowGuideDeletion)
{
    ClipController *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->clipId();
    QString comment = clip->markerComment(pos);
    if (comment.isEmpty()) {
        if (allowGuideDeletion && m_projectMonitor->isActive()) {
            slotDeleteGuide();
        }
        else m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }
    pCore->bin()->deleteClipMarker(comment, id, pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    ClipController *clip = NULL;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }
    pCore->bin()->deleteAllClipMarkers(clip->clipId());
}

void MainWindow::slotEditClipMarker()
{
    ClipController *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->clipId();
    CommentedTime oldMarker = clip->markerAt(pos);
    if (oldMarker == CommentedTime()) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    QPointer<MarkerDialog> d = new MarkerDialog(clip, oldMarker,
                                                pCore->projectManager()->current()->timecode(), i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() <<d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) pCore->projectManager()->current()->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(pCore->projectManager()->current()->fps())), d->markerImage());
        if (d->newMarker().time() != pos) {
            // remove old marker
            oldMarker.setMarkerType(-1);
            pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() <<oldMarker);
        }
    }
    delete d;
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!pCore->projectManager()->currentTimeline() || !pCore->projectManager()->current())
        return;

    if (m_clipMonitor->isActive()) {
        ClipController *clip = m_clipMonitor->currentController();
        GenTime pos = m_clipMonitor->position();

        if (!clip) {
            m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
            return;
        }
        //TODO: allow user to set default marker category
        CommentedTime marker(pos, pCore->projectManager()->current()->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
        pCore->bin()->slotAddClipMarker(clip->clipId(), QList <CommentedTime>() <<marker);
    } else {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide(false);
    }
}

void MainWindow::slotAddGuide()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide();
}

void MainWindow::slotInsertSpace()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertSpace();
}

void MainWindow::slotRemoveSpace()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotRemoveSpace();
}

void MainWindow::slotInsertTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertTrack(ix );
    }
    if (pCore->projectManager()->current()) {
        m_effectStack->transitionConfig()->updateProjectFormat();
    }
}

void MainWindow::slotDeleteTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_effectStack->transitionConfig()->updateProjectFormat();
    }
}

void MainWindow::slotConfigTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotConfigTracks(ix);
    }
    if (pCore->projectManager()->current())
        m_effectStack->transitionConfig()->updateProjectFormat();
}

void MainWindow::slotSelectTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotSelectClipsInTrack();
    }
}

void MainWindow::slotSelectAllTracks()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotSelectAllClips();
}

void MainWindow::slotEditGuide(int pos, QString text)
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotEditGuide(pos, text);
}

void MainWindow::slotDeleteGuide()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteGuide();
}

void MainWindow::slotDeleteAllGuides()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteAllGuides();
}

void MainWindow::slotCutTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->cutSelectedClips();
}

void MainWindow::slotInsertClipOverwrite()
{
    if (pCore->projectManager()->currentTimeline()) {
        QPoint binZone = m_clipMonitor->getZoneInfo();
        pCore->projectManager()->currentTimeline()->projectView()->insertZone(TimelineMode::OverwriteEdit, m_clipMonitor->activeClipId(), binZone);
    }
}

void MainWindow::slotInsertClipInsert()
{
    if (pCore->projectManager()->currentTimeline()) {
        QPoint binZone = m_clipMonitor->getZoneInfo();
        pCore->projectManager()->currentTimeline()->projectView()->insertZone(TimelineMode::InsertEdit, m_clipMonitor->activeClipId(), binZone);
    }
}

void MainWindow::slotExtractZone()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->extractZone(QPoint(), true);
    }
}

void MainWindow::slotLiftZone()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->extractZone(QPoint(),false);
    }
}

void MainWindow::slotPreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->startPreviewRender();
    }
}

void MainWindow::slotDefinePreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->addPreviewRange(true);
    }
}

void MainWindow::slotRemovePreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->addPreviewRange(false);
    }
}

void MainWindow::slotSelectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true);
}

void MainWindow::slotSelectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true);
}

void MainWindow::slotDeselectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(false, true);
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(false, true);
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true, true);
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true, true);
}

void MainWindow::slotGroupClips()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->groupClips();
}

void MainWindow::slotUnGroupClips()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->groupClips(false);
}

void MainWindow::slotEditItemDuration()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->editItemDuration();
}

void MainWindow::slotAddProjectClip(const QUrl &url)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << url);
}

void MainWindow::slotAddProjectClipList(const QList<QUrl> &urls)
{
    pCore->bin()->droppedUrls(urls);
}

void MainWindow::slotAddTransition(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty() || info.count() < 2) return;
    QDomElement transition = transitions.getEffectByTag(info.at(0), info.at(1));
    if (pCore->projectManager()->currentTimeline() && !transition.isNull()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
    }
}

void MainWindow::slotAddVideoEffect(QAction *result)
{
    if (!result) {
        return;
    }
    const int VideoEffect = 1;
    const int AudioEffect = 2;
    QStringList info = result->data().toStringList();

    if (info.isEmpty() || info.size() < 3) {
        return;
    }
    QDomElement effect ;
    if (info.last() == QString::number((int) VideoEffect)) {
        effect = videoEffects.getEffectByTag(info.at(0), info.at(1));
    } else if (info.last() == QString::number((int) AudioEffect)) {
        effect = audioEffects.getEffectByTag(info.at(0), info.at(1));
    } else {
        effect = customEffects.getEffectByTag(info.at(0), info.at(1));
    }

    if (!effect.isNull()) {
        slotAddEffect(effect);
    } else {
        m_messageLabel->setMessage(i18n("Cannot find effect %1 / %2", info.at(0), info.at(1)), ErrorMessage);
    }
}


void MainWindow::slotZoomIn()
{
    m_zoomSlider->setValue(m_zoomSlider->value() - 1);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotZoomOut()
{
    m_zoomSlider->setValue(m_zoomSlider->value() + 1);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotFitZoom()
{
    if (pCore->projectManager()->currentTimeline()) {
        m_zoomSlider->setValue(pCore->projectManager()->currentTimeline()->fitZoom());
	// Make sure to reset scroll bar to start
	pCore->projectManager()->currentTimeline()->projectView()->scrollToStart();
    }
}

void MainWindow::slotSetZoom(int value)
{
    value = qMax(m_zoomSlider->minimum(), value);
    value = qMin(m_zoomSlider->maximum(), value);

    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->slotChangeZoom(value);
    }

    m_zoomOut->setEnabled(value < m_zoomSlider->maximum());
    m_zoomIn->setEnabled(value > m_zoomSlider->minimum());
    slotUpdateZoomSliderToolTip(value);

    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(value);
    m_zoomSlider->blockSignals(false);
}

void MainWindow::slotShowZoomSliderToolTip(int zoomlevel)
{
    if (zoomlevel != -1) {
        slotUpdateZoomSliderToolTip(zoomlevel);
    }

    QPoint global = m_zoomSlider->rect().topLeft();
    global.ry() += m_zoomSlider->height() / 2;
    QHelpEvent toolTipEvent(QEvent::ToolTip, QPoint(0, 0), m_zoomSlider->mapToGlobal(global));
    QApplication::sendEvent(m_zoomSlider, &toolTipEvent);
}

void MainWindow::slotUpdateZoomSliderToolTip(int zoomlevel)
{
    m_zoomSlider->setToolTip(i18n("Zoom Level: %1/13", (13 - zoomlevel)));
}

void MainWindow::slotGotProgressInfo(const QString &message, int progress, MessageType type)
{
    if (type == DefaultMessage) {
        m_statusProgressBar->setValue(progress);
    }
    m_messageLabel->setMessage(progress < 100 ? message : QString(), type);
    if (progress >= 0) {
        if (type == DefaultMessage) {
            m_statusProgressBar->setVisible(progress < 100);
        }
    } else {
        m_statusProgressBar->setVisible(false);
    }
}

void MainWindow::customEvent(QEvent* e)
{
    if (e->type() == QEvent::User)
        m_messageLabel->setMessage(static_cast <MltErrorEvent *>(e)->message(), MltError);
}

void MainWindow::slotTimelineClipSelected(ClipItem* item, bool reloadStack, bool raise)
{
    m_effectStack->slotClipItemSelected(item, m_projectMonitor, reloadStack);
    m_projectMonitor->slotSetSelectedClip(item);
    if (raise) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotTrackSelected(int index, const TrackInfo &info, bool raise)
{
    m_effectStack->slotTrackItemSelected(index, info, m_projectMonitor);
    if (raise) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotActivateTransitionView(Transition *transition)
{
    if (transition)
        m_effectStack->raiseWindow(m_effectStackDock);
}

void MainWindow::slotSnapRewind()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->slotSeekToPreviousSnap();
        }
    } else  {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->slotSeekToNextSnap();
        }
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->clipStart();
        }
    }
}

void MainWindow::slotClipEnd()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->clipEnd();
        }
    }
}

void MainWindow::slotChangeTool(QAction * action)
{
    if (action == m_buttonSelectTool)
        slotSetTool(SelectTool);
    else if (action == m_buttonRazorTool)
        slotSetTool(RazorTool);
    else if (action == m_buttonSpacerTool)
        slotSetTool(SpacerTool);
}

void MainWindow::slotChangeEdit(QAction * action)
{
    if (!pCore->projectManager()->currentTimeline())
        return;

    if (action == m_overwriteEditTool)
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::OverwriteEdit);
    else if (action == m_insertEditTool)
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::InsertEdit);
    else
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::NormalEdit);
}

void MainWindow::slotSetTool(ProjectTool tool)
{
    if (pCore->projectManager()->current() && pCore->projectManager()->currentTimeline()) {
        //pCore->projectManager()->current()->setTool(tool);
        QString message;
        switch (tool)  {
        case SpacerTool:
            message = i18n("Ctrl + click to use spacer on current track only");
            break;
        case RazorTool:
            message = i18n("Click on a clip to cut it, Shift + move to preview cut frame");
            break;
        default:
            message = i18n("Shift + click to create a selection rectangle, Ctrl + click to add an item to selection");
            break;
        }
        m_messageLabel->setMessage(message, InformationMessage);
        pCore->projectManager()->currentTimeline()->projectView()->setTool(tool);
    }
}

void MainWindow::slotCopy()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->copyClip();
}

void MainWindow::slotPaste()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->pasteClipEffects();
}

void MainWindow::slotClipInTimeline(const QString &clipId)
{
    if (pCore->projectManager()->currentTimeline()) {
        QList<ItemInfo> matching = pCore->projectManager()->currentTimeline()->projectView()->findId(clipId);
        QMenu *inTimelineMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("clip_in_timeline"), this));

        QList <QAction *> actionList;
        for (int i = 0; i < matching.count(); ++i) {
            QString track = pCore->projectManager()->currentTimeline()->getTrackInfo(matching.at(i).track).trackName;
            QString start = pCore->projectManager()->current()->timecode().getTimecode(matching.at(i).startPos);
            int j = 0;
            QAction *a = new QAction(track + ": " + start, inTimelineMenu);
            a->setData(QStringList() << track << start);
            connect(a, SIGNAL(triggered()), this, SLOT(slotSelectClipInTimeline()));
            while (j < actionList.count()) {
                if (actionList.at(j)->text() > a->text()) break;
                j++;
            }
            actionList.insert(j, a);
        }
	QList <QAction*> list = inTimelineMenu->actions();
	unplugActionList("timeline_occurences");
	qDeleteAll(list);
	plugActionList("timeline_occurences", actionList);

        if (actionList.isEmpty()) {
            inTimelineMenu->setEnabled(false);
        } else {
            inTimelineMenu->setEnabled(true);
        }
    }
}

void MainWindow::slotClipInProjectTree()
{
    if (pCore->projectManager()->currentTimeline()) {
        int pos = -1;
        QPoint zone;
        const QString selectedId = pCore->projectManager()->currentTimeline()->projectView()->getClipUnderCursor(&pos, &zone);
        if (selectedId.isEmpty()) {
            return;
        }
        m_projectBinDock->raise();
        pCore->bin()->selectClipById(selectedId, pos, zone);
        if (m_projectMonitor->isActive()) {
            slotSwitchMonitors();
        }
    }
}

void MainWindow::slotSelectClipInTimeline()
{
    if (pCore->projectManager()->currentTimeline()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        pCore->projectManager()->currentTimeline()->projectView()->selectFound(data.at(0), data.at(1));
    }
}

/** Gets called when the window gets hidden */
void MainWindow::hideEvent(QHideEvent */*event*/)
{
    if (isMinimized() && pCore->monitorManager())
        pCore->monitorManager()->pauseActiveMonitor();
}

/*void MainWindow::slotSaveZone(Render *render, const QPoint &zone, DocClipBase *baseClip, QUrl path)
{
    QPointer<QDialog> dialog = new QDialog(this);
    dialog->setWindowTitle("Save clip zone");
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);
    
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    dialog->connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    dialog->connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));

    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    if (path.isEmpty()) {
        QString tmppath = pCore->projectManager()->current()->projectFolder().path() + QDir::separator();
        if (baseClip == NULL) {
            tmppath.append("untitled.mlt");
        } else {
            tmppath.append((baseClip->name().isEmpty() ? baseClip->fileURL().fileName() : baseClip->name()) + '-' + QString::number(zone.x()).rightJustified(4, '0') + ".mlt");
        }
        path = QUrl(tmppath);
    }
    KUrlRequester *url = new KUrlRequester(path, this);
    url->setFilter("video/mlt-playlist");
    QLabel *label2 = new QLabel(i18n("Description:"), this);
    QLineEdit *edit = new QLineEdit(this);
    mainLayout->addWidget(label1);
    mainLayout->addWidget(url);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(edit);    
    mainLayout->addWidget(buttonBox);

    if (dialog->exec() == QDialog::Accepted) {
        if (QFile::exists(url->url().path())) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", url->url().path())) == KMessageBox::No) {
                slotSaveZone(render, zone, baseClip, url->url());
                delete dialog;
                return;
            }
        }
        if (baseClip && !baseClip->fileURL().isEmpty()) {
            // create zone from clip url, so that we don't have problems with proxy clips
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.remove("MLT_PROFILE");
            p.setProcessEnvironment(env);
            p.start(KdenliveSettings::rendererpath(), QStringList() << baseClip->fileURL().toLocalFile() << "in=" + QString::number(zone.x()) << "out=" + QString::number(zone.y()) << "-consumer" << "xml:" + url->url().path());
            if (!p.waitForStarted(3000)) {
                KMessageBox::sorry(this, i18n("Cannot start MLT's renderer:\n%1", KdenliveSettings::rendererpath()));
            }
            else if (!p.waitForFinished(5000)) {
                KMessageBox::sorry(this, i18n("Timeout while creating xml output"));
            }
        }
        else render->saveZone(url->url(), edit->text(), zone);
    }
    delete dialog;
}*/

void MainWindow::slotResizeItemStart()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->setInPoint();
}

void MainWindow::slotResizeItemEnd()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->setOutPoint();
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec()) entries = dialog->changedEntries();
    foreach(const KNS3::Entry & entry, entries) {
        if (entry.status() == KNS3::Entry::Installed)
            qDebug() << "// Installed files: " << entry.installedFiles();
    }
    delete dialog;
    return entries.size();
}

void MainWindow::slotGetNewTitleStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_titles.knsrc")) > 0) {
        // get project title path
        QString titlePath = pCore->projectManager()->current()->projectFolder().path();
        titlePath.append(QStringLiteral("/titles/"));
        TitleWidget::refreshTitleTemplates(titlePath);
    }
}

void MainWindow::slotGetNewLumaStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_wipes.knsrc")) > 0) {
        initEffects::refreshLumas();
        pCore->projectManager()->currentTimeline()->projectView()->reloadTransitionLumas();
    }
}

void MainWindow::slotGetNewRenderStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_renderprofiles.knsrc")) > 0)
        if (m_renderWidget)
            m_renderWidget->reloadProfiles();
}

void MainWindow::slotGetNewMltProfileStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_projectprofiles.knsrc")) > 0) {
        // update the list of profiles in settings dialog
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists(QStringLiteral("settings")));
        if (d)
            d->checkProfile();
    }
}

void MainWindow::slotAutoTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->autoTransition();
}

void MainWindow::slotSplitAudio()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->splitAudio();
}

void MainWindow::slotSetAudioAlignReference()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->setAudioAlignReference();
    }
}

void MainWindow::slotAlignAudio()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->alignAudio();
    }
}

void MainWindow::slotUpdateClipType(QAction *action)
{
    if (pCore->projectManager()->currentTimeline()) {
        PlaylistState::ClipState state = (PlaylistState::ClipState) action->data().toInt();
        pCore->projectManager()->currentTimeline()->projectView()->setClipType(state);
    }
}

void MainWindow::slotDvdWizard(const QString &url)
{
    // We must stop the monitors since we create a new on in the dvd wizard
    QPointer<DvdWizard> w = new DvdWizard(pCore->monitorManager(), url, this);
    w->exec();
    delete w;
    pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor);
}

void MainWindow::slotShowTimeline(bool show)
{
    if (show == false) {
        m_timelineState = saveState();
        centralWidget()->setHidden(true);
    } else {
        centralWidget()->setHidden(false);
        restoreState(m_timelineState);
    }
}

void MainWindow::loadClipActions()
{
    unplugActionList(QStringLiteral("add_effect"));
    plugActionList(QStringLiteral("add_effect"), m_effectsMenu->actions());


    QList <QAction *>clipJobActions = getExtraActions("clipjobs");
    unplugActionList("clip_jobs");
    plugActionList("clip_jobs", clipJobActions);

    QList <QAction *>atcActions = getExtraActions(QStringLiteral("audiotranscoderslist"));
    unplugActionList(QStringLiteral("audio_transcoders_list"));
    plugActionList(QStringLiteral("audio_transcoders_list"), atcActions);

    QList <QAction *>tcActions = getExtraActions(QStringLiteral("transcoderslist"));
    unplugActionList(QStringLiteral("transcoders_list"));
    plugActionList(QStringLiteral("transcoders_list"), tcActions);
}

void MainWindow::loadDockActions()
{
    QList <QAction *>list = kdenliveCategoryMap.value(QStringLiteral("interface"))->actions();
    // Sort actions
    QMap <QString, QAction*> sorted;
    QStringList sortedList;
    foreach(QAction *a, list) {
        sorted.insert(a->text(), a);
        sortedList << a->text();
    }
    QList <QAction *>orderedList;
    sortedList.sort(Qt::CaseInsensitive);
    foreach(const QString &text, sortedList) {
        orderedList << sorted.value(text);
    }    
    unplugActionList( "dock_actions" );
    plugActionList( "dock_actions", orderedList);
}

void MainWindow::buildDynamicActions()
{
    KActionCategory *ts = NULL;
    if (kdenliveCategoryMap.contains(QStringLiteral("clipjobs"))) {
	ts = kdenliveCategoryMap.take(QStringLiteral("clipjobs"));
	delete ts;
    }
    ts = new KActionCategory(i18n("Clip Jobs"), m_extraFactory->actionCollection());

    Mlt::Profile profile;
    Mlt::Filter *filter;

    foreach(const QString &stab, QStringList() << "vidstab" << "videostab2" << "videostab") {
	filter = Mlt::Factory::filter(profile, (char*)stab.toUtf8().constData());
        if (filter && filter->is_valid()) {
	    QAction *action = new QAction(i18n("Stabilize") + " (" + stab + ")", m_extraFactory->actionCollection());
            action->setData(QStringList() << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << stab);
	    ts->addAction(action->text(), action);
            connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
            delete filter;
            break;
        }
        delete filter;
    }
    filter = Mlt::Factory::filter(profile,(char*)"motion_est");
    if (filter) {
	if (filter->is_valid()) {
	    QAction *action = new QAction(i18n("Automatic scene split"), m_extraFactory->actionCollection());
            QStringList stabJob;
            stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << QStringLiteral("motion_est");
            action->setData(stabJob);
	    ts->addAction(action->text(), action);
            connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
        }
        delete filter;
    }
    if (KdenliveSettings::producerslist().contains(QStringLiteral("timewarp"))) {
	QAction *action = new QAction(i18n("Reverse clip"), m_extraFactory->actionCollection());
        QStringList stabJob;
        stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << QStringLiteral("timewarp");
        action->setData(stabJob);
	ts->addAction(action->text(), action);
        connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
    }
    QAction *action = new QAction(i18n("Analyse keyframes"), m_extraFactory->actionCollection());
    QStringList stabJob(QString::number((int) AbstractClipJob::ANALYSECLIPJOB));
    action->setData(stabJob);
    ts->addAction(action->text(), action);
    connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
    kdenliveCategoryMap.insert(QStringLiteral("clipjobs"), ts);

    if (kdenliveCategoryMap.contains(QStringLiteral("transcoderslist"))) {
	ts = kdenliveCategoryMap.take(QStringLiteral("transcoderslist"));
	delete ts;
    }
    if (kdenliveCategoryMap.contains(QStringLiteral("audiotranscoderslist"))) {
	ts = kdenliveCategoryMap.take(QStringLiteral("audiotranscoderslist"));
	delete ts;
    }
    ts = new KActionCategory(i18n("Transcoders"), m_extraFactory->actionCollection());
    KActionCategory *ats = new KActionCategory(i18n("Extract Audio"), m_extraFactory->actionCollection());
    KSharedConfigPtr config = KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList data;
        data << QString::number((int) AbstractClipJob::TRANSCODEJOB);
        data << i.value().split(';');
        QAction *a = new QAction(i.key(), m_extraFactory->actionCollection());
        a->setData(data);
        if (data.count() > 1) a->setToolTip(data.at(1));
        // slottranscode
        connect(a, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
	if (data.count() > 3 && data.at(3) == "audio") {
	    // This is an audio transcoding action
	    ats->addAction(i.key(), a);
	} else {
	    ts->addAction(i.key(), a);
	}
    }
    kdenliveCategoryMap.insert(QStringLiteral("transcoderslist"), ts);
    kdenliveCategoryMap.insert(QStringLiteral("audiotranscoderslist"), ats);

    // Populate View menu with show / hide actions for dock widgets
    KActionCategory *guiActions = NULL;
    if (kdenliveCategoryMap.contains(QStringLiteral("interface"))) {
	guiActions = kdenliveCategoryMap.take(QStringLiteral("interface"));
	delete guiActions;
    }
    guiActions = new KActionCategory(i18n("Interface"), actionCollection());
    QAction *showTimeline = new QAction(i18n("Timeline"), this);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, SIGNAL(triggered(bool)), this, SLOT(slotShowTimeline(bool)));
    guiActions->addAction(showTimeline->text(), showTimeline);
    actionCollection()->addAction(showTimeline->text(), showTimeline);

    QList <QDockWidget *> docks = findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget* dock = docks.at(i);
        QAction * dockInformations = dock->toggleViewAction();
        if (!dockInformations) continue;
        dockInformations->setChecked(!dock->isHidden());
        guiActions->addAction(dockInformations->text(), dockInformations);
    }
    kdenliveCategoryMap.insert(QStringLiteral("interface"), guiActions);
}

QList <QAction *> MainWindow::getExtraActions(const QString &name)
{
    if (!kdenliveCategoryMap.contains(name)) return QList <QAction *> ();
    return kdenliveCategoryMap.value(name)->actions();
}

void MainWindow::slotTranscode(const QStringList &urls)
{
    QString params;
    QString desc;
    QString condition;
    if (urls.isEmpty()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        pCore->bin()->startClipJob(data);
        return;
    }
    if (urls.isEmpty()) {
        m_messageLabel->setMessage(i18n("No clip to transcode"), ErrorMessage);
        return;
    }
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc);
    connect(d, SIGNAL(addClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    d->show();
}

void MainWindow::slotTranscodeClip()
{
    QString allExtensions = ClipCreationDialog::getExtensions().join(QStringLiteral(" "));
    const QString dialogFilter =  i18n("All Supported Files") + '(' + allExtensions + ");;" + i18n("All Files") + "(*)";
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QStringList urls = QFileDialog::getOpenFileNames(this, i18n("Files to transcode"), clipFolder, dialogFilter);
    if (urls.isEmpty()) return;
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(const QMap <QString, QString> &props)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    bool modified = false;
    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
	if (project->getDocumentProperty(i.key()) == i.value())
	    continue;
	project->setDocumentProperty(i.key(), i.value());
	modified = true;
    }
    if (modified) project->setModified();
}


void MainWindow::slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (m_renderWidget == NULL) return;
    QString scriptPath;
    QString playlistPath;
    QString mltSuffix(QStringLiteral(".mlt"));
    QList<QString> playlistPaths;
    QList<QString> trackNames;
    int tracksCount = 1;
    bool stemExport = m_renderWidget->isStemAudioExportEnabled();

    if (scriptExport) {
        //QString scriptsFolder = project->projectFolder().path(QUrl::AddTrailingSlash) + "scripts/";
        QString path = m_renderWidget->getFreeScriptName(project->url());
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl::fromLocalFile(path), i18n("Create Render Script"), this);
        getUrl->urlRequester()->setMode(KFile::File);
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            return;
        }
        scriptPath = getUrl->selectedUrl().path();
        delete getUrl;
        QFile f(scriptPath);
        if (f.exists()) {
            if (KMessageBox::warningYesNo(this, i18n("Script file already exists. Do you want to overwrite it?")) != KMessageBox::Yes)
                return;
        }
        playlistPath = scriptPath;
    } else {
        QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/kdenlive_rendering_XXXXXX.mlt"));
        temp.setAutoRemove(false);
        temp.open();
        playlistPath = temp.fileName();
    }

    QString playlistContent = pCore->projectManager()->projectSceneList();
    if (!chapterFile.isEmpty()) {
        int in = 0;
        int out;
        if (!zoneOnly) out = (int) GenTime(project->projectDuration()).frames(project->fps());
        else {
            in = pCore->projectManager()->currentTimeline()->inPoint();
            out = pCore->projectManager()->currentTimeline()->outPoint();
        }
        QDomDocument doc;
        QDomElement chapters = doc.createElement(QStringLiteral("chapters"));
        chapters.setAttribute(QStringLiteral("fps"), project->fps());
        doc.appendChild(chapters);

        QMap <double, QString> guidesData = pCore->projectManager()->currentTimeline()->projectView()->guidesData();
        QMapIterator<double, QString> g(guidesData);
        QLocale locale;
        while (g.hasNext()) {
            g.next();
            int time = (int) GenTime(g.key()).frames(project->fps());
            if (time >= in && time < out) {
                if (zoneOnly) time = time - in;
                QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
                chapters.appendChild(chapter);
                chapter.setAttribute(QStringLiteral("title"), g.value());
                chapter.setAttribute(QStringLiteral("time"), time);
            }
        }

        if (chapters.childNodes().count() > 0) {
            if (pCore->projectManager()->currentTimeline()->projectView()->hasGuide(out, 0) == -1) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute(QStringLiteral("title"), i18nc("the first in a list of chapters", "Start"));
                chapter.setAttribute(QStringLiteral("time"), QStringLiteral("0"));
            }
            // save chapters file
            QFile file(chapterFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
            } else {
                file.write(doc.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    qWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
                }
                file.close();
            }
        }
    }

    // check if audio export is selected
    bool exportAudio;
    if (m_renderWidget->automaticAudioExport()) {
        exportAudio = pCore->projectManager()->currentTimeline()->checkProjectAudio();
    } else {
        exportAudio = m_renderWidget->selectedAudioExport();
    }

    // Set playlist audio volume to 100%
    QDomDocument doc;
    doc.setContent(playlistContent);
    QDomElement tractor = doc.documentElement().firstChildElement(QStringLiteral("tractor"));
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName(QStringLiteral("property"));
        for (int i = 0; i < props.count(); ++i) {
            if (props.at(i).toElement().attribute(QStringLiteral("name")) == QLatin1String("meta.volume")) {
                props.at(i).firstChild().setNodeValue(QStringLiteral("1"));
                break;
            }
        }
    }

    // Add autoclose to playlists.
    QDomNodeList playlists = doc.elementsByTagName("playlist");
    for (int i = 0; i < playlists.length();++i) {
        playlists.item(i).toElement().setAttribute("autoclose", 1);
    }

    // Do we want proxy rendering
    if (project->useProxy() && !m_renderWidget->proxyRendering()) {
        QString root = doc.documentElement().attribute(QStringLiteral("root"));

        // replace proxy clips with originals
        //TODO
        QMap <QString, QString> proxies = pCore->binController()->getProxies();

        QDomNodeList producers = doc.elementsByTagName(QStringLiteral("producer"));
        QString producerResource;
        QString producerService;
        QString suffix;
        QString prefix;
        for (int n = 0; n < producers.length(); ++n) {
            QDomElement e = producers.item(n).toElement();
            producerResource = EffectsList::property(e, QStringLiteral("resource"));
            producerService = EffectsList::property(e, QStringLiteral("mlt_service"));
            if (producerResource.isEmpty()) {
                continue;
            }
            if (producerService == QLatin1String("timewarp")) {
                // slowmotion producer
                prefix = producerResource.section(':', 0, 0) + ":";
                producerResource = producerResource.section(':', 1);
            } else {
                prefix.clear();
            }
            if (producerService == QLatin1String("framebuffer")) {
                // slowmotion producer
                suffix = '?' + producerResource.section('?', 1);
                producerResource = producerResource.section('?', 0, 0);
            } else {
                suffix.clear();
            }
            if (!producerResource.startsWith('/')) {
                producerResource.prepend(root + '/');
            }
            if (!producerResource.isEmpty()) {
                if (proxies.contains(producerResource)) {
                    QString replacementResource = proxies.value(producerResource);
                    EffectsList::setProperty(e, QStringLiteral("resource"), prefix + replacementResource + suffix);
                    if (producerService == QLatin1String("timewarp")) {
                        EffectsList::setProperty(e, QStringLiteral("warp_resource"), replacementResource);
                    }
                    // We need to delete the "aspect_ratio" property because proxy clips
                    // sometimes have different ratio than original clips
                    EffectsList::removeProperty(e, QStringLiteral("aspect_ratio"));
                    EffectsList::removeMetaProperties(e);
                }
            }
        }
    }

    QList<QDomDocument> docList;

    // check which audio tracks have to be exported
    if (stemExport) {
        Timeline* ct = pCore->projectManager()->currentTimeline();
        int allTracksCount = ct->tracksCount();

        // reset tracks count (tracks to be rendered)
        tracksCount = 0;
        // begin with track 1 (track zero is a hidden black track)
        for (int i = 1; i < allTracksCount; i++) {
            Track* track = ct->track(i);
            // add only tracks to render list that are not muted and have audio
            if (track && !track->info().isMute && track->hasAudio()) {
                QDomDocument docCopy = doc.cloneNode(true).toDocument();
                QString trackName = track->info().trackName;

                // save track name
                trackNames << trackName;
                qDebug() << "Track-Name: " << trackName;

                // create stem export doc content
                QDomNodeList tracks = docCopy.elementsByTagName(QStringLiteral("track"));
                for (int j = 0; j < allTracksCount; j++) {
                    if (j != i) {
                        // mute other tracks
                        tracks.at(j).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                    }
                }
                docList << docCopy;
                tracksCount++;
            }
        }
    } else {
        docList << doc;
    }

    // create full playlistPaths
    for (int i = 0; i < tracksCount; i++) {
        QString plPath(playlistPath);

        // add track number to path name
        if (stemExport) {
            plPath = plPath + "_" + QString(trackNames.at(i)).replace(QLatin1String(" "), QLatin1String("_"));
        }
        // add mlt suffix
        plPath += mltSuffix;
        playlistPaths << plPath;
        qDebug() << "playlistPath: " << plPath << endl;

        // Do save scenelist
        QFile file(plPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_messageLabel->setMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            return;
        }
        file.write(docList.at(i).toString().toUtf8());
        if (file.error() != QFile::NoError) {
            m_messageLabel->setMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            file.close();
            return;
        }
        file.close();
    }
    m_renderWidget->slotExport(scriptExport,
            pCore->projectManager()->currentTimeline()->inPoint(),
            pCore->projectManager()->currentTimeline()->outPoint(),
            project->metadata(),
            playlistPaths, trackNames, scriptPath, exportAudio);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_effectStack->transitionConfig()->updateTimecodeFormat();
    m_effectStack->updateTimecodeFormat();
    pCore->bin()->updateTimecodeFormat();
    //pCore->projectManager()->currentTimeline()->projectView()->clearSelection();
    pCore->projectManager()->currentTimeline()->updateRuler();
    slotUpdateMousePosition(pCore->projectManager()->currentTimeline()->projectView()->getMousePos());
}

void MainWindow::slotRemoveFocus()
{
    pCore->projectManager()->currentTimeline()->setFocus();
}

void MainWindow::slotShutdown()
{
    pCore->projectManager()->current()->setModified(false);
    // Call shutdown
    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (interface && interface->isServiceRegistered(QStringLiteral("org.kde.ksmserver"))) {
        QDBusInterface smserver(QStringLiteral("org.kde.ksmserver"), QStringLiteral("/KSMServer"), QStringLiteral("org.kde.KSMServerInterface"));
        smserver.call(QStringLiteral("logout"), 1, 2, 2);
    } else if (interface && interface->isServiceRegistered(QStringLiteral("org.gnome.SessionManager"))) {
        QDBusInterface smserver(QStringLiteral("org.gnome.SessionManager"), QStringLiteral("/org/gnome/SessionManager"), QStringLiteral("org.gnome.SessionManager"));
        smserver.call(QStringLiteral("Shutdown"));
    }
}

void MainWindow::slotUpdateTrackInfo()
{
    m_effectStack->transitionConfig()->updateProjectFormat();
}

void MainWindow::slotSwitchMonitors()
{
    pCore->monitorManager()->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) pCore->projectManager()->currentTimeline()->projectView()->setFocus();
    else pCore->bin()->focusBinView();
}

void MainWindow::slotSwitchMonitorOverlay(QAction *action)
{
    if (pCore->monitorManager()->isActive(Kdenlive::ClipMonitor)) {
        m_clipMonitor->switchMonitorInfo(action->data().toInt());
    } else {
        m_projectMonitor->switchMonitorInfo(action->data().toInt());
    }
}

void MainWindow::slotSwitchDropFrames(bool drop)
{
    m_clipMonitor->switchDropFrames(drop);
    m_projectMonitor->switchDropFrames(drop);
}

void MainWindow::slotSetMonitorGamma(int gamma)
{
    KdenliveSettings::setMonitor_gamma(gamma);
    m_clipMonitor->updateMonitorGamma();
    m_projectMonitor->updateMonitorGamma();
}

void MainWindow::slotInsertZoneToTree()
{
    if (!m_clipMonitor->isActive() || m_clipMonitor->currentController() == NULL) return;
    QPoint info = m_clipMonitor->getZoneInfo();
    pCore->bin()->slotAddClipCut(m_clipMonitor->activeClipId(), info.x(), info.y());
}

void MainWindow::slotInsertZoneToTimeline()
{
    if (pCore->projectManager()->currentTimeline() == NULL || m_clipMonitor->currentController() == NULL) return;
    QPoint info = m_clipMonitor->getZoneInfo();
    pCore->projectManager()->currentTimeline()->projectView()->insertClipCut(m_clipMonitor->activeClipId(), info.x(), info.y());
}


void MainWindow::slotMonitorRequestRenderFrame(bool request)
{
    if (request) {
        m_projectMonitor->render->sendFrameForAnalysis = true;
        return;
    } else {
        for (int i = 0; i < m_gfxScopesList.count(); ++i) {
            if (m_gfxScopesList.at(i)->isVisible() && tabifiedDockWidgets(m_gfxScopesList.at(i)).isEmpty() && static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget())->autoRefreshEnabled()) {
                request = true;
                break;
            }
        }
    }
#ifdef DEBUG_MAINW
    qDebug() << "Any scope accepting new frames? " << request;
#endif
    if (!request) {
        m_projectMonitor->render->sendFrameForAnalysis = false;
    }
}


void MainWindow::slotOpenStopmotion()
{
    if (m_stopmotion == NULL) {
        m_stopmotion = new StopmotionWidget(pCore->monitorManager(), pCore->projectManager()->current()->projectFolder(), m_stopmotion_actions->actions(), this);
        //TODO
        //connect(m_stopmotion, SIGNAL(addOrUpdateSequence(QString)), m_projectList, SLOT(slotAddOrUpdateSequence(QString)));
        //for (int i = 0; i < m_gfxScopesList.count(); ++i) {
        // Check if we need the renderer to send a new frame for update
        /*if (!m_scopesList.at(i)->widget()->visibleRegion().isEmpty() && !(static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->autoRefreshEnabled())) request = true;*/
        //connect(m_stopmotion, SIGNAL(gotFrame(QImage)), static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget()), SLOT(slotRenderZoneUpdated(QImage)));
        //static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->slotMonitorCapture();
        //}
    }
    m_stopmotion->show();
}

void MainWindow::slotUpdateProxySettings()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    if (m_renderWidget) m_renderWidget->updateProxyConfig(project->useProxy());
    if (KdenliveSettings::enableproxy()) {
        QDir dir(pCore->projectManager()->current()->projectFolder().path());
        dir.mkdir(QStringLiteral("proxy"));
    }
    pCore->bin()->refreshProxySettings();
}

void MainWindow::slotArchiveProject()
{
    QList <ClipController*> list = pCore->binController()->getControllerList();
    pCore->binController()->saveDocumentProperties(pCore->projectManager()->currentTimeline()->documentProperties(), pCore->projectManager()->current()->metadata(), pCore->projectManager()->currentTimeline()->projectView()->guidesData());
    QDomDocument doc = pCore->projectManager()->current()->xmlSceneList(m_projectMonitor->sceneList());
    QPointer<ArchiveWidget> d = new ArchiveWidget(pCore->projectManager()->current()->url().fileName(), doc, list, pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), this);
    if (d->exec()) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
    delete d;
}

void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (pCore->projectManager()->current())
        currentFolder = pCore->projectManager()->current()->projectFolder().path();
    else
        currentFolder = KdenliveSettings::defaultprojectfolder();
    ResourceWidget *d = new ResourceWidget(currentFolder);
    connect(d, SIGNAL(addClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    d->show();
}

void MainWindow::slotProcessImportKeyframes(GraphicsRectItem type, const QString &tag, const QString& data)
{
    if (type == AVWidget) {
        // This data should be sent to the effect stack
        m_effectStack->setKeyframes(tag, data);
    }
    else if (type == TransitionWidget) {
        // This data should be sent to the transition stack
        m_effectStack->transitionConfig()->setKeyframes(tag, data);
    }
    else {
        // Error
    }
}

void MainWindow::slotAlignPlayheadToMousePos()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    pCore->projectManager()->currentTimeline()->projectView()->slotAlignPlayheadToMousePos();
}

void MainWindow::triggerKey(QKeyEvent* ev)
{
    // Hack: The QQuickWindow that displays fullscreen monitor does not integrate quith QActions.
    // so on keypress events we parse keys and check for shortcuts in all existing actions
    QKeySequence seq;
    // Remove the Num modifier or some shortcuts like "*" will not work
    if (ev->modifiers() != Qt::KeypadModifier) {
        seq = QKeySequence(ev->key() + ev->modifiers());
    } else {
        seq = QKeySequence(ev->key());
    }
    QList< KActionCollection * > collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        foreach( QAction* tempAction, coll->actions()) {
            if (tempAction->shortcuts().contains(seq)) {
                // Trigger action
                tempAction->trigger();
                ev->accept();
                return;
            }
        }
    }
}

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget* widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);
    return dockWidget;
}

void MainWindow::slotUpdateMonitorOverlays(int id, int code)
{
    QMenu *monitorOverlay = static_cast<QMenu*>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (!monitorOverlay) return;
    QList <QAction *> actions = monitorOverlay->actions();
    foreach(QAction *ac, actions) {
        int data = ac->data().toInt();
        if (data == 0x010) {
            ac->setEnabled(id == Kdenlive::ClipMonitor);
        }
        ac->setChecked(code & data);
    }
}

void MainWindow::slotChangeStyle(QAction *a)
{
    QString style = a->data().toString();
    KdenliveSettings::setWidgetstyle(style);
    doChangeStyle();
}

void MainWindow::doChangeStyle()
{
    QString newStyle = KdenliveSettings::widgetstyle();
    if (newStyle.isEmpty() || newStyle == QStringLiteral("Default")) {
        newStyle = defaultStyle("Breeze");
    }
    QApplication::setStyle(QStyleFactory::create(newStyle));
    // Changing widget style resets color theme, so update color theme again
    ThemeManager::instance()->slotChangePalette();
}

bool MainWindow::isTabbedWith(QDockWidget *widget, const QString & otherWidget)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    QList<QDockWidget *> tabbed = tabifiedDockWidgets(widget);
    for (int i = 0; i < tabbed.count(); i++) {
        if (tabbed.at(i)->objectName() == otherWidget)
            return true;
    }
    return false;
#else
    return false;
#endif
}

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif

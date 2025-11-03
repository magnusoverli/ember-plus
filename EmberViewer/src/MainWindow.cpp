







#include "MainWindow.h"
#include "EmberConnection.h"
#include "EmberTreeWidget.h"
#include "ParameterDelegate.h"
#include "PathColumnDelegate.h"
#include "VirtualizedMatrixWidget.h"
#include "MeterWidget.h"
#include "TriggerWidget.h"
#include "SliderWidget.h"
#include "GraphWidget.h"
#include "FunctionInvocationDialog.h"
#include "DeviceSnapshot.h"
#include "EmulatorWindow.h"
#include "UpdateManager.h"
#include "UpdateDialog.h"
#include "ConnectionManager.h"
#include "ConnectionsTreeWidget.h"
#include "ImportExportDialog.h"
#include "version.h"
#include "TreeViewController.h"
#include "SubscriptionManager.h"
#include "MatrixManager.h"
#include "CrosspointActivityTracker.h"
#include "SnapshotManager.h"
#include "FunctionInvoker.h"


#ifdef Q_OS_LINUX
#include "LinuxUpdateManager.h"
#elif defined(Q_OS_WIN)
#include "WindowsUpdateManager.h"
#elif defined(Q_OS_MACOS)
#include "MacUpdateManager.h"
#endif
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDateTime>
#include <QHeaderView>
#include <QCloseEvent>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QFileDialog>
#include <QInputDialog>
#include <QIcon>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_treeWidget(nullptr)
    , m_propertyPanel(nullptr)
    , m_consoleLog(nullptr)
    , m_consoleGroup(nullptr)
    , m_propertyGroup(nullptr)
    , m_mainSplitter(nullptr)
    , m_verticalSplitter(nullptr)
    , m_hostEdit(nullptr)
    , m_portSpin(nullptr)
    , m_connectButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_statusLabel(nullptr)
    , m_pathLabel(nullptr)
    , m_connection(nullptr)
    , m_activeMeter(nullptr)
    , m_activeMeterPath()
    , m_activeParameterWidget(nullptr)
    , m_activeParameterPath()
    , m_enableCrosspointsAction(nullptr)
    , m_crosspointsStatusLabel(nullptr)
    , m_emulatorWindow(nullptr)
    , m_updateManager(nullptr)
    , m_updateDialog(nullptr)
    , m_updateStatusLabel(nullptr)
    , m_connectionManager(nullptr)
    , m_connectionsTree(nullptr)
    , m_treeViewController(nullptr)
    , m_subscriptionManager(nullptr)
    , m_matrixManager(nullptr)
    , m_activityTracker(nullptr)
    , m_isConnected(false)
    , m_showOidPath(false)
    , m_enableQtInternalLogging(false)
{
    
    
    m_connectionManager = new ConnectionManager(this);
    m_connectionManager->loadFromDefaultLocation();
    
    setupUi();
    setupMenu();
    setupStatusBar();
    createDockWindows();
    
    
    loadSettings();
    
    
    m_connection = new EmberConnection(this);
    connect(m_connection, &EmberConnection::connected, this, [this]() {
        onConnectionStateChanged(true);
    });
    connect(m_connection, &EmberConnection::disconnected, this, [this]() {
        onConnectionStateChanged(false);
    });
    
    
    
    
    
    
    
    connect(m_connection, &EmberConnection::nodeReceived, this, &MainWindow::onNodeReceived);
    connect(m_connection, &EmberConnection::parameterReceived, this, &MainWindow::onParameterReceived);
    connect(m_connection, &EmberConnection::streamValueReceived, this, &MainWindow::onStreamValueReceived);
    
    
    connect(m_connection, &EmberConnection::matrixReceived, this, &MainWindow::onMatrixReceived);
    connect(m_connection, &EmberConnection::matrixTargetReceived, this, &MainWindow::onMatrixTargetReceived);
    connect(m_connection, &EmberConnection::matrixSourceReceived, this, &MainWindow::onMatrixSourceReceived);
    connect(m_connection, &EmberConnection::matrixConnectionReceived, this, &MainWindow::onMatrixConnectionReceived);
    connect(m_connection, &EmberConnection::matrixConnectionsCleared, this, &MainWindow::onMatrixConnectionsCleared);
    connect(m_connection, &EmberConnection::matrixTargetConnectionsCleared, this, &MainWindow::onMatrixTargetConnectionsCleared);
    connect(m_connection, &EmberConnection::functionReceived, this, &MainWindow::onFunctionReceived);
    
    
    
    m_treeViewController = new TreeViewController(m_treeWidget, m_connection, this);
    m_subscriptionManager = new SubscriptionManager(m_connection, this);
    m_matrixManager = new MatrixManager(m_connection, this);
    m_activityTracker = new CrosspointActivityTracker(m_crosspointsStatusLabel, this);
    m_functionInvoker = new FunctionInvoker(m_connection, this);
    m_snapshotManager = new SnapshotManager(m_treeWidget, m_connection, m_matrixManager, m_functionInvoker, this);
    
    
    qApp->installEventFilter(m_activityTracker);
    
    
    connect(m_activityTracker, &CrosspointActivityTracker::timeout, this, &MainWindow::onActivityTimeout);
    
    
    connect(m_treeWidget, &QTreeWidget::itemExpanded, m_treeViewController, &TreeViewController::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemExpanded, m_subscriptionManager, &SubscriptionManager::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, m_subscriptionManager, &SubscriptionManager::onItemCollapsed);
    
    
    connect(m_matrixManager, &MatrixManager::matrixDimensionsUpdated, this, &MainWindow::onMatrixDimensionsUpdated);
    connect(m_matrixManager, &MatrixManager::matrixWidgetCreated, this, &MainWindow::onMatrixWidgetCreated);
    
    
    connect(m_activityTracker, &CrosspointActivityTracker::timeRemainingChanged, 
            this, [this](int seconds) {
        VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(m_propertyPanel);
        if (matrixWidget) {
            matrixWidget->updateCornerButton(m_activityTracker->isEnabled(), seconds);
        }
    });
    

    
    setWindowTitle("EmberViewer - Ember+ Protocol Viewer");
    
    
    
    QIcon windowIcon = QIcon::fromTheme("emberviewer", QIcon(":/icon.png"));
    setWindowIcon(windowIcon);
    
    
#ifdef Q_OS_LINUX
    m_updateManager = new LinuxUpdateManager(this);
#elif defined(Q_OS_WIN)
    m_updateManager = new WindowsUpdateManager(this);
#elif defined(Q_OS_MACOS)
    m_updateManager = new MacUpdateManager(this);
#endif

    if (m_updateManager) {
        connect(m_updateManager, &UpdateManager::updateAvailable, 
                this, &MainWindow::onUpdateAvailable);
        connect(m_updateManager, &UpdateManager::noUpdateAvailable, 
                this, &MainWindow::onNoUpdateAvailable);
        connect(m_updateManager, &UpdateManager::updateCheckFailed, 
                this, &MainWindow::onUpdateCheckFailed);
        
        
        QTimer::singleShot(2000, this, &MainWindow::onCheckForUpdates);
    }
    
    
    resize(1300, 700);
    
    
    
    int leftWidth = width() * 0.55;
    int rightWidth = width() * 0.45;
    m_mainSplitter->setSizes(QList<int>() << leftWidth << rightWidth);
    
    
    int treeHeight = height() - 150;
    m_verticalSplitter->setSizes(QList<int>() << treeHeight << 150);
    
    
    QString startupMsg = QString("[%1] EmberViewer started. Ready to connect.")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    appendToConsole(startupMsg);
    
    qInfo().noquote() << "EmberViewer started. Ready to connect.";
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    
    if (watched == m_portSpin && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            
            if (m_connectButton->isEnabled()) {
                onConnectClicked();
                return true;  
            }
        }
    }
    
    
    if (watched == m_updateStatusLabel && event->type() == QEvent::MouseButtonPress) {
        if (m_updateDialog) {
            m_updateDialog->show();
            m_updateDialog->raise();
            m_updateDialog->activateWindow();
        }
        return true;
    }
    
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi()
{
    
    m_treeWidget = new EmberTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    m_treeWidget->setColumnCount(3);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);  
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);  
    m_treeWidget->setColumnWidth(1, 130);  
    m_treeWidget->header()->setStretchLastSection(true);  
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    
    
    
    
    
    
    
    
    
    m_treeWidget->setAutoExpandDelay(0);
    m_treeWidget->setAnimated(false);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setAllColumnsShowFocus(false);
    
    
    int doubleClickInterval = QApplication::doubleClickInterval();
    qDebug() << "System double-click interval:" << doubleClickInterval << "ms";
    
    
    QApplication::setDoubleClickInterval(250);  
    qDebug() << "Set double-click interval to: 250ms";
    
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
        if (!item) return;
        
        QString type = item->text(1);
        if (type == "Function") {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!m_functionInvoker->hasFunction(path)) return;
            
            QMenu contextMenu;
            QAction *invokeAction = contextMenu.addAction("Invoke Function...");
            
            QAction *selected = contextMenu.exec(m_treeWidget->mapToGlobal(pos));
            if (selected == invokeAction) {
                FunctionInfo funcInfo = m_functionInvoker->getFunctionInfo(path);
                
                FunctionInvocationDialog dialog(funcInfo.identifier, funcInfo.description,
                                               funcInfo.argNames, funcInfo.argTypes, this);
                
                if (dialog.exec() == QDialog::Accepted) {
                    QList<QVariant> arguments = dialog.getArguments();
                    m_functionInvoker->invokeFunction(path, arguments);
                    
                    qInfo().noquote() << QString("Invoking function '%1' with %2 arguments")
                        .arg(funcInfo.identifier).arg(arguments.size());
                }
            }
        }
    });
    
    
    PathColumnDelegate *pathDelegate = new PathColumnDelegate(this);
    m_treeWidget->setItemDelegateForColumn(0, pathDelegate);  
    
    
    ParameterDelegate *delegate = new ParameterDelegate(this);
    m_treeWidget->setItemDelegateForColumn(2, delegate);  
    
    
    m_treeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);
    
    
    connect(delegate, &ParameterDelegate::valueChanged, this, [this](const QString &path, const QString &newValue) {
        
        
        QTreeWidgetItem *item = m_treeViewController->findTreeItem(path);
        if (item) {
            int type = item->data(0, Qt::UserRole + 1).toInt();
            m_connection->sendParameterValue(path, newValue, type);
        }
    });
    
    
    QWidget *connectionWidget = new QWidget(this);
    QHBoxLayout *connLayout = new QHBoxLayout(connectionWidget);
    
    connLayout->addWidget(new QLabel("Host:"));
    m_hostEdit = new QLineEdit("localhost");
    m_hostEdit->setMaximumWidth(200);
    
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);
    connLayout->addWidget(m_hostEdit);
    
    connLayout->addWidget(new QLabel("Port:"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(DEFAULT_EMBER_PORT);
    m_portSpin->setMaximumWidth(80);
    
    m_portSpin->installEventFilter(this);
    connLayout->addWidget(m_portSpin);
    
    m_connectButton = new QPushButton("Connect");
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connLayout->addWidget(m_connectButton);
    
    m_disconnectButton = new QPushButton("Disconnect");
    m_disconnectButton->setEnabled(false);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connLayout->addWidget(m_disconnectButton);
    
    m_statusLabel = new QLabel("Not connected");
    connLayout->addWidget(m_statusLabel);
    
    connLayout->addStretch();
    
    
    
    QWidget *tempWidget = new QWidget(this);
    QVBoxLayout *tempLayout = new QVBoxLayout(tempWidget);
    tempLayout->addWidget(connectionWidget);
    tempLayout->setContentsMargins(0, 0, 0, 0);
    
    
    QWidget *treeContainer = new QWidget(this);
    QVBoxLayout *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->addWidget(connectionWidget);
    treeLayout->addWidget(m_treeWidget);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    
    
    setCentralWidget(treeContainer);
}

void MainWindow::setupMenu()
{
    
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    QAction *connectAction = fileMenu->addAction("&Connect");
    connectAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectClicked);
    
    QAction *disconnectAction = fileMenu->addAction("&Disconnect");
    disconnectAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnectClicked);
    
    fileMenu->addSeparator();
    
    QAction *saveConnectionAction = fileMenu->addAction("Save Current &Connection...");
    saveConnectionAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveConnectionAction, &QAction::triggered, this, &MainWindow::onSaveCurrentConnection);
    
    QAction *importConnectionsAction = fileMenu->addAction("&Import Connections...");
    connect(importConnectionsAction, &QAction::triggered, this, &MainWindow::onImportConnections);
    
    QAction *exportConnectionsAction = fileMenu->addAction("&Export Connections...");
    connect(exportConnectionsAction, &QAction::triggered, this, &MainWindow::onExportConnections);
    
    fileMenu->addSeparator();
    
    QAction *saveDeviceAction = fileMenu->addAction("Save &Device Snapshot...");
    saveDeviceAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveDeviceAction, &QAction::triggered, this, &MainWindow::onSaveEmberDevice);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    
    m_enableCrosspointsAction = editMenu->addAction("Enable &Crosspoints");
    m_enableCrosspointsAction->setShortcut(QKeySequence("Ctrl+E"));
    m_enableCrosspointsAction->setCheckable(true);
    m_enableCrosspointsAction->setChecked(false);
    connect(m_enableCrosspointsAction, &QAction::toggled, this, &MainWindow::onEnableCrosspointsToggled);
    
    
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    
    QAction *emulatorAction = toolsMenu->addAction("Open &Emulator...");
    emulatorAction->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(emulatorAction, &QAction::triggered, this, &MainWindow::onOpenEmulator);
    
    toolsMenu->addSeparator();
    
    QAction *openLogsAction = toolsMenu->addAction("Open &Log Directory");
    connect(openLogsAction, &QAction::triggered, this, [this]() {
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/EmberViewer/logs";
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
    });
    
    toolsMenu->addSeparator();
    
    QAction *qtInternalLoggingAction = toolsMenu->addAction("Enable &Qt Internal Logging");
    qtInternalLoggingAction->setCheckable(true);
    qtInternalLoggingAction->setChecked(m_enableQtInternalLogging);
    connect(qtInternalLoggingAction, &QAction::toggled, this, &MainWindow::setQtInternalLoggingEnabled);
    
    
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *checkUpdatesAction = helpMenu->addAction("Check for &Updates...");
    connect(checkUpdatesAction, &QAction::triggered, this, &MainWindow::onCheckForUpdates);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About EmberViewer",
            "<h3>EmberViewer v" EMBERVIEWER_VERSION_STRING "</h3>"
            "<p>A modern, cross-platform Ember+ protocol viewer.</p>"
            "<p><b>Built with:</b><br>"
            "Qt5 and libember by Lawo GmbH</p>"
            "<p><b>License:</b><br>"
            "Distributed under the Boost Software License, Version 1.0<br>"
            "See LICENSE_1_0.txt for details</p>"
            "<p><b>Copyright:</b><br>"
            "© 2012-2019 Lawo GmbH (libember)<br>"
            "© 2025 Magnus Overli (EmberViewer)</p>"
            "<p><a href='https:
    });
}


void MainWindow::setupStatusBar()
{
    
    m_pathLabel = new QLabel("No selection");
    m_pathLabel->setStyleSheet("QLabel { padding: 2px 8px; }");
    m_pathLabel->setMinimumWidth(200);
    statusBar()->addPermanentWidget(m_pathLabel, 1);  
    
    
    m_crosspointsStatusLabel = new QLabel("");
    m_crosspointsStatusLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #c62828; font-weight: bold; }");
    m_crosspointsStatusLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_crosspointsStatusLabel);
    
    
    m_updateStatusLabel = new QLabel("");
    m_updateStatusLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #1976d2; cursor: pointer; }");
    m_updateStatusLabel->setVisible(false);
    m_updateStatusLabel->setCursor(Qt::PointingHandCursor);
    m_updateStatusLabel->installEventFilter(this);
    statusBar()->addPermanentWidget(m_updateStatusLabel);
    
    
    QCheckBox *pathToggle = new QCheckBox("Show OID Path");
    pathToggle->setChecked(false);
    connect(pathToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_showOidPath = checked;
        onTreeSelectionChanged();  
    });
    statusBar()->addPermanentWidget(pathToggle);
    
    
}

void MainWindow::createDockWindows()
{
    
    QWidget *treeContainer = centralWidget();
    
    
    m_consoleGroup = new QGroupBox("Console", this);
    QVBoxLayout *consoleLayout = new QVBoxLayout(m_consoleGroup);
    m_consoleLog = new QTextEdit();
    m_consoleLog->setReadOnly(true);
    consoleLayout->addWidget(m_consoleLog);
    consoleLayout->setContentsMargins(5, 5, 5, 5);
    
    
    m_propertyGroup = new QGroupBox("Properties", this);
    QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
    m_propertyPanel = new QWidget();
    QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
    propContentLayout->addWidget(new QLabel("Select an item to view properties"));
    propContentLayout->addStretch();
    propLayout->addWidget(m_propertyPanel);
    propLayout->setContentsMargins(5, 5, 5, 5);
    
    
    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->addWidget(treeContainer);
    m_verticalSplitter->addWidget(m_consoleGroup);
    
    
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_verticalSplitter);
    m_mainSplitter->addWidget(m_propertyGroup);
    
    
    if (m_connectionManager) {
        m_connectionsTree = new ConnectionsTreeWidget(m_connectionManager, this);
        m_connectionsTree->setMinimumWidth(200);
        m_connectionsTree->setMaximumWidth(300);
        connect(m_connectionsTree, &ConnectionsTreeWidget::connectionDoubleClicked,
                this, &MainWindow::onSavedConnectionDoubleClicked);
        
        
        QSplitter *outerSplitter = new QSplitter(Qt::Horizontal, this);
        outerSplitter->addWidget(m_connectionsTree);
        outerSplitter->addWidget(m_mainSplitter);
        
        
        setCentralWidget(outerSplitter);
    } else {
        
        setCentralWidget(m_mainSplitter);
    }
}

void MainWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    
    
    
    qInfo().noquote() << QString("Connecting to %1:%2...").arg(host).arg(port);
    m_connection->connectToHost(host, port);
}

void MainWindow::onDisconnectClicked()
{
    qInfo().noquote() << "Disconnecting...";
    m_connection->disconnect();
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    m_isConnected = connected;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    
    if (connected) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        logMessage("Connected successfully!");
        
        
        
        
        
    } else {
        m_statusLabel->setText("Not connected");
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        qInfo().noquote() << "Disconnected";
        
        
        VirtualizedMatrixWidget *currentMatrix = qobject_cast<VirtualizedMatrixWidget*>(m_propertyPanel);
        if (currentMatrix) {
            
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            
            m_propertyPanel = new QWidget();
            QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
            propContentLayout->addWidget(new QLabel("Not connected"));
            propContentLayout->addStretch();
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(m_propertyPanel);
            propLayout->setContentsMargins(5, 5, 5, 5);
        }
        
        m_treeWidget->clear();
        
        
        m_treeViewController->clear();
        m_subscriptionManager->clear();
        m_matrixManager->clear();
        m_activeMeterPath.clear();
    }
}

void MainWindow::onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline)
{
    
    m_treeViewController->onNodeReceived(path, identifier, description, isOnline);
}

void MainWindow::onParameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier,
                                    const QString &format, const QString &referenceLevel, const QString &formula, int factor)
{
    
    if (path.contains("matrix", Qt::CaseInsensitive) || path.contains("label", Qt::CaseInsensitive)) {
        qDebug().noquote() << QString("PARAM_TRACE: Path='%1', Identifier='%2', Value='%3'")
            .arg(path).arg(identifier).arg(value);
    }
    
    if (streamIdentifier > 0) {
        m_streamIdToPath[streamIdentifier] = path;
    }
    
    
    
    QStringList pathParts = path.split('.');
    if (pathParts.size() >= 4 && pathParts[pathParts.size() - 3] == QString::number(MATRIX_LABEL_PATH_MARKER)) {
        qDebug().noquote() << QString("LABEL_MATCH: Detected matrix label parameter - Path: %1").arg(path);
        
        QString labelType = pathParts[pathParts.size() - 2];  
        int labelNumber = pathParts.last().toInt();
        
        
        QStringList matrixPathParts = pathParts.mid(0, pathParts.size() - 3);
        QString matrixPath = matrixPathParts.join('.');
        
        qDebug().noquote() << QString("LABEL_MATCH: Matrix path: %1, Type: %2, Number: %3, Value: %4")
            .arg(matrixPath).arg(labelType).arg(labelNumber).arg(value);
        
        if (labelType == "1") {
            
            m_matrixManager->onMatrixTargetReceived(matrixPath, labelNumber, value);
        } else if (labelType == "2") {
            
            m_matrixManager->onMatrixSourceReceived(matrixPath, labelNumber, value);
        }
        
        return; 
    } else if (path.contains("label", Qt::CaseInsensitive)) {
        
        qDebug().noquote() << QString("LABEL_NO_MATCH: Path contains 'label' but doesn't match pattern. Path='%1', Parts=%2, Marker='%3'")
            .arg(path).arg(pathParts.size()).arg(MATRIX_LABEL_PATH_MARKER);
    }
    
    
    m_treeViewController->onParameterReceived(path, number, identifier, value, access, type, 
                                             minimum, maximum, enumOptions, enumValues, isOnline, streamIdentifier, format, referenceLevel, formula, factor);
}

void MainWindow::onMatrixReceived(const QString &path, int number, const QString &identifier, 
                                   const QString &description, int type, int targetCount, int sourceCount)
{
    
    m_treeViewController->onMatrixReceived(path, number, identifier, description, type, targetCount, sourceCount);
    
    
    m_matrixManager->onMatrixReceived(path, number, identifier, description, type, targetCount, sourceCount);
}

void MainWindow::onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label)
{
    m_matrixManager->onMatrixTargetReceived(matrixPath, targetNumber, label);
}

void MainWindow::onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label)
{
    m_matrixManager->onMatrixSourceReceived(matrixPath, sourceNumber, label);
}

void MainWindow::onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition)
{
    m_matrixManager->onMatrixConnectionReceived(matrixPath, targetNumber, sourceNumber, connected, disposition);
}

void MainWindow::onMatrixConnectionsCleared(const QString &matrixPath)
{
    m_matrixManager->onMatrixConnectionsCleared(matrixPath);
}

void MainWindow::onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber)
{
    m_matrixManager->onMatrixTargetConnectionsCleared(matrixPath, targetNumber);
}

void MainWindow::onMatrixWidgetCreated(const QString &path, QWidget *widget)
{
    qInfo().noquote() << QString("onMatrixWidgetCreated called for path: %1").arg(path);
    
    
    VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(widget);
    if (matrixWidget) {
        
        connect(matrixWidget, 
                static_cast<void(VirtualizedMatrixWidget::*)(const QString&, int, int)>(&VirtualizedMatrixWidget::crosspointClicked),
                this, &MainWindow::onCrosspointClicked);
        
        
        connect(matrixWidget, &VirtualizedMatrixWidget::enableCrosspointsRequested,
                this, [this](bool enable) {
            m_enableCrosspointsAction->setChecked(enable);
        });
        
        qDebug().noquote() << QString("Connected signals for matrix: %1").arg(path);
    }
}

void MainWindow::onMatrixDimensionsUpdated(const QString &path, QWidget *widget)
{
    qInfo().noquote() << QString("onMatrixDimensionsUpdated called for path: %1").arg(path);
    
    
    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    if (!selected.isEmpty()) {
        QTreeWidgetItem *item = selected.first();
        QString selectedPath = item->data(0, Qt::UserRole).toString();
        QString selectedType = item->text(1);
        
        qInfo().noquote() << QString("Currently selected: type=%1, path=%2").arg(selectedType).arg(selectedPath);
        
        
        if (selectedType == "Matrix" && selectedPath == path) {
            qInfo().noquote() << QString("Matrix dimensions updated for currently selected matrix: %1, refreshing display").arg(path);
            
            
            
            VirtualizedMatrixWidget *virtualizedWidget = qobject_cast<VirtualizedMatrixWidget*>(widget);
            if (virtualizedWidget) {
                virtualizedWidget->rebuild();
            }
            
            
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(widget);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = widget;
        } else {
            qInfo().noquote() << QString("Matrix updated but not currently selected. Updated: %1, Selected: %2 (%3)")
                .arg(path).arg(selectedPath).arg(selectedType);
        }
    } else {
        qInfo().noquote() << "No item currently selected";
    }
}

void MainWindow::onTreeSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    
    if (selected.isEmpty()) {
        m_pathLabel->setText("No selection");
        
        if (m_activityTracker && m_activityTracker->isEnabled()) {
            m_enableCrosspointsAction->setChecked(false);
        }
        return;
    }
    
    QTreeWidgetItem *item = selected.first();
    QString oidPath = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    
    
    cleanupActiveParameterWidget();
    if (m_activityTracker && m_activityTracker->isEnabled()) {
        m_enableCrosspointsAction->setChecked(false);
    }
    cleanupPropertyPanelLayout();
    
    
    if (m_showOidPath) {
        
        if (!oidPath.isEmpty()) {
            m_pathLabel->setText(QString("%1  [%2]").arg(oidPath).arg(type));
        } else {
            m_pathLabel->setText(QString("(no path)  [%1]").arg(type));
        }
    } else {
        
        QStringList breadcrumbs;
        QTreeWidgetItem *current = item;
        
        while (current) {
            QString name = current->text(0);
            if (!name.isEmpty()) {
                breadcrumbs.prepend(name);
            }
            current = current->parent();
        }
        
        QString breadcrumbPath = breadcrumbs.join(" → ");
        m_pathLabel->setText(QString("%1  [%2]").arg(breadcrumbPath).arg(type));
    }
    
    
    if (type == "Matrix") {
        
        QString dimensionText = item->text(2);
        qInfo().noquote() << QString("Matrix selected: %1, dimensions: %2").arg(oidPath).arg(dimensionText);
        
        
        if (dimensionText == "0×0" || dimensionText.isEmpty()) {
            
            if (!m_treeViewController->hasPathBeenFetched(oidPath)) {
                qInfo().noquote() << QString("Matrix has no dimensions, requesting details for: %1").arg(oidPath);
                m_treeViewController->markPathAsFetched(oidPath);
                m_connection->sendGetDirectoryForPath(oidPath);
            }
        } else {
            
            QWidget *existingWidget = m_matrixManager->getMatrix(oidPath);
            if (existingWidget) {
                qInfo().noquote() << QString("Displaying existing matrix widget for: %1").arg(oidPath);
                
                
                QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
                propLayout->addWidget(existingWidget);
                propLayout->setContentsMargins(5, 5, 5, 5);
                existingWidget->show();  
                m_propertyPanel = existingWidget;
                
                
                VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(existingWidget);
                if (matrixWidget && m_activityTracker) {
                    matrixWidget->updateCornerButton(m_activityTracker->isEnabled(), 
                                                     m_activityTracker->isEnabled() ? 60 : 0);
                }
            }
        }
    }
    
    
    if (type == "Parameter") {
        
        int streamIdentifier = item->data(0, Qt::UserRole + 9).toInt();
        int paramType = item->data(0, Qt::UserRole + 1).toInt();
        bool isAudioMeter = (streamIdentifier > 0) && (paramType == 1 || paramType == 2);
        
        qDebug().noquote() << QString("[MainWindow] Parameter selected - Path: %1, StreamID: %2, ParamType: %3, IsAudioMeter: %4")
            .arg(oidPath).arg(streamIdentifier).arg(paramType).arg(isAudioMeter ? "YES" : "NO");
        
        if (isAudioMeter) {
            
            
            if (!m_activeMeterPath.isEmpty()) {
                m_connection->unsubscribeFromParameter(m_activeMeterPath);
                qDebug().noquote() << QString("Unsubscribed from previous meter: %1")
                    .arg(m_activeMeterPath);
                m_activeMeterPath.clear();
            }
            
            
            if (m_activeMeter) {
                m_activeMeter = nullptr;  
            }
            
            
            if (m_activityTracker && m_activityTracker->isEnabled()) {
                m_enableCrosspointsAction->setChecked(false);
            }
            
            
            
            
            m_propertyPanel = nullptr;
            
            
            m_activeMeter = new MeterWidget();
            
            
            QString identifier = item->text(0).replace("📊 ", "");  
            QVariant minVar = item->data(0, Qt::UserRole + 3);
            QVariant maxVar = item->data(0, Qt::UserRole + 4);
            
            double minValue = minVar.isValid() ? minVar.toDouble() : 0.0;
            double maxValue = maxVar.isValid() ? maxVar.toDouble() : 100.0;
            
            
            
            QString format = item->data(0, Qt::UserRole + 10).toString();
            QString referenceLevel = item->data(0, Qt::UserRole + 11).toString();
            QString formula = item->data(0, Qt::UserRole + 12).toString();
            int factor = item->data(0, Qt::UserRole + 13).toInt();
            if (factor == 0) factor = 1;  
            qDebug() << "[MainWindow] Read from tree item - format:" << format << "referenceLevel:" << referenceLevel 
                     << "formula:" << formula << "factor:" << factor << "for path:" << oidPath;
            m_activeMeter->setParameterInfo(identifier, oidPath, minValue, maxValue, format, referenceLevel, factor);
            m_activeMeter->setStreamIdentifier(streamIdentifier);
            
            
            
            if (!oidPath.isEmpty() && m_isConnected) {
                m_connection->subscribeToParameter(oidPath, true);
                m_activeMeterPath = oidPath;
                qDebug().noquote() << QString("Subscribed to meter parameter: %1 (stream ID: %2)")
                    .arg(oidPath).arg(streamIdentifier);
            }
            
            
            QString currentValue = item->text(2);
            bool ok;
            double val = currentValue.toDouble(&ok);
            if (ok) {
                m_activeMeter->updateValue(val);
            }
            
            
            
            QWidget *meterContainer = new QWidget();
            QVBoxLayout *containerLayout = new QVBoxLayout(meterContainer);
            containerLayout->setContentsMargins(0, 0, 0, 0);
            
            
            containerLayout->addWidget(m_activeMeter);
            
            
            
            QString rangeInfo;
            bool likelyMismatch = (minValue >= 0.0 && !referenceLevel.isEmpty() && 
                                   referenceLevel.contains("dB", Qt::CaseInsensitive));
            
            if (likelyMismatch) {
                
                if (referenceLevel.contains("dBFS", Qt::CaseInsensitive) || 
                    referenceLevel.contains("dBTP", Qt::CaseInsensitive)) {
                    rangeInfo = QString("Min: -60.0 dBFS (auto)\nMax: 0.0 dBFS (auto)");
                } else if (referenceLevel.contains("dBr", Qt::CaseInsensitive)) {
                    rangeInfo = QString("Min: -50.0 dBr (auto)\nMax: +5.0 dBr (auto)");
                } else if (referenceLevel.contains("dBu", Qt::CaseInsensitive)) {
                    rangeInfo = QString("Min: -20.0 dBu (auto)\nMax: +20.0 dBu (auto)");
                } else if (referenceLevel.contains("LUFS", Qt::CaseInsensitive)) {
                    rangeInfo = QString("Min: -40.0 LUFS (auto)\nMax: 0.0 LUFS (auto)");
                } else if (factor > 1) {
                    
                    double rangeInDb = 2560.0 / factor;
                    double minCalc = -rangeInDb;
                    double maxCalc = rangeInDb / 4.0;
                    rangeInfo = QString("Min: %1 dB (factor=%2)\nMax: %3 dB (factor=%2)")
                        .arg(minCalc, 0, 'f', 1).arg(factor).arg(maxCalc, 0, 'f', 1);
                } else {
                    rangeInfo = QString("Min: -10.0 dB (auto)\nMax: +10.0 dB (auto)");
                }
            } else {
                rangeInfo = QString("Min: %1\nMax: %2").arg(minValue).arg(maxValue);
            }
            
            QLabel *infoLabel = new QLabel(QString("Path: %1\n%2\nStream ID: %3")
                .arg(oidPath).arg(rangeInfo).arg(streamIdentifier));
            infoLabel->setStyleSheet("padding: 10px; background-color: transparent; border-top: 1px solid #ddd;");
            infoLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            containerLayout->addWidget(infoLabel);
            
            
            QScrollArea *scrollArea = new QScrollArea();
            scrollArea->setWidget(meterContainer);
            scrollArea->setWidgetResizable(true);
            scrollArea->setFrameShape(QFrame::NoFrame);
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(scrollArea);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = m_activeMeter;
        }
        
        else if (paramType == 5) {
            
            int access = item->data(0, Qt::UserRole + 2).toInt();
            QString identifier = item->text(0);
            
            
            cleanupActiveParameterWidget();
            
            
            TriggerWidget *triggerWidget = new TriggerWidget();
            triggerWidget->setParameterInfo(identifier, oidPath, access);
            connect(triggerWidget, &TriggerWidget::triggerActivated,
                    this, [this](QString path, QString value) {
                        m_connection->sendParameterValue(path, value, 1);  
                    });
            
            
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    if (item->widget()) item->widget()->deleteLater();
                    delete item;
                }
                delete oldLayout;
            }
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(triggerWidget);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = triggerWidget;
            m_activeParameterWidget = triggerWidget;
            m_activeParameterPath = oidPath;
        }
        
        else if (paramType == 1 || paramType == 2) {
            QVariant minVar = item->data(0, Qt::UserRole + 3);
            QVariant maxVar = item->data(0, Qt::UserRole + 4);
            QString formula = item->data(0, Qt::UserRole + 12).toString();
            int access = item->data(0, Qt::UserRole + 2).toInt();
            QString identifier = item->text(0);
            QString format = item->data(0, Qt::UserRole + 10).toString();
            
            bool hasRange = minVar.isValid() && maxVar.isValid();
            double minValue = minVar.toDouble();
            double maxValue = maxVar.toDouble();
            double range = maxValue - minValue;
            
            
            bool useSlider = hasRange && (range > 50.0 || !formula.isEmpty() || (paramType == 2 && range > 10.0));
            
            if (useSlider) {
                
                
                
                SliderWidget *sliderWidget = new SliderWidget();
                sliderWidget->setParameterInfo(identifier, oidPath, minValue, maxValue, paramType, access, formula, format);
                
                
                QString currentValue = item->text(2);
                bool ok;
                double val = currentValue.toDouble(&ok);
                if (ok) {
                    sliderWidget->setValue(val);
                }
                
                
                connect(sliderWidget, &SliderWidget::valueChanged,
                        this, [this](QString path, QString newValue, int type) {
                            m_connection->sendParameterValue(path, newValue, type);
                        });
                
            
            cleanupPropertyPanelLayout();
                
                QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
                propLayout->addWidget(sliderWidget);
                propLayout->setContentsMargins(5, 5, 5, 5);
                m_propertyPanel = sliderWidget;
                m_activeParameterWidget = sliderWidget;
                m_activeParameterPath = oidPath;
            }
        }
        
        else if (streamIdentifier > 0 && !isAudioMeter) {
            
            QVariant minVar = item->data(0, Qt::UserRole + 3);
            QVariant maxVar = item->data(0, Qt::UserRole + 4);
            QString identifier = item->text(0);
            QString format = item->data(0, Qt::UserRole + 10).toString();
            
            double minValue = minVar.isValid() ? minVar.toDouble() : 0.0;
            double maxValue = maxVar.isValid() ? maxVar.toDouble() : 100.0;
            
            
            
            
            GraphWidget *graphWidget = new GraphWidget();
            graphWidget->setParameterInfo(identifier, oidPath, minValue, maxValue, format);
            graphWidget->setStreamIdentifier(streamIdentifier);
            
            
            if (!oidPath.isEmpty() && m_isConnected) {
                m_connection->subscribeToParameter(oidPath, true);
                m_activeParameterPath = oidPath;
                qDebug().noquote() << QString("Subscribed to graph parameter: %1 (stream ID: %2)")
                    .arg(oidPath).arg(streamIdentifier);
            }
            
            
            QString currentValue = item->text(2);
            bool ok;
            double val = currentValue.toDouble(&ok);
            if (ok) {
                graphWidget->addDataPoint(val);
            }
            
            
            cleanupPropertyPanelLayout();
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(graphWidget);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = graphWidget;
            m_activeParameterWidget = graphWidget;
        } else {
            
            qDebug().noquote() << QString("Generic parameter selected (no special widget): %1").arg(oidPath);
            
            
            
            m_propertyPanel = new QWidget();
            QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
            propContentLayout->addWidget(new QLabel("Parameter properties"));
            propContentLayout->addWidget(new QLabel(QString("Path: %1").arg(oidPath)));
            propContentLayout->addWidget(new QLabel(QString("Value: %1").arg(item->text(2))));
            propContentLayout->addStretch();
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(m_propertyPanel);
            propLayout->setContentsMargins(5, 5, 5, 5);
            
            m_propertyGroup->setTitle("Properties");
        }
    }
    else if (type == "Matrix") {
        
        
        if (!m_activeMeterPath.isEmpty()) {
            m_connection->unsubscribeFromParameter(m_activeMeterPath);
            qDebug().noquote() << QString("Unsubscribed from meter (switching to Matrix): %1")
                .arg(m_activeMeterPath);
            m_activeMeterPath.clear();
        }
        
        qInfo().noquote() << QString("Matrix selected: %1").arg(oidPath);
        
        
        m_connection->subscribeToMatrix(oidPath, false);
        
        VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixManager->getMatrix(oidPath));
        qInfo().noquote() << QString("Matrix widget pointer: %1").arg(matrixWidget ? "EXISTS" : "NULL");
        if (matrixWidget) {
            
            
            matrixWidget->rebuild();
            
            
            QWidget *oldWidget = m_propertyPanel;
            if (oldWidget) {
                
                VirtualizedMatrixWidget *oldMatrix = qobject_cast<VirtualizedMatrixWidget*>(oldWidget);
                if (!oldMatrix) {
                    
                    oldWidget->deleteLater();
                }
            }
            
            
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(matrixWidget);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = matrixWidget;
            
            
            int matrixType = matrixWidget->getMatrixType();
            QString matrixTypeStr;
            switch (matrixType) {
                case 0: matrixTypeStr = "1:N"; break;
                case 1: matrixTypeStr = "N:1"; break;
                case 2: matrixTypeStr = "N:N"; break;
                default: matrixTypeStr = "Unknown"; break;
            }
            int sourceCount = matrixWidget->getSourceNumbers().size();
            int targetCount = matrixWidget->getTargetNumbers().size();
            m_propertyGroup->setTitle(QString("Matrix: %1×%2 (%3)")
                .arg(sourceCount)
                .arg(targetCount)
                .arg(matrixTypeStr));
            
            
        }
    } else {
        
        qDebug().noquote() << QString("Selected type: '%1' - showing basic info").arg(type);
        
        
        
        m_propertyPanel = new QWidget();
        QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
        
        QString identifier = item->text(0);
        QString value = item->text(2);
        
        propContentLayout->addWidget(new QLabel(QString("<b>Type:</b> %1").arg(type)));
        if (!identifier.isEmpty()) {
            propContentLayout->addWidget(new QLabel(QString("<b>Name:</b> %1").arg(identifier)));
        }
        if (!oidPath.isEmpty()) {
            propContentLayout->addWidget(new QLabel(QString("<b>Path:</b> %1").arg(oidPath)));
        }
        if (!value.isEmpty() && value != "—") {
            propContentLayout->addWidget(new QLabel(QString("<b>Value:</b> %1").arg(value)));
        }
        propContentLayout->addStretch();
        
        QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
        propLayout->addWidget(m_propertyPanel);
        propLayout->setContentsMargins(5, 5, 5, 5);
        
        m_propertyGroup->setTitle(QString("%1 Properties").arg(type));
    }
}

void MainWindow::cleanupActiveParameterWidget()
{
    
    if (!m_activeParameterPath.isEmpty()) {
        m_connection->unsubscribeFromParameter(m_activeParameterPath);
        qDebug().noquote() << QString("Unsubscribed from parameter: %1")
            .arg(m_activeParameterPath);
        m_activeParameterPath.clear();
    }
    
    
    m_activeParameterWidget = nullptr;
}

void MainWindow::cleanupPropertyPanelLayout()
{
    
    QLayout *oldLayout = m_propertyGroup->layout();
    if (oldLayout) {
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                QWidget *widget = item->widget();
                
                
                VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(widget);
                if (matrixWidget) {
                    
                    
                    matrixWidget->hide();
                    matrixWidget->setParent(nullptr);  
                    qDebug().noquote() << "Hidden matrix widget (not deleted - managed by MatrixManager)";
                } else {
                    
                    widget->deleteLater();
                }
            }
            delete item;
        }
        delete oldLayout;
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    
    QString host = settings.value("connection/host", "localhost").toString();
    int port = settings.value("connection/port", DEFAULT_PORT_FALLBACK).toInt();
    
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);
    
    
    m_enableQtInternalLogging = settings.value("logging/qtInternal", false).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    
    settings.setValue("connection/host", m_hostEdit->text());
    settings.setValue("connection/port", m_portSpin->value());
    
    
    settings.setValue("logging/qtInternal", m_enableQtInternalLogging);
    
    
    settings.sync();
}

void MainWindow::setQtInternalLoggingEnabled(bool enabled)
{
    m_enableQtInternalLogging = enabled;
    saveSettings();
    
    
    if (enabled) {
        qInfo() << "Qt internal logging enabled";
    } else {
        qInfo() << "Qt internal logging disabled";
    }
}

void MainWindow::logMessage(const QString &message)
{
    qInfo().noquote() << message;
}

void MainWindow::appendToConsole(const QString &message)
{
    if (m_consoleLog) {
        m_consoleLog->append(message);
    }
}

void MainWindow::onActivityTimeout()
{
    
    m_enableCrosspointsAction->setChecked(false);
    logMessage("Crosspoint editing auto-disabled after 60 seconds of inactivity");
}

void MainWindow::onEnableCrosspointsToggled(bool enabled)
{
    if (enabled) {
        
        m_activityTracker->enable();
        logMessage("Crosspoint editing ENABLED (60 second timeout)");
    } else {
        
        m_activityTracker->disable();
        logMessage("Crosspoint editing DISABLED");
    }
    
    
    VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(m_propertyPanel);
    if (matrixWidget) {
        matrixWidget->setCrosspointsEnabled(enabled);
        matrixWidget->updateCornerButton(enabled, enabled ? 60 : 0);
    }
}



void MainWindow::onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber)
{
    if (!m_activityTracker || !m_activityTracker->isEnabled()) {
        qDebug().noquote() << "Crosspoint click ignored - crosspoints not enabled";
        return;
    }
    
    
    m_activityTracker->resetTimer();
    
    QWidget *widget = m_matrixManager->getMatrix(matrixPath);
    if (!widget) {
        qWarning().noquote() << "Matrix widget not found for path: " + matrixPath;
        return;
    }
    
    
    bool currentlyConnected = false;
    int matrixType = 2; 
    
    VirtualizedMatrixWidget *matrixWidget = qobject_cast<VirtualizedMatrixWidget*>(widget);
    if (matrixWidget) {
        currentlyConnected = matrixWidget->isConnected(targetNumber, sourceNumber);
        matrixType = matrixWidget->getMatrixType();
    }
    
    QString typeStr;
    if (matrixType == 0) typeStr = "1:N";
    else if (matrixType == 1) typeStr = "1:1";
    else typeStr = "N:N";
    
    
    
    bool newState = !currentlyConnected;
    
    
    QString targetLabel = matrixWidget->getTargetLabel(targetNumber);
    QString sourceLabel = matrixWidget->getSourceLabel(sourceNumber);
    
    if (newState) {
        qInfo().noquote() << QString("Crosspoint CONNECT: %1 [%2] ← %3 [%4]")
                   .arg(targetLabel, QString::number(targetNumber), sourceLabel, QString::number(sourceNumber));
    } else {
        qInfo().noquote() << QString("Crosspoint DISCONNECT: %1 [%2]")
                   .arg(targetLabel, QString::number(targetNumber));
    }
    
    
    matrixWidget->setConnection(targetNumber, sourceNumber, newState, 2);
    
    
    m_connection->setMatrixConnection(matrixPath, targetNumber, sourceNumber, newState);
    
    
    
}

void MainWindow::onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                                   const QStringList &argNames, const QList<int> &argTypes,
                                   const QStringList &resultNames, const QList<int> &resultTypes)
{
    
    m_functionInvoker->registerFunction(path, identifier, description, argNames, argTypes, resultNames, resultTypes);
    
    
    m_treeViewController->onFunctionReceived(path, identifier, description, argNames, argTypes, resultNames, resultTypes);
}



void MainWindow::onStreamValueReceived(int streamIdentifier, double value)
{
    
    if (m_activeMeter && m_activeMeter->streamIdentifier() == streamIdentifier) {
        m_activeMeter->updateValue(value);
        
        
        
    }
    
    else if (m_activeParameterWidget) {
        GraphWidget *graphWidget = qobject_cast<GraphWidget*>(m_activeParameterWidget);
        if (graphWidget && graphWidget->streamIdentifier() == streamIdentifier) {
            graphWidget->addDataPoint(value);
        }
    }
}

void MainWindow::onSaveEmberDevice()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Not Connected", 
            "Cannot save device: not connected to an Ember+ provider.");
        return;
    }
    
    
    m_snapshotManager->saveSnapshot(m_hostEdit, m_portSpin);
}



void MainWindow::onOpenEmulator()
{
    if (!m_emulatorWindow) {
        
        m_emulatorWindow = new EmulatorWindow(nullptr);
        
        
        connect(m_emulatorWindow, &QObject::destroyed, this, [this]() {
            m_emulatorWindow = nullptr;
        });
    }
    m_emulatorWindow->show();
    m_emulatorWindow->raise();
    m_emulatorWindow->activateWindow();
}

void MainWindow::onCheckForUpdates()
{
    if (!m_updateManager) {
        qWarning() << "Update manager not available on this platform";
        QMessageBox::information(this, "Updates Not Available",
            "Automatic updates are not available on this platform.\n"
            "Please check for updates manually on GitHub.");
        return;
    }

    qInfo() << "Checking for updates...";
    statusBar()->showMessage("Checking for updates...", 3000);
    m_updateManager->checkForUpdates();
}

void MainWindow::onUpdateAvailable(const UpdateManager::UpdateInfo &updateInfo)
{
    qInfo() << "Update available:" << updateInfo.version;
    
    
    m_updateStatusLabel->setText(QString("⬇ Update to v%1 available - Click to install").arg(updateInfo.version));
    m_updateStatusLabel->setVisible(true);
    
    
    if (m_updateDialog) {
        delete m_updateDialog;
    }
    
    m_updateDialog = new UpdateDialog(updateInfo, this);
    
    
    connect(m_updateDialog, &UpdateDialog::updateNowClicked, this, [this, updateInfo]() {
        qInfo() << "User chose to update now";
        m_updateManager->installUpdate(updateInfo);
    });
    
    connect(m_updateDialog, &UpdateDialog::remindLaterClicked, this, [this]() {
        qInfo() << "User chose to be reminded later";
        m_updateStatusLabel->setVisible(true);
    });
    
    connect(m_updateDialog, &UpdateDialog::skipVersionClicked, this, [this, updateInfo]() {
        qInfo() << "User chose to skip version:" << updateInfo.version;
        m_updateManager->skipVersion(updateInfo.version);
        m_updateStatusLabel->setVisible(false);
    });
    
    
    connect(m_updateManager, &UpdateManager::downloadProgress, m_updateDialog, &UpdateDialog::setDownloadProgress);
    connect(m_updateManager, &UpdateManager::installationStarted, this, [this]() {
        qInfo() << "Installation started";
    });
    connect(m_updateManager, &UpdateManager::installationFinished, this, [this](bool success, const QString &message) {
        if (m_updateDialog) {
            m_updateDialog->setInstallationStatus(message);
        }
        
        if (!success) {
            QMessageBox::warning(this, "Update Failed", message);
        }
    });
    
    
    m_updateDialog->show();
}

void MainWindow::onNoUpdateAvailable()
{
    qInfo() << "No update available";
    statusBar()->showMessage("You are running the latest version", 3000);
    m_updateStatusLabel->setVisible(false);
}

void MainWindow::onUpdateCheckFailed(const QString &errorMessage)
{
    qWarning() << "Update check failed:" << errorMessage;
    statusBar()->showMessage("Update check failed", 3000);
    m_updateStatusLabel->setVisible(false);
}

void MainWindow::onSaveCurrentConnection()
{
    if (!m_isConnected) {
        QMessageBox::information(this, "Not Connected", 
            "Please connect to a device before saving the connection.");
        return;
    }

    
    QString deviceName;
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstRoot = m_treeWidget->topLevelItem(0);
        deviceName = firstRoot->text(0);
    }

    
    if (deviceName.isEmpty()) {
        deviceName = m_hostEdit->text();
    }

    QString host = m_hostEdit->text();
    int port = m_portSpin->value();

    
    QStringList folderNames;
    folderNames.append("(Root Level)");  
    
    QList<ConnectionManager::Folder> folders = m_connectionManager->getAllFolders();
    QMap<QString, QString> folderNameToId;  
    folderNameToId["(Root Level)"] = QString();  
    
    for (const ConnectionManager::Folder &folder : folders) {
        folderNames.append(folder.name);
        folderNameToId[folder.name] = folder.id;
    }

    bool ok;
    QString selectedFolder = QInputDialog::getItem(this, "Save Connection", 
        "Choose folder to save connection in:", folderNames, 0, false, &ok);

    if (ok) {
        QString folderId = folderNameToId[selectedFolder];
        m_connectionManager->addConnection(deviceName, host, port, folderId);
        m_connectionManager->saveToDefaultLocation();
        
        qInfo() << "Saved connection:" << deviceName << "(" << host << ":" << port << ")";
        QMessageBox::information(this, "Connection Saved", 
            QString("Connection '%1' saved successfully.").arg(deviceName));
    }
}

void MainWindow::onImportConnections()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import Connections",
        QString(),
        "JSON Files (*.json);;All Files (*)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    
    ImportExportDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        bool merge = dialog.shouldMerge();
        
        if (m_connectionManager->importConnections(fileName, merge)) {
            m_connectionManager->saveToDefaultLocation();
            QMessageBox::information(this, "Import Successful", 
                QString("Connections imported successfully (%1).").arg(merge ? "merged" : "replaced"));
        } else {
            QMessageBox::critical(this, "Import Failed", 
                "Failed to import connections. Please check the file format.");
        }
    }
}

void MainWindow::onExportConnections()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Connections",
        "connections.json",
        "JSON Files (*.json);;All Files (*)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    if (m_connectionManager->exportConnections(fileName)) {
        QMessageBox::information(this, "Export Successful", 
            QString("Connections exported successfully to:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "Export Failed", 
            "Failed to export connections.");
    }
}

void MainWindow::onSavedConnectionDoubleClicked(const QString &name, const QString &host, int port)
{
    
    if (m_isConnected) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            "Already Connected",
            QString("You are currently connected to %1:%2.\n\nDo you want to disconnect and connect to '%3'?")
                .arg(m_hostEdit->text())
                .arg(m_portSpin->value())
                .arg(name),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            return;
        }
        
        
        m_connection->disconnect();
    }

    
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);

    
    qInfo() << "Connecting to saved connection:" << name << "(" << host << ":" << port << ")";
    onConnectClicked();
}


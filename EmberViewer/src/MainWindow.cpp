/*
    EmberViewer - Main application window implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "MainWindow.h"
#include "EmberConnection.h"
#include "EmberTreeWidget.h"
#include "ParameterDelegate.h"
#include "PathColumnDelegate.h"
#include "MatrixWidget.h"
#include "MeterWidget.h"
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

// Platform-specific update managers
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
{
    // Initialize connection manager and load saved connections early
    // (needed before createDockWindows to integrate into layout properly)
    m_connectionManager = new ConnectionManager(this);
    m_connectionManager->loadFromDefaultLocation();
    
    setupUi();
    setupMenu();
    setupStatusBar();
    createDockWindows();
    
    // Load saved settings
    loadSettings();
    
    // Create connection handler
    m_connection = new EmberConnection(this);
    connect(m_connection, &EmberConnection::connected, this, [this]() {
        onConnectionStateChanged(true);
    });
    connect(m_connection, &EmberConnection::disconnected, this, [this]() {
        onConnectionStateChanged(false);
    });
    
    // LAZY LOADING OPTIMIZATION: Use direct connections for instant UI updates
    // With lazy loading, we only receive small batches of children at a time,
    // so there's no risk of UI freeze. User sees real-time tree population.
    // (Previously disabled for aggressive enumeration with 100+ items at once)
    
    // Connect tree population signals (delegates to TreeViewController)
    connect(m_connection, &EmberConnection::nodeReceived, this, &MainWindow::onNodeReceived);
    connect(m_connection, &EmberConnection::parameterReceived, this, &MainWindow::onParameterReceived);
    connect(m_connection, &EmberConnection::streamValueReceived, this, &MainWindow::onStreamValueReceived);
    
    // Tree fetch progress now handled by SnapshotManager
    connect(m_connection, &EmberConnection::matrixReceived, this, &MainWindow::onMatrixReceived);
    connect(m_connection, &EmberConnection::matrixTargetReceived, this, &MainWindow::onMatrixTargetReceived);
    connect(m_connection, &EmberConnection::matrixSourceReceived, this, &MainWindow::onMatrixSourceReceived);
    connect(m_connection, &EmberConnection::matrixConnectionReceived, this, &MainWindow::onMatrixConnectionReceived);
    connect(m_connection, &EmberConnection::matrixConnectionsCleared, this, &MainWindow::onMatrixConnectionsCleared);
    connect(m_connection, &EmberConnection::matrixTargetConnectionsCleared, this, &MainWindow::onMatrixTargetConnectionsCleared);
    connect(m_connection, &EmberConnection::functionReceived, this, &MainWindow::onFunctionReceived);
    // Invocation results now handled by FunctionInvoker
    
    // Initialize manager classes
    m_treeViewController = new TreeViewController(m_treeWidget, m_connection, this);
    m_subscriptionManager = new SubscriptionManager(m_connection, this);
    m_matrixManager = new MatrixManager(m_connection, this);
    m_activityTracker = new CrosspointActivityTracker(m_crosspointsStatusLabel, this);
    m_functionInvoker = new FunctionInvoker(m_connection, this);
    m_snapshotManager = new SnapshotManager(m_treeWidget, m_connection, m_matrixManager, m_functionInvoker, this);
    
    // Install event filter on application to detect user activity (for crosspoint tracker)
    qApp->installEventFilter(m_activityTracker);
    
    // Connect manager signals
    connect(m_activityTracker, &CrosspointActivityTracker::timeout, this, &MainWindow::onActivityTimeout);
    
    // Connect tree signals directly to managers
    connect(m_treeWidget, &QTreeWidget::itemExpanded, m_treeViewController, &TreeViewController::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemExpanded, m_subscriptionManager, &SubscriptionManager::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, m_subscriptionManager, &SubscriptionManager::onItemCollapsed);
    
    setWindowTitle("EmberViewer - Ember+ Protocol Viewer");
    
    // Explicitly set window icon (especially important for Wayland)
    // This reinforces the application-level icon for this specific window
    QIcon windowIcon = QIcon::fromTheme("emberviewer", QIcon(":/icon.png"));
    setWindowIcon(windowIcon);
    
    // Initialize update manager (platform-specific)
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
        
        // Auto-check for updates on launch (2 second delay)
        QTimer::singleShot(2000, this, &MainWindow::onCheckForUpdates);
    }
    
    // Set default window size (1300x700) - will be overridden by saved settings if they exist
    resize(1300, 700);
    
    // Set splitter sizes after window is resized
    // Main splitter: 55% tree+console, 45% properties
    int leftWidth = width() * 0.55;
    int rightWidth = width() * 0.45;
    m_mainSplitter->setSizes(QList<int>() << leftWidth << rightWidth);
    
    // Vertical splitter: most space for tree, 150px for console
    int treeHeight = height() - 150;
    m_verticalSplitter->setSizes(QList<int>() << treeHeight << 150);
    
    // Log startup message directly to console since globalMainWindow isn't set yet during construction
    QString startupMsg = QString("[%1] EmberViewer started. Ready to connect.")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    appendToConsole(startupMsg);
    // Also log via qInfo for stderr and log file
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
    // Handle Enter key on port spin box to trigger connection
    if (watched == m_portSpin && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Only trigger if not already connected
            if (m_connectButton->isEnabled()) {
                onConnectClicked();
                return true;  // Event handled
            }
        }
    }
    
    // Reset activity timer on any user interaction (handled by activity tracker's eventFilter)
    // The activity tracker is installed on qApp and will handle this
    
    // Handle click on update status label
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
    // Create central widget with custom tree view (handles arrow clicks differently)
    m_treeWidget = new EmberTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    m_treeWidget->setColumnCount(3);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);  // Path column auto-resizes
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);  // Type column fixed
    m_treeWidget->setColumnWidth(1, 130);  // Type column width
    m_treeWidget->header()->setStretchLastSection(true);  // Value column stretches to fill
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // OPTIMIZATION: Remove expand/collapse delay for instant responsiveness
    // 1. autoExpandDelay: Default is ~1000ms to prevent accidental expansions during drag-and-drop
    //    We set to 0 for instant collapse/re-expand with no delay
    // 2. animated: Default animations add visual delay (250-300ms per expand/collapse)
    //    We disable for instant visual response
    // 3. uniformRowHeights: Tells Qt all rows are same height, allows faster rendering/layout
    //    Eliminates ~300ms delay after collapse before next expand is allowed
    // 4. allColumnsShowFocus: Improves click responsiveness by simplifying focus handling
    // Device load protection is handled by m_fetchedPaths and m_requestedPaths caching
    m_treeWidget->setAutoExpandDelay(0);
    m_treeWidget->setAnimated(false);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setAllColumnsShowFocus(false);
    
    // INVESTIGATION: Check system double-click interval
    int doubleClickInterval = QApplication::doubleClickInterval();
    qDebug() << "System double-click interval:" << doubleClickInterval << "ms";
    
    // Set comfortable double-click interval for item text (arrow clicks are instant via EmberTreeWidget)
    QApplication::setDoubleClickInterval(250);  // Comfortable timing for double-clicking items
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
    
    // Install path column delegate for extra padding
    PathColumnDelegate *pathDelegate = new PathColumnDelegate(this);
    m_treeWidget->setItemDelegateForColumn(0, pathDelegate);  // Path column
    
    // Install parameter delegate for inline editing
    ParameterDelegate *delegate = new ParameterDelegate(this);
    m_treeWidget->setItemDelegateForColumn(2, delegate);  // Value column
    
    // Enable editing on double-click
    m_treeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);
    
    // Connect delegate value changes to send parameter updates
    connect(delegate, &ParameterDelegate::valueChanged, this, [this](const QString &path, const QString &newValue) {
        // Find the item to get its type
        // Use cached item lookup O(log n) instead of O(n) iteration
        QTreeWidgetItem *item = m_treeViewController->findTreeItem(path);
        if (item) {
            int type = item->data(0, Qt::UserRole + 1).toInt();
            m_connection->sendParameterValue(path, newValue, type);
        }
    });
    
    // Connection controls
    QWidget *connectionWidget = new QWidget(this);
    QHBoxLayout *connLayout = new QHBoxLayout(connectionWidget);
    
    connLayout->addWidget(new QLabel("Host:"));
    m_hostEdit = new QLineEdit("localhost");
    m_hostEdit->setMaximumWidth(200);
    // Connect Enter key to trigger connection
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);
    connLayout->addWidget(m_hostEdit);
    
    connLayout->addWidget(new QLabel("Port:"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(DEFAULT_EMBER_PORT);
    m_portSpin->setMaximumWidth(80);
    // Install event filter to catch Enter key on spin box
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
    
    // Will be set up in createDockWindows() with splitters
    // Store connection widget for later
    QWidget *tempWidget = new QWidget(this);
    QVBoxLayout *tempLayout = new QVBoxLayout(tempWidget);
    tempLayout->addWidget(connectionWidget);
    tempLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create tree container with connection controls
    QWidget *treeContainer = new QWidget(this);
    QVBoxLayout *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->addWidget(connectionWidget);
    treeLayout->addWidget(m_treeWidget);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    
    // Store for use in createDockWindows
    setCentralWidget(treeContainer);
}

void MainWindow::setupMenu()
{
    // File menu
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
    
    // Edit menu
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    
    m_enableCrosspointsAction = editMenu->addAction("Enable &Crosspoints");
    m_enableCrosspointsAction->setShortcut(QKeySequence("Ctrl+E"));
    m_enableCrosspointsAction->setCheckable(true);
    m_enableCrosspointsAction->setChecked(false);
    connect(m_enableCrosspointsAction, &QAction::toggled, this, &MainWindow::onEnableCrosspointsToggled);
    
    // Tools menu
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
    
    // Help menu
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
            "<p><a href='https://github.com/magnusoverli/ember-plus'>github.com/magnusoverli/ember-plus</a></p>");
    });
}


void MainWindow::setupStatusBar()
{
    // Create path label on the left side of status bar (permanent widget)
    m_pathLabel = new QLabel("No selection");
    m_pathLabel->setStyleSheet("QLabel { padding: 2px 8px; }");
    m_pathLabel->setMinimumWidth(200);
    statusBar()->addPermanentWidget(m_pathLabel, 1);  // Stretch factor 1 to take available space
    
    // Add crosspoints status label (initially hidden)
    m_crosspointsStatusLabel = new QLabel("");
    m_crosspointsStatusLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #c62828; font-weight: bold; }");
    m_crosspointsStatusLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_crosspointsStatusLabel);
    
    // Add update status label (initially hidden)
    m_updateStatusLabel = new QLabel("");
    m_updateStatusLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #1976d2; cursor: pointer; }");
    m_updateStatusLabel->setVisible(false);
    m_updateStatusLabel->setCursor(Qt::PointingHandCursor);
    m_updateStatusLabel->installEventFilter(this);
    statusBar()->addPermanentWidget(m_updateStatusLabel);
    
    // Add toggle checkbox for path display mode
    QCheckBox *pathToggle = new QCheckBox("Show OID Path");
    pathToggle->setChecked(false);
    connect(pathToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_showOidPath = checked;
        onTreeSelectionChanged();  // Refresh display
    });
    statusBar()->addPermanentWidget(pathToggle);
    
    // No need for statusBar()->showMessage() - we use permanent widgets instead
}

void MainWindow::createDockWindows()
{
    // Get the tree container from central widget
    QWidget *treeContainer = centralWidget();
    
    // Create console group box
    m_consoleGroup = new QGroupBox("Console", this);
    QVBoxLayout *consoleLayout = new QVBoxLayout(m_consoleGroup);
    m_consoleLog = new QTextEdit();
    m_consoleLog->setReadOnly(true);
    consoleLayout->addWidget(m_consoleLog);
    consoleLayout->setContentsMargins(5, 5, 5, 5);
    
    // Create property group box
    m_propertyGroup = new QGroupBox("Properties", this);
    QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
    m_propertyPanel = new QWidget();
    QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
    propContentLayout->addWidget(new QLabel("Select an item to view properties"));
    propContentLayout->addStretch();
    propLayout->addWidget(m_propertyPanel);
    propLayout->setContentsMargins(5, 5, 5, 5);
    
    // Create vertical splitter (tree + console)
    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->addWidget(treeContainer);
    m_verticalSplitter->addWidget(m_consoleGroup);
    
    // Create main horizontal splitter (left side + properties)
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_verticalSplitter);
    m_mainSplitter->addWidget(m_propertyGroup);
    
    // Create connections tree widget (if connection manager is initialized)
    if (m_connectionManager) {
        m_connectionsTree = new ConnectionsTreeWidget(m_connectionManager, this);
        m_connectionsTree->setMinimumWidth(200);
        m_connectionsTree->setMaximumWidth(300);
        connect(m_connectionsTree, &ConnectionsTreeWidget::connectionDoubleClicked,
                this, &MainWindow::onSavedConnectionDoubleClicked);
        
        // Create outer splitter with connections tree on the left
        QSplitter *outerSplitter = new QSplitter(Qt::Horizontal, this);
        outerSplitter->addWidget(m_connectionsTree);
        outerSplitter->addWidget(m_mainSplitter);
        
        // Set the outer splitter as central widget (only one setCentralWidget call)
        setCentralWidget(outerSplitter);
    } else {
        // Fallback: just use main splitter if connection manager not initialized
        setCentralWidget(m_mainSplitter);
    }
}

void MainWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    // Don't clear console - keep history across connections
    // m_consoleLog->clear();
    
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
        
        // OPTIMIZATION: With lazy loading, tree updates are no longer disabled
        // We only receive small batches of children at a time (5-20 items max)
        // so there's no risk of UI freeze. User sees real-time tree population.
        // (Previously disabled for aggressive enumeration with 100+ items at once)
    } else {
        m_statusLabel->setText("Not connected");
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        qInfo().noquote() << "Disconnected";
        
        // If we're currently showing a matrix, remove it from the property panel first
        MatrixWidget *currentMatrix = qobject_cast<MatrixWidget*>(m_propertyPanel);
        if (currentMatrix) {
            // Remove old layout and widget
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            // Reset to default property panel
            m_propertyPanel = new QWidget();
            QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
            propContentLayout->addWidget(new QLabel("Not connected"));
            propContentLayout->addStretch();
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(m_propertyPanel);
            propLayout->setContentsMargins(5, 5, 5, 5);
        }
        
        m_treeWidget->clear();
        
        // Clear all manager state
        m_treeViewController->clear();
        m_subscriptionManager->clear();
        m_matrixManager->clear();
    }
}

void MainWindow::onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline)
{
    // Delegate to TreeViewController
    m_treeViewController->onNodeReceived(path, identifier, description, isOnline);
}

void MainWindow::onParameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier)
{
    // Track stream identifiers for StreamCollection routing (used in onStreamValueReceived)
    if (streamIdentifier > 0) {
        m_streamIdToPath[streamIdentifier] = path;
    }
    
    // Check if this parameter is actually a matrix label
    // Pattern: matrixPath.666999666.1.N (targets) or matrixPath.666999666.2.N (sources)
    QStringList pathParts = path.split('.');
    if (pathParts.size() >= 4 && pathParts[pathParts.size() - 3] == QString::number(MATRIX_LABEL_PATH_MARKER)) {
        // This is a label! Extract matrix path and target/source info
        QString labelType = pathParts[pathParts.size() - 2];  // "1" for targets, "2" for sources
        int labelNumber = pathParts.last().toInt();
        
        // Matrix path is everything except the last 3 segments
        QStringList matrixPathParts = pathParts.mid(0, pathParts.size() - 3);
        QString matrixPath = matrixPathParts.join('.');
        
        // Route label to MatrixManager
        if (labelType == "1") {
            // Target label
            m_matrixManager->onMatrixTargetReceived(matrixPath, labelNumber, value);
        } else if (labelType == "2") {
            // Source label
            m_matrixManager->onMatrixSourceReceived(matrixPath, labelNumber, value);
        }
        
        // Still create the tree item for the label (for completeness)
    }
    
    // Delegate to TreeViewController for tree item creation
    m_treeViewController->onParameterReceived(path, number, identifier, value, access, type, 
                                             minimum, maximum, enumOptions, enumValues, isOnline, streamIdentifier);
}

void MainWindow::onMatrixReceived(const QString &path, int number, const QString &identifier, 
                                   const QString &description, int type, int targetCount, int sourceCount)
{
    // Delegate to TreeViewController for tree item creation
    m_treeViewController->onMatrixReceived(path, number, identifier, description, type, targetCount, sourceCount);
    
    // Delegate to MatrixManager for widget management  
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
void MainWindow::onTreeSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    
    if (selected.isEmpty()) {
        m_pathLabel->setText("No selection");
        // Disable crosspoints when nothing is selected
        if (m_activityTracker && m_activityTracker->isEnabled()) {
            m_enableCrosspointsAction->setChecked(false);
        }
        return;
    }
    
    QTreeWidgetItem *item = selected.first();
    QString oidPath = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    // Update path label
    if (m_showOidPath) {
        // Show OID path mode
        if (!oidPath.isEmpty()) {
            m_pathLabel->setText(QString("%1  [%2]").arg(oidPath).arg(type));
        } else {
            m_pathLabel->setText(QString("(no path)  [%1]").arg(type));
        }
    } else {
        // Show breadcrumb path mode
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
    
    // Update property panel based on type
    if (type == "Parameter") {
        // Check if this is a meter parameter (must have streamIdentifier > 0)
        int streamIdentifier = item->data(0, Qt::UserRole + 9).toInt();
        int paramType = item->data(0, Qt::UserRole + 1).toInt();
        bool isAudioMeter = (streamIdentifier > 0) && (paramType == 1 || paramType == 2);
        
        if (isAudioMeter) {
            // This is an audio meter parameter - show meter widget
            
            // Clean up old meter
            if (m_activeMeter) {
                m_activeMeter = nullptr;  // Will be deleted with old panel
            }
            
            // Disable crosspoints if active
            if (m_activityTracker && m_activityTracker->isEnabled()) {
                m_enableCrosspointsAction->setChecked(false);
            }
            
            // Remove old layout
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *layoutItem;
                while ((layoutItem = oldLayout->takeAt(0)) != nullptr) {
                    delete layoutItem;
                }
                delete oldLayout;
            }
            
            // Create meter widget
            m_activeMeter = new MeterWidget();
            
            // Get parameter info from item data
            QString identifier = item->text(0).replace("📊 ", "");  // Remove icon
            QVariant minVar = item->data(0, Qt::UserRole + 3);
            QVariant maxVar = item->data(0, Qt::UserRole + 4);
            
            double minValue = minVar.isValid() ? minVar.toDouble() : 0.0;
            double maxValue = maxVar.isValid() ? maxVar.toDouble() : 100.0;
            
            m_activeMeter->setParameterInfo(identifier, oidPath, minValue, maxValue);
            m_activeMeter->setStreamIdentifier(streamIdentifier);
            
            // Set initial value from current parameter value
            QString currentValue = item->text(2);
            bool ok;
            double val = currentValue.toDouble(&ok);
            if (ok) {
                m_activeMeter->updateValue(val);
            }
            
            // Create layout with meter and parameter properties
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(m_activeMeter);
            
            // Add parameter info section
            QLabel *infoLabel = new QLabel(QString("Path: %1\nMin: %2\nMax: %3\nStream ID: %4")
                .arg(oidPath).arg(minValue).arg(maxValue).arg(streamIdentifier));
            infoLabel->setStyleSheet("padding: 10px; background-color: #f0f0f0; border-radius: 5px;");
            propLayout->addWidget(infoLabel);
            
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = m_activeMeter;
        }
    }
    else if (type == "Matrix") {
        // Show matrix widget in property panel
        MatrixWidget *matrixWidget = m_matrixManager->getMatrix(oidPath);
        if (matrixWidget) {
            // Rebuild the grid to ensure it's up to date with all labels and connections
            matrixWidget->rebuild();
            
            // Clear old property panel content
            QWidget *oldWidget = m_propertyPanel;
            if (oldWidget) {
                // If the old widget is a MatrixWidget, don't delete it (it's stored in m_matrixWidgets)
                MatrixWidget *oldMatrix = qobject_cast<MatrixWidget*>(oldWidget);
                if (!oldMatrix) {
                    // Only delete non-matrix widgets
                    oldWidget->deleteLater();
                }
            }
            
            // Remove old layout
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            // Directly show the matrix widget (it has internal scroll areas now)
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(matrixWidget);
            propLayout->setContentsMargins(5, 5, 5, 5);
            m_propertyPanel = matrixWidget;
            
            // Update background based on crosspoints state
        }
    } else {
        // For non-matrix items, show the default property panel
        // Check if we need to restore the default panel
        MatrixWidget *currentMatrix = qobject_cast<MatrixWidget*>(m_propertyPanel);
        if (currentMatrix) {
            // Disable crosspoints BEFORE we remove the matrix widget
            if (m_activityTracker && m_activityTracker->isEnabled()) {
                m_enableCrosspointsAction->setChecked(false);
            }
            
            // Remove old layout
            QLayout *oldLayout = m_propertyGroup->layout();
            if (oldLayout) {
                QLayoutItem *item;
                while ((item = oldLayout->takeAt(0)) != nullptr) {
                    delete item;
                }
                delete oldLayout;
            }
            
            // Create new default property panel
            m_propertyPanel = new QWidget();
            QVBoxLayout *propContentLayout = new QVBoxLayout(m_propertyPanel);
            propContentLayout->addWidget(new QLabel("Select an item to view properties"));
            propContentLayout->addStretch();
            
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyGroup);
            propLayout->addWidget(m_propertyPanel);
            propLayout->setContentsMargins(5, 5, 5, 5);
        }
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    // Load connection settings only
    QString host = settings.value("connection/host", "localhost").toString();
    int port = settings.value("connection/port", DEFAULT_PORT_FALLBACK).toInt();
    
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);
}

void MainWindow::saveSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    // Save connection settings only (don't save window geometry or dock state)
    settings.setValue("connection/host", m_hostEdit->text());
    settings.setValue("connection/port", m_portSpin->value());
    
    // Force immediate write to disk
    settings.sync();
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
    // Timeout reached - disable crosspoints
    m_enableCrosspointsAction->setChecked(false);
    logMessage("Crosspoint editing auto-disabled after 60 seconds of inactivity");
}

void MainWindow::onEnableCrosspointsToggled(bool enabled)
{
    if (enabled) {
        // Enable crosspoint editing via activity tracker
        m_activityTracker->enable();
        logMessage("Crosspoint editing ENABLED (60 second timeout)");
    } else {
        // Disable crosspoint editing via activity tracker
        m_activityTracker->disable();
        logMessage("Crosspoint editing DISABLED");
    }
    
    // Update only the currently displayed matrix widget (if any)
    MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(m_propertyPanel);
    if (matrixWidget) {
        matrixWidget->setCrosspointsEnabled(enabled);
    }
}



void MainWindow::onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber)
{
    if (!m_activityTracker || !m_activityTracker->isEnabled()) {
        qDebug().noquote() << "Crosspoint click ignored - crosspoints not enabled";
        return;
    }
    
    // Reset activity timer on crosspoint interaction
    m_activityTracker->resetTimer();
    
    MatrixWidget *matrixWidget = m_matrixManager->getMatrix(matrixPath);
    if (!matrixWidget) {
        qWarning().noquote() << "Matrix widget not found for path: " + matrixPath;
        return;
    }
    
    bool currentlyConnected = matrixWidget->isConnected(targetNumber, sourceNumber);
    int matrixType = matrixWidget->getMatrixType();
    
    QString typeStr;
    if (matrixType == 0) typeStr = "1:N";
    else if (matrixType == 1) typeStr = "1:1";
    else typeStr = "N:N";
    
    // For all matrix types, clicking toggles the connection
    // The Ember+ device will handle the matrix type rules (disconnecting other sources, etc.)
    bool newState = !currentlyConnected;
    
    // Get labels for more readable log messages
    QString targetLabel = matrixWidget->getTargetLabel(targetNumber);
    QString sourceLabel = matrixWidget->getSourceLabel(sourceNumber);
    
    if (newState) {
        qInfo().noquote() << QString("Crosspoint CONNECT: %1 [%2] ← %3 [%4]")
                   .arg(targetLabel, QString::number(targetNumber), sourceLabel, QString::number(sourceNumber));
    } else {
        qInfo().noquote() << QString("Crosspoint DISCONNECT: %1 [%2]")
                   .arg(targetLabel, QString::number(targetNumber));
    }
    
    // **OPTIMISTIC UPDATE**: Update UI immediately for instant feedback (use Pending disposition)
    matrixWidget->setConnection(targetNumber, sourceNumber, newState, 2);
    
    // Send command to EmberConnection
    m_connection->setMatrixConnection(matrixPath, targetNumber, sourceNumber, newState);
    
    // Note: With subscription enabled, we'll receive automatic updates from the provider
    // No need to manually request matrix connections anymore
}

void MainWindow::onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                                   const QStringList &argNames, const QList<int> &argTypes,
                                   const QStringList &resultNames, const QList<int> &resultTypes)
{
    // Register function with FunctionInvoker
    m_functionInvoker->registerFunction(path, identifier, description, argNames, argTypes, resultNames, resultTypes);
    
    // Delegate tree item creation to TreeViewController
    m_treeViewController->onFunctionReceived(path, identifier, description, argNames, argTypes, resultNames, resultTypes);
}

// onInvocationResultReceived now handled by FunctionInvoker

void MainWindow::onStreamValueReceived(int streamIdentifier, double value)
{
    // Handle StreamCollection updates - route to active meter if it matches
    if (m_activeMeter && m_activeMeter->streamIdentifier() == streamIdentifier) {
        m_activeMeter->updateValue(value);
        
        // Also update the tree item value for consistency
        QString path = m_streamIdToPath.value(streamIdentifier);
        if (!path.isEmpty()) {
            QTreeWidgetItem *item = m_treeViewController->findTreeItem(path);
            if (item) {
                item->setText(2, QString::number(value, 'f', 2));
            }
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
    
    // Delegate to SnapshotManager
    m_snapshotManager->saveSnapshot(m_hostEdit, m_portSpin);
}



void MainWindow::onOpenEmulator()
{
    if (!m_emulatorWindow) {
        // Create as independent window (no parent) so it doesn't stay on top
        m_emulatorWindow = new EmulatorWindow(nullptr);
        
        // Connect destroyed signal to clean up pointer
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
    
    // Show notification in status bar
    m_updateStatusLabel->setText(QString("⬇ Update to v%1 available - Click to install").arg(updateInfo.version));
    m_updateStatusLabel->setVisible(true);
    
    // Create update dialog (don't show immediately, user can click status bar)
    if (m_updateDialog) {
        delete m_updateDialog;
    }
    
    m_updateDialog = new UpdateDialog(updateInfo, this);
    
    // Connect update dialog signals
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
    
    // Connect installation progress signals
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
    
    // Show dialog immediately (auto-check scenario)
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

    // Get device name from first root node
    QString deviceName;
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstRoot = m_treeWidget->topLevelItem(0);
        deviceName = firstRoot->text(0);
    }

    // Fall back to hostname if no device name
    if (deviceName.isEmpty()) {
        deviceName = m_hostEdit->text();
    }

    QString host = m_hostEdit->text();
    int port = m_portSpin->value();

    // Ask where to save it (which folder)
    QStringList folderNames;
    folderNames.append("(Root Level)");  // Option for root
    
    QList<ConnectionManager::Folder> folders = m_connectionManager->getAllFolders();
    QMap<QString, QString> folderNameToId;  // name -> id
    folderNameToId["(Root Level)"] = QString();  // Empty string = root
    
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

    // Ask merge or replace
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
    // Check if already connected
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
        
        // Disconnect first
        m_connection->disconnect();
    }

    // Set connection parameters
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);

    // Connect
    qInfo() << "Connecting to saved connection:" << name << "(" << host << ":" << port << ")";
    onConnectClicked();
}


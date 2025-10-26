/*
    EmberViewer - Main application window implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "MainWindow.h"
#include "EmberConnection.h"
#include "ParameterDelegate.h"
#include "PathColumnDelegate.h"
#include "MatrixWidget.h"
#include "FunctionInvocationDialog.h"
#include "DeviceSnapshot.h"
#include "EmulatorWindow.h"
#include "version.h"
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
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
    , m_enableCrosspointsAction(nullptr)
    , m_activityTimer(nullptr)
    , m_tickTimer(nullptr)
    , m_activityTimeRemaining(0)
    , m_crosspointsStatusLabel(nullptr)
    , m_emulatorWindow(nullptr)
    , m_isConnected(false)
    , m_showOidPath(false)
    , m_crosspointsEnabled(false)
{
    setupUi();
    setupMenu();
    setupStatusBar();
    createDockWindows();
    
    // Install event filter on application to detect user activity
    qApp->installEventFilter(this);
    
    // Setup activity timer (60 second timeout)
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(true);
    m_activityTimer->setInterval(ACTIVITY_TIMEOUT_MS);
    connect(m_activityTimer, &QTimer::timeout, this, &MainWindow::onActivityTimeout);
    
    // Setup tick timer (1 second updates for status bar)
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(TICK_INTERVAL_MS); // 1 second
    connect(m_tickTimer, &QTimer::timeout, this, &MainWindow::onActivityTimerTick);
    
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
    connect(m_connection, &EmberConnection::logMessage, this, &MainWindow::onLogMessage);
    connect(m_connection, &EmberConnection::treePopulated, this, &MainWindow::subscribeToExpandedItems);
    connect(m_connection, &EmberConnection::nodeReceived, this, &MainWindow::onNodeReceived);
    connect(m_connection, &EmberConnection::parameterReceived, this, &MainWindow::onParameterReceived);
    connect(m_connection, &EmberConnection::matrixReceived, this, &MainWindow::onMatrixReceived);
    connect(m_connection, &EmberConnection::matrixTargetReceived, this, &MainWindow::onMatrixTargetReceived);
    connect(m_connection, &EmberConnection::matrixSourceReceived, this, &MainWindow::onMatrixSourceReceived);
    connect(m_connection, &EmberConnection::matrixConnectionReceived, this, &MainWindow::onMatrixConnectionReceived);
    connect(m_connection, &EmberConnection::matrixConnectionsCleared, this, &MainWindow::onMatrixConnectionsCleared);
    connect(m_connection, &EmberConnection::matrixTargetConnectionsCleared, this, &MainWindow::onMatrixTargetConnectionsCleared);
    connect(m_connection, &EmberConnection::functionReceived, this, &MainWindow::onFunctionReceived);
    connect(m_connection, &EmberConnection::invocationResultReceived, this, &MainWindow::onInvocationResultReceived);
    
    setWindowTitle("EmberViewer - Ember+ Protocol Viewer");
    
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
    
    logMessage("EmberViewer started. Ready to connect.");
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
    
    // Reset activity timer on any user interaction
    if (m_crosspointsEnabled) {
        QEvent::Type type = event->type();
        if (type == QEvent::MouseButtonPress ||
            type == QEvent::MouseButtonRelease ||
            type == QEvent::MouseMove ||
            type == QEvent::KeyPress ||
            type == QEvent::KeyRelease ||
            type == QEvent::Wheel) {
            resetActivityTimer();
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi()
{
    // Create central widget with tree view
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    m_treeWidget->setColumnCount(3);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);  // Path column auto-resizes
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);  // Type column fixed
    m_treeWidget->setColumnWidth(1, 130);  // Type column width
    m_treeWidget->header()->setStretchLastSection(true);  // Value column stretches to fill
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, &MainWindow::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, this, &MainWindow::onItemCollapsed);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
        if (!item) return;
        
        QString type = item->text(1);
        if (type == "Function") {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!m_functions.contains(path)) return;
            
            QMenu contextMenu;
            QAction *invokeAction = contextMenu.addAction("Invoke Function...");
            
            QAction *selected = contextMenu.exec(m_treeWidget->mapToGlobal(pos));
            if (selected == invokeAction) {
                FunctionInfo funcInfo = m_functions[path];
                
                FunctionInvocationDialog dialog(funcInfo.identifier, funcInfo.description,
                                               funcInfo.argNames, funcInfo.argTypes, this);
                
                if (dialog.exec() == QDialog::Accepted) {
                    QList<QVariant> arguments = dialog.getArguments();
                    
                    int invocationId = m_connection->property("nextInvocationId").toInt();
                    m_pendingInvocations[invocationId] = path;
                    
                    m_connection->invokeFunction(path, arguments);
                    
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
        QTreeWidgetItemIterator it(m_treeWidget);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toString() == path) {
                int type = (*it)->data(0, Qt::UserRole + 1).toInt();  // TypeRole
                m_connection->sendParameterValue(path, newValue, type);
                break;
            }
            ++it;
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
    
    QAction *saveDeviceAction = fileMenu->addAction("&Save Device Snapshot...");
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
    
    // Set the main splitter as central widget
    setCentralWidget(m_mainSplitter);
}

void MainWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    // Clear console log from previous session
    m_consoleLog->clear();
    
    qDebug().noquote() << QString("Connecting to %1:%2...").arg(host).arg(port);
    m_connection->connectToHost(host, port);
}

void MainWindow::onDisconnectClicked()
{
    qDebug().noquote() << "Disconnecting...";
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
        
        // Clear the path cache
        m_pathToItem.clear();
        
        // Clear subscription tracking
        m_subscribedPaths.clear();
        m_subscriptionStates.clear();
        
        // Now it's safe to delete all matrix widgets
        qDeleteAll(m_matrixWidgets);
        m_matrixWidgets.clear();
    }
}

void MainWindow::onLogMessage(const QString &message)
{
    logMessage(message);
}

void MainWindow::onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new node (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        // For nodes: prefer description as display name (often more readable than UUID identifier)
        // If no description, fall back to identifier
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Node");
        item->setText(2, "");  // Keep Value column empty for nodes
        
        // Store isOnline state in item data (UserRole + 4 for nodes)
        // Note: Parameters use UserRole + 8 to avoid collision with parameter metadata
        item->setData(0, Qt::UserRole + 4, isOnline);
        
        // Apply offline styling if needed
        if (!isOnline) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
            item->setForeground(0, QBrush(QColor("#888888")));
            item->setForeground(1, QBrush(QColor("#888888")));
            item->setForeground(2, QBrush(QColor("#888888")));
            item->setToolTip(0, QString("%1 - Offline").arg(displayName));
        } else {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            // Reset foreground to default
            item->setForeground(0, QBrush());
            item->setForeground(1, QBrush());
            item->setForeground(2, QBrush());
            item->setToolTip(0, "");
        }
        
        if (isNew) {
            qDebug().noquote() << QString("Node: %1 [%2] - %3")
                .arg(displayName).arg(path).arg(isOnline ? "Online" : "Offline");
        }
    }
}

void MainWindow::onParameterReceived(const QString &path, int /* number */, const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline)
{
    // Check if this parameter is actually a matrix label
    // Pattern: matrixPath.666999666.1.N (targets) or matrixPath.666999666.2.N (sources)
    QStringList pathParts = path.split('.');
    if (pathParts.size() >= 4 && pathParts[pathParts.size() - 3] == QString::number(MATRIX_LABEL_PATH_MARKER)) {
        // This is a label! Extract matrix path and target/source info
        QString labelType = pathParts[pathParts.size() - 2];  // "1" for targets, "2" for sources
        int number = pathParts.last().toInt();
        
        // Matrix path is everything except the last 3 segments
        QStringList matrixPathParts = pathParts.mid(0, pathParts.size() - 3);
        QString matrixPath = matrixPathParts.join('.');
        
        MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
        if (matrixWidget) {
            if (labelType == "1") {
                // Target label
                Q_ASSERT(matrixWidget != nullptr);
                matrixWidget->setTargetLabel(number, value);
            } else if (labelType == "2") {
                // Source label
                Q_ASSERT(matrixWidget != nullptr);
                matrixWidget->setSourceLabel(number, value);
            }
        }
        
        // Still create the tree item for the label (for completeness)
    }
    
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new parameter (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        setItemDisplayName(item, identifier);
        item->setText(1, "Parameter");
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        
        // Store parameter metadata in item for delegate access
        // Using custom roles (Qt::UserRole + N)
        item->setData(0, Qt::UserRole, path);          // Path (for sending updates)
        item->setData(0, Qt::UserRole + 1, type);       // TypeRole
        item->setData(0, Qt::UserRole + 2, access);     // AccessRole
        item->setData(0, Qt::UserRole + 3, minimum);    // MinimumRole
        item->setData(0, Qt::UserRole + 4, maximum);    // MaximumRole
        item->setData(0, Qt::UserRole + 5, enumOptions); // EnumOptionsRole
        
        // Convert QList<int> to QList<QVariant> for storage
        QList<QVariant> enumValuesVar;
        for (int val : enumValues) {
            enumValuesVar.append(val);
        }
        item->setData(0, Qt::UserRole + 6, QVariant::fromValue(enumValuesVar)); // EnumValuesRole
        item->setData(0, Qt::UserRole + 8, isOnline);   // IsOnlineRole (after Matrix's UserRole + 7)
        
        // Check if parameter is editable
        // Access: 0=None, 1=ReadOnly, 2=WriteOnly, 3=ReadWrite
        bool isEditable = (access == 2 || access == 3);  // WriteOnly or ReadWrite
        
        // Determine effective type
        int effectiveType = type;
        
        // Check if widget already exists - if so, preserve its type
        QWidget *existingWidget = m_treeWidget->itemWidget(item, 2);
        QComboBox *existingCombo = qobject_cast<QComboBox*>(existingWidget);
        QPushButton *existingButton = qobject_cast<QPushButton*>(existingWidget);
        
        if (existingCombo) {
            // Combobox exists - it's either Boolean or Enum, preserve the widget
            // Infer type from combobox contents if type is None
            if (effectiveType == 0) {
                if (existingCombo->count() == 2 && existingCombo->itemText(0) == "false") {
                    effectiveType = 4;  // Boolean
                    qDebug().noquote() << QString("Preserving Boolean widget for %1 (type was 0)").arg(path);
                } else {
                    effectiveType = 6;  // Enum
                    qDebug().noquote() << QString("Preserving Enum widget for %1 (type was 0)").arg(path);
                }
            } else {
                qDebug().noquote() << QString("Widget exists for %1, type=%2").arg(path).arg(effectiveType);
            }
        } else if (existingButton) {
            // Button exists - it's a Trigger
            if (effectiveType == 0) {
                effectiveType = 5;  // Trigger
            }
        } else {
            // No widget exists - infer type if not set (Type=0 means None)
            if (effectiveType == 0) {
                QString lowerValue = value.toLower();
                if (lowerValue == "true" || lowerValue == "false") {
                    effectiveType = 4;  // Boolean
                    qDebug().noquote() << QString("No widget for %1, inferring Boolean from value '%2'").arg(path).arg(value);
                } else {
                    qDebug().noquote() << QString("No widget for %1, type=0, value='%2' (not boolean)").arg(path).arg(value);
                }
            }
        }
        
        // HYBRID APPROACH: Use persistent widgets for Boolean/Enum/Trigger, delegates for others
        if (effectiveType == 5) {
            // Trigger: Show button (regardless of access - triggers are actions, not values)
            
            // Remove any existing widget first
            QWidget *existingWidget = m_treeWidget->itemWidget(item, 2);
            if (existingWidget) {
                m_treeWidget->removeItemWidget(item, 2);
                existingWidget->deleteLater();
            }
            
            QPushButton *button = new QPushButton("Trigger");
            button->setMaximumWidth(100);
            
            connect(button, &QPushButton::clicked, this, [this, path]() {
                qInfo().noquote() << QString("Trigger invoked: %1").arg(path);
                // For triggers, send an empty or null value (convention varies)
                m_connection->sendParameterValue(path, "", 5);  // Type: Trigger
            });
            
            // Set the widget in the tree
            m_treeWidget->setItemWidget(item, 2, button);
            
            // Clear text (widget takes precedence)
            item->setText(2, "");
            
            // Disable delegate editing for this item (button handles it)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            
        } else if (isEditable && ((effectiveType == 4) || (effectiveType == 6 && !enumOptions.isEmpty()))) {
            // Boolean or Enum: Use persistent dropdown widget (only for editable parameters)
            // Read-only booleans/enums will be shown as text below
            
            // Re-use the existing widget check from above
            QComboBox *combo = existingCombo;
            
            if (combo) {
                // Widget exists - just update the value
                combo->blockSignals(true);
                
                if (effectiveType == 4) {
                    // Boolean - update selection
                    combo->setCurrentText(value.toLower());
                } else if (effectiveType == 6) {
                    // Enum - find and set current value
                    int currentIdx = -1;
                    for (int i = 0; i < enumOptions.size(); i++) {
                        if (enumOptions[i] == value) {
                            currentIdx = i;
                            break;
                        }
                    }
                    if (currentIdx >= 0) {
                        combo->setCurrentIndex(currentIdx);
                    }
                }
                
                combo->blockSignals(false);
                
            } else {
                // Widget doesn't exist - create new one
                
                // Remove any non-combobox widget first
                if (existingWidget) {
                    m_treeWidget->removeItemWidget(item, 2);
                    existingWidget->deleteLater();
                }
                
                combo = new QComboBox();
                combo->setFrame(false);  // Frameless for cleaner look
                
                if (effectiveType == 4) {
                    // Boolean parameter
                    combo->addItem("false");
                    combo->addItem("true");
                    
                    // Set initial value WITHOUT triggering signal
                    combo->blockSignals(true);
                    combo->setCurrentText(value.toLower());
                    combo->blockSignals(false);
                    
                    // Connect AFTER setting initial value
                    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, path, combo](int) {
                        QString newValue = combo->currentText();
                        qDebug().noquote() << QString("Boolean changed: %1 → %2").arg(path).arg(newValue);
                        m_connection->sendParameterValue(path, newValue, 4);  // Type: Boolean
                    });
                } else if (effectiveType == 6) {
                    // Enum parameter
                    combo->addItems(enumOptions);
                    
                    // Find current value in enum options
                    int currentIdx = -1;
                    for (int i = 0; i < enumOptions.size(); i++) {
                        if (enumOptions[i] == value) {
                            currentIdx = i;
                            break;
                        }
                    }
                    
                    // Set initial value WITHOUT triggering signal
                    if (currentIdx >= 0) {
                        combo->blockSignals(true);
                        combo->setCurrentIndex(currentIdx);
                        combo->blockSignals(false);
                    }
                    
                    // Connect AFTER setting initial value
                    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, path, enumValues, combo](int idx) {
                        if (idx >= 0 && idx < enumValues.size()) {
                            QString newValue = QString::number(enumValues[idx]);
                            QString displayValue = combo->currentText();
                            qDebug().noquote() << QString("Enum changed: %1 → '%2' (value %3)").arg(path).arg(displayValue).arg(newValue);
                            m_connection->sendParameterValue(path, newValue, 6);  // Type: Enum
                        }
                    });
                }
                
                // Set the widget in the tree
                m_treeWidget->setItemWidget(item, 2, combo);
                
                // Clear text and reset color (widget takes precedence)
                item->setText(2, "");
                item->setForeground(2, QBrush(palette().color(QPalette::Text)));  // Reset to default color
                
                // Disable delegate editing for this item (widget handles it)
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            
        } else {
            // Integer/Real/String/ReadOnly: Use text + delegate (double-click to edit)
            
            // Remove any existing widget
            QWidget *existingWidget = m_treeWidget->itemWidget(item, 2);
            if (existingWidget) {
                qDebug().noquote() << QString("REMOVING WIDGET for %1 (fell through to text branch, type=%2, access=%3)").arg(path).arg(effectiveType).arg(access);
                m_treeWidget->removeItemWidget(item, 2);
                existingWidget->deleteLater();
            }
            
            item->setText(2, value);
            
            if (isEditable) {
                item->setForeground(2, QBrush(QColor(30, 144, 255)));  // Bright blue (DodgerBlue)
                // Enable delegate editing for the value column
                item->setFlags(item->flags() | Qt::ItemIsEditable);
            } else {
                item->setForeground(2, QBrush(palette().color(QPalette::Text)));  // Default color
                // Disable editing for read-only parameters
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
        
        if (isNew) {
            qDebug().noquote() << QString("Parameter: %1 = %2 [%3] (Type: %4, Access: %5)").arg(identifier).arg(value).arg(path).arg(type).arg(access);
        }
    }
}

void MainWindow::onMatrixReceived(const QString &path, int /* number */, const QString &identifier, 
                                   const QString &description, int type, int targetCount, int sourceCount)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        bool isNew = item->text(1).isEmpty();
        
        // For matrices: prefer description as display name (like nodes)
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Matrix");
        item->setText(2, QString("%1×%2").arg(sourceCount).arg(targetCount));
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        
        // Store matrix path in user data for later retrieval
        item->setData(0, Qt::UserRole, path);
        item->setData(0, Qt::UserRole + 7, "Matrix");  // Store type as string for onTreeSelectionChanged
        
        // Create or update the matrix widget
        MatrixWidget *matrixWidget = m_matrixWidgets.value(path, nullptr);
        if (!matrixWidget) {
            matrixWidget = new MatrixWidget(this);
            m_matrixWidgets[path] = matrixWidget;
            matrixWidget->setMatrixPath(path);
            
            connect(matrixWidget, &MatrixWidget::crosspointClicked,
                    this, &MainWindow::onCrosspointClicked);
            connect(matrixWidget, &MatrixWidget::crosspointToggleRequested,
                    this, [this]() {
                m_enableCrosspointsAction->trigger();
            });
            
            // Auto-subscribe to matrix for connection updates
            if (m_isConnected && !m_subscribedPaths.contains(path)) {
                m_connection->subscribeToMatrix(path, true);
                m_subscribedPaths.insert(path);
                SubscriptionState state;
                state.subscribedAt = QDateTime::currentDateTime();
                state.autoSubscribed = true;
                m_subscriptionStates[path] = state;
            }
        }
        
        matrixWidget->setMatrixInfo(identifier, description, type, targetCount, sourceCount);
        
        if (isNew) {
            qInfo().noquote() << QString("Matrix discovered: %1 (%2×%3)").arg(displayName).arg(sourceCount).arg(targetCount);
        }
    }
}

void MainWindow::onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label)
{
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->setTargetLabel(targetNumber, label);
        // Don't rebuild for every label - too expensive and destroys button map
        // The grid will be built when the matrix is first displayed
    }
}

void MainWindow::onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label)
{
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->setSourceLabel(sourceNumber, label);
        // Don't rebuild for every label - too expensive and destroys button map
        // The grid will be built when the matrix is first displayed
    }
}

void MainWindow::onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition)
{
    QString dispositionStr;
    switch (disposition) {
        case 0: dispositionStr = "Tally"; break;
        case 1: dispositionStr = "Modified"; break;
        case 2: dispositionStr = "Pending"; break;
        case 3: dispositionStr = "Locked"; break;
        default: dispositionStr = QString("Unknown(%1)").arg(disposition); break;
    }
    
    qDebug().noquote() << QString("Connection received - Matrix [%1], Target %2, Source %3, Connected: %4, Disposition: %5")
               .arg(matrixPath).arg(targetNumber).arg(sourceNumber).arg(connected ? "YES" : "NO").arg(dispositionStr);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        qDebug().noquote() << QString("Found matrix widget, calling setConnection()");
        matrixWidget->setConnection(targetNumber, sourceNumber, connected, disposition);
    } else {
        qWarning().noquote() << QString("No matrix widget found for path [%1]").arg(matrixPath);
    }
}

void MainWindow::onMatrixConnectionsCleared(const QString &matrixPath)
{
    qDebug().noquote() << QString("Clearing all connections for matrix %1").arg(matrixPath);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->clearConnections();
        qDebug().noquote() << QString("Connections cleared for matrix %1").arg(matrixPath);
    }
}

void MainWindow::onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber)
{
    qDebug().noquote() << QString("Clearing connections for target %1 in matrix %2").arg(targetNumber).arg(matrixPath);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->clearTargetConnections(targetNumber);
        qDebug().noquote() << QString("Target %1 connections cleared for matrix %2").arg(targetNumber).arg(matrixPath);
    }
}

QTreeWidgetItem* MainWindow::findOrCreateTreeItem(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    
    // Check cache first for O(1) lookup
    if (m_pathToItem.contains(path)) {
        return m_pathToItem[path];
    }
    
    // Split path into segments
    QStringList pathSegments = path.split('.', Qt::SkipEmptyParts);
    
    QTreeWidgetItem *parent = nullptr;
    QString currentPath;
    
    // Walk through the tree, creating missing nodes
    for (int i = 0; i < pathSegments.size(); ++i) {
        if (!currentPath.isEmpty()) {
            currentPath += ".";
        }
        currentPath += pathSegments[i];
        
        // Check cache for this intermediate path
        QTreeWidgetItem *found = m_pathToItem.value(currentPath, nullptr);
        
        // If not in cache, search for it
        if (!found) {
            if (parent == nullptr) {
                // Search in root level
                for (int j = 0; j < m_treeWidget->topLevelItemCount(); ++j) {
                    QTreeWidgetItem *topItem = m_treeWidget->topLevelItem(j);
                    if (topItem->data(0, Qt::UserRole).toString() == currentPath) {
                        found = topItem;
                        break;
                    }
                }
            } else {
                // Search in children of parent
                for (int j = 0; j < parent->childCount(); ++j) {
                    QTreeWidgetItem *childItem = parent->child(j);
                    if (childItem->data(0, Qt::UserRole).toString() == currentPath) {
                        found = childItem;
                        break;
                    }
                }
            }
        }
        
        // If not found, create it
        if (!found) {
            QStringList columns;
            columns << pathSegments[i] << "" << "";
            
            if (parent == nullptr) {
                found = new QTreeWidgetItem(m_treeWidget, columns);
            } else {
                found = new QTreeWidgetItem(parent, columns);
            }
            found->setData(0, Qt::UserRole, currentPath);
        }
        
        // Cache this item
        m_pathToItem[currentPath] = found;
        
        parent = found;
    }
    
    return parent;
}


void MainWindow::onTreeSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    
    if (selected.isEmpty()) {
        m_pathLabel->setText("No selection");
        // Disable crosspoints when nothing is selected
        if (m_crosspointsEnabled) {
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
    if (type == "Matrix") {
        // Show matrix widget in property panel
        MatrixWidget *matrixWidget = m_matrixWidgets.value(oidPath, nullptr);
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
            if (m_crosspointsEnabled) {
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

void MainWindow::onEnableCrosspointsToggled(bool enabled)
{
    m_crosspointsEnabled = enabled;
    
    if (enabled) {
        // Start activity timer (60 seconds)
        resetActivityTimer();
        m_tickTimer->start();
        logMessage("Crosspoint editing ENABLED (60 second timeout)");
    } else {
        // Stop timers
        m_activityTimer->stop();
        m_tickTimer->stop();
        m_activityTimeRemaining = 0;
        updateCrosspointsStatusBar();
        logMessage("Crosspoint editing DISABLED");
    }
    
    // Update only the currently displayed matrix widget (if any)
    MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(m_propertyPanel);
    if (matrixWidget) {
        matrixWidget->setCrosspointsEnabled(enabled);
    }
    
}

void MainWindow::onActivityTimeout()
{
    // Timeout reached - disable crosspoints
    m_enableCrosspointsAction->setChecked(false);
    logMessage("Crosspoint editing auto-disabled after 60 seconds of inactivity");
}

void MainWindow::onActivityTimerTick()
{
    if (m_crosspointsEnabled) {
        m_activityTimeRemaining = m_activityTimer->remainingTime() / 1000; // Convert to seconds
        updateCrosspointsStatusBar();
    }
}

void MainWindow::resetActivityTimer()
{
    if (m_crosspointsEnabled) {
        m_activityTimeRemaining = 60;
        m_activityTimer->start();
        updateCrosspointsStatusBar();
    }
}

void MainWindow::updateCrosspointsStatusBar()
{
    if (m_crosspointsEnabled && m_activityTimeRemaining > 0) {
        m_crosspointsStatusLabel->setText(QString("⚠ Crosspoints Enabled (%1s)").arg(m_activityTimeRemaining));
        m_crosspointsStatusLabel->setVisible(true);
    } else {
        m_crosspointsStatusLabel->setVisible(false);
    }
}


void MainWindow::onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber)
{
    if (!m_crosspointsEnabled) {
        qDebug().noquote() << "Crosspoint click ignored - crosspoints not enabled";
        return;
    }
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
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
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        bool isNew = item->text(1).isEmpty();
        
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Function");
        item->setText(2, "");
        item->setIcon(0, QIcon::fromTheme("system-run", style()->standardIcon(QStyle::SP_CommandLink)));
        
        FunctionInfo info;
        info.identifier = identifier;
        info.description = description;
        info.argNames = argNames;
        info.argTypes = argTypes;
        info.resultNames = resultNames;
        info.resultTypes = resultTypes;
        m_functions[path] = info;
        
        if (isNew) {
            QString argsStr;
            for (int i = 0; i < argNames.size(); i++) {
                if (i > 0) argsStr += ", ";
                argsStr += argNames[i];
            }
            qDebug().noquote() << QString("Function: %1 [%2] (%3 args)")
                .arg(displayName).arg(path).arg(argNames.size());
        }
    }
}

void MainWindow::onInvocationResultReceived(int invocationId, bool success, const QList<QVariant> &results)
{
    QString functionPath = m_pendingInvocations.value(invocationId, "Unknown");
    m_pendingInvocations.remove(invocationId);
    
    FunctionInfo funcInfo = m_functions.value(functionPath);
    
    QString resultText;
    if (success) {
        resultText = QString("✅ Function '%1' invoked successfully").arg(funcInfo.identifier);
        if (!results.isEmpty()) {
            resultText += "\n\nReturn values:";
            for (int i = 0; i < results.size() && i < funcInfo.resultNames.size(); i++) {
                QString valueName = funcInfo.resultNames[i];
                QVariant value = results[i];
                resultText += QString("\n  • %1: %2").arg(valueName).arg(value.toString());
            }
        }
    } else {
        resultText = QString("❌ Function '%1' invocation failed").arg(funcInfo.identifier);
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Function Invocation Result");
    msgBox.setText(resultText);
    msgBox.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.exec();
    
    qInfo().noquote() << QString("Invocation result - ID: %1, Success: %2, Results: %3")
        .arg(invocationId).arg(success ? "YES" : "NO").arg(results.size());
}

void MainWindow::onItemExpanded(QTreeWidgetItem *item)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    if (path.isEmpty() || m_subscribedPaths.contains(path)) {
        return;  // Already subscribed or no path
    }
    
    // Subscribe based on type
    if (type == "Node") {
        m_connection->subscribeToNode(path, true);
    } else if (type == "Parameter") {
        m_connection->subscribeToParameter(path, true);
    } else if (type == "Matrix") {
        m_connection->subscribeToMatrix(path, true);
    }
    
    if (!type.isEmpty()) {
        m_subscribedPaths.insert(path);
        SubscriptionState state;
        state.subscribedAt = QDateTime::currentDateTime();
        state.autoSubscribed = true;
        m_subscriptionStates[path] = state;
    }
}

void MainWindow::onItemCollapsed(QTreeWidgetItem *item)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    if (path.isEmpty() || !m_subscribedPaths.contains(path)) {
        return;  // Not subscribed
    }
    
    // Only auto-unsubscribe if it was auto-subscribed
    if (m_subscriptionStates.contains(path) && m_subscriptionStates[path].autoSubscribed) {
        if (type == "Node") {
            m_connection->unsubscribeFromNode(path);
        } else if (type == "Parameter") {
            m_connection->unsubscribeFromParameter(path);
        } else if (type == "Matrix") {
            m_connection->unsubscribeFromMatrix(path);
        }
        
        m_subscribedPaths.remove(path);
        m_subscriptionStates.remove(path);
    }
}

void MainWindow::setItemDisplayName(QTreeWidgetItem *item, const QString &baseName)
{
    item->setText(0, baseName);
}

void MainWindow::subscribeToExpandedItems()
{
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            QString type = (*it)->text(1);
            
            if (!path.isEmpty() && !m_subscribedPaths.contains(path)) {
                SubscriptionState state;
                state.subscribedAt = QDateTime::currentDateTime();
                state.autoSubscribed = true;
                
                if (type == "Node") {
                    m_connection->subscribeToNode(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                } else if (type == "Parameter") {
                    m_connection->subscribeToParameter(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                } else if (type == "Matrix") {
                    m_connection->subscribeToMatrix(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                }
            }
        }
        ++it;
    }
}

void MainWindow::onSaveEmberDevice()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Not Connected", 
            "Cannot save device: not connected to an Ember+ provider.");
        return;
    }
    
    // Generate default filename using actual device name from tree
    QString deviceName;
    
    // Try to get device name from first root node in tree
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstRoot = m_treeWidget->topLevelItem(0);
        deviceName = firstRoot->text(0);
    }
    
    // If no tree items or empty name, fall back to hostname
    if (deviceName.isEmpty()) {
        deviceName = m_hostEdit->text();
    }
    
    // Sanitize device name for filename (remove invalid chars)
    deviceName = deviceName.replace(QRegExp("[^a-zA-Z0-9_.-]"), "_");
    
    // If still empty after sanitization, use generic name
    if (deviceName.isEmpty()) {
        deviceName = "ember_device";
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("ddMMyyyy");
    QString defaultName = QString("%1_%2.json").arg(deviceName).arg(timestamp);
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Ember Device",
        defaultName,
        "Ember Device Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;  // User cancelled
    }
    
    // Capture the snapshot
    DeviceSnapshot snapshot = captureSnapshot();
    
    // Save to file
    if (snapshot.saveToFile(fileName)) {
        QString stats = QString("Device saved: %1 nodes, %2 parameters, %3 matrices, %4 functions")
            .arg(snapshot.nodeCount())
            .arg(snapshot.parameterCount())
            .arg(snapshot.matrixCount())
            .arg(snapshot.functionCount());
        
        qInfo().noquote() << stats;
        QMessageBox::information(this, "Device Saved", 
            QString("Ember device saved successfully to:\n%1\n\n%2")
                .arg(fileName)
                .arg(stats));
    } else {
        QMessageBox::critical(this, "Save Failed", 
            QString("Failed to save device to:\n%1").arg(fileName));
    }
}

DeviceSnapshot MainWindow::captureSnapshot()
{
    DeviceSnapshot snapshot;
    
    // Set metadata
    // Try to get actual device name from first root node
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstRoot = m_treeWidget->topLevelItem(0);
        snapshot.deviceName = firstRoot->text(0);
    }
    // Fall back to hostname if no tree items
    if (snapshot.deviceName.isEmpty()) {
        snapshot.deviceName = m_hostEdit->text();
    }
    
    snapshot.hostAddress = m_hostEdit->text();
    snapshot.port = m_portSpin->value();
    snapshot.captureTime = QDateTime::currentDateTime();
    
    // Iterate through tree and capture all elements
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QString path = (*it)->data(0, Qt::UserRole).toString();
        QString type = (*it)->text(1);
        
        if (path.isEmpty()) {
            ++it;
            continue;
        }
        
        if (type == "Node") {
            NodeData nodeData;
            nodeData.path = path;
            nodeData.identifier = (*it)->text(0);
            nodeData.description = "";  // Description not stored separately in tree
            nodeData.isOnline = (*it)->data(0, Qt::UserRole + 4).toBool();
            
            // Collect child paths
            for (int i = 0; i < (*it)->childCount(); ++i) {
                QString childPath = (*it)->child(i)->data(0, Qt::UserRole).toString();
                if (!childPath.isEmpty()) {
                    nodeData.childPaths.append(childPath);
                }
            }
            
            snapshot.nodes[path] = nodeData;
            
        } else if (type == "Parameter") {
            ParameterData paramData;
            paramData.path = path;
            paramData.identifier = (*it)->text(0);
            paramData.value = (*it)->text(2);
            paramData.type = (*it)->data(0, Qt::UserRole + 1).toInt();
            paramData.access = (*it)->data(0, Qt::UserRole + 2).toInt();
            paramData.minimum = (*it)->data(0, Qt::UserRole + 3);
            paramData.maximum = (*it)->data(0, Qt::UserRole + 4);
            paramData.enumOptions = (*it)->data(0, Qt::UserRole + 5).toStringList();
            
            // Convert QList<QVariant> to QList<int>
            QList<QVariant> enumVarList = (*it)->data(0, Qt::UserRole + 6).toList();
            for (const QVariant& var : enumVarList) {
                paramData.enumValues.append(var.toInt());
            }
            
            paramData.isOnline = (*it)->data(0, Qt::UserRole + 8).toBool();
            
            snapshot.parameters[path] = paramData;
            
        } else if (type == "Matrix") {
            // Get matrix widget to extract connection data
            MatrixWidget* matrixWidget = m_matrixWidgets.value(path, nullptr);
            if (matrixWidget) {
                MatrixData matrixData;
                matrixData.path = path;
                matrixData.identifier = (*it)->text(0);
                matrixData.description = "";
                
                // Parse size from text (e.g., "8x16" or "8×16")
                QString sizeText = (*it)->text(2);
                QStringList sizeParts = sizeText.split(QChar(0x00D7));  // Unicode multiplication sign
                if (sizeParts.size() == 2) {
                    matrixData.sourceCount = sizeParts[0].toInt();
                    matrixData.targetCount = sizeParts[1].toInt();
                }
                
                matrixData.type = matrixWidget->getMatrixType();
                
                // Get actual target and source indices (not just counts!)
                matrixData.targetNumbers = matrixWidget->getTargetNumbers();
                matrixData.sourceNumbers = matrixWidget->getSourceNumbers();
                
                // Extract labels and connections from MatrixWidget
                // Use actual indices, not 0 to count-1
                for (int targetIdx : matrixData.targetNumbers) {
                    QString label = matrixWidget->getTargetLabel(targetIdx);
                    // Only save custom labels (not default "Target N" format)
                    if (!label.isEmpty() && label != QString("Target %1").arg(targetIdx)) {
                        matrixData.targetLabels[targetIdx] = label;
                    }
                }
                
                for (int sourceIdx : matrixData.sourceNumbers) {
                    QString srcLabel = matrixWidget->getSourceLabel(sourceIdx);
                    // Only save custom labels (not default "Source N" format)
                    if (!srcLabel.isEmpty() && srcLabel != QString("Source %1").arg(sourceIdx)) {
                        matrixData.sourceLabels[sourceIdx] = srcLabel;
                    }
                }
                
                // Extract connections for all valid crosspoints
                for (int targetIdx : matrixData.targetNumbers) {
                    for (int sourceIdx : matrixData.sourceNumbers) {
                        bool connected = matrixWidget->isConnected(targetIdx, sourceIdx);
                        matrixData.connections[QPair<int,int>(targetIdx, sourceIdx)] = connected;
                    }
                }
                
                snapshot.matrices[path] = matrixData;
            }
            
        } else if (type == "Function") {
            if (m_functions.contains(path)) {
                FunctionInfo funcInfo = m_functions[path];
                
                FunctionData funcData;
                funcData.path = path;
                funcData.identifier = funcInfo.identifier;
                funcData.description = funcInfo.description;
                funcData.argNames = funcInfo.argNames;
                funcData.argTypes = funcInfo.argTypes;
                funcData.resultNames = funcInfo.resultNames;
                funcData.resultTypes = funcInfo.resultTypes;
                
                snapshot.functions[path] = funcData;
            }
        }
        
        ++it;
    }
    
    // Collect root paths in order (top-level items from tree widget)
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_treeWidget->topLevelItem(i);
        QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            snapshot.rootPaths.append(path);
        }
    }
    
    qInfo().noquote() << QString("Captured snapshot: %1 nodes, %2 parameters, %3 matrices, %4 functions")
        .arg(snapshot.nodeCount())
        .arg(snapshot.parameterCount())
        .arg(snapshot.matrixCount())
        .arg(snapshot.functionCount());
    
    return snapshot;
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

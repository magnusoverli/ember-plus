/*
    EmberViewer - Ember+ Device Emulator Window
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "EmulatorWindow.h"
#include "EmberProvider.h"
#include "DeviceSnapshot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QStandardPaths>
#include <QHeaderView>

EmulatorWindow::EmulatorWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_deviceTree(nullptr)
    , m_activityLog(nullptr)
    , m_clientList(nullptr)
    , m_mainSplitter(nullptr)
    , m_loadButton(nullptr)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_portSpin(nullptr)
    , m_statusLabel(nullptr)
    , m_deviceNameLabel(nullptr)
    , m_provider(nullptr)
    , m_isRunning(false)
{
    setWindowTitle("Ember+ Emulator");
    resize(1000, 700);
    
    setupUi();
    setupMenu();
    
    // Create provider
    m_provider = new EmberProvider(this);
    
    // Connect signals
    connect(m_provider, &EmberProvider::serverStateChanged, this, &EmulatorWindow::onServerStateChanged);
    connect(m_provider, &EmberProvider::clientConnected, this, &EmulatorWindow::onClientConnected);
    connect(m_provider, &EmberProvider::clientDisconnected, this, &EmulatorWindow::onClientDisconnected);
    connect(m_provider, &EmberProvider::requestReceived, this, &EmulatorWindow::onRequestReceived);
    connect(m_provider, &EmberProvider::errorOccurred, this, &EmulatorWindow::onErrorOccurred);
    
    updateServerStatus();
}

EmulatorWindow::~EmulatorWindow()
{
    if (m_provider && m_isRunning) {
        m_provider->stopListening();
    }
}

void EmulatorWindow::setupUi()
{
    // Central widget with main splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    // Left panel - Device tree
    m_deviceGroup = new QGroupBox("Emulated Device", this);
    QVBoxLayout *deviceLayout = new QVBoxLayout(m_deviceGroup);
    
    m_deviceNameLabel = new QLabel("No device loaded", this);
    m_deviceNameLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    deviceLayout->addWidget(m_deviceNameLabel);
    
    m_deviceTree = new QTreeWidget(this);
    m_deviceTree->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    m_deviceTree->setColumnWidth(0, 200);
    m_deviceTree->setColumnWidth(1, 100);
    deviceLayout->addWidget(m_deviceTree);
    
    // Load button
    m_loadButton = new QPushButton("Load Snapshot...", this);
    connect(m_loadButton, &QPushButton::clicked, this, &EmulatorWindow::onLoadSnapshot);
    deviceLayout->addWidget(m_loadButton);
    
    m_mainSplitter->addWidget(m_deviceGroup);
    
    // Right panel - Server status and activity
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    
    // Server status group
    m_statusGroup = new QGroupBox("Server Status", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(m_statusGroup);
    
    m_statusLabel = new QLabel("Status: Stopped", this);
    statusLayout->addWidget(m_statusLabel);
    
    // Port control
    QHBoxLayout *portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("Port:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(DEFAULT_EMULATOR_PORT);
    portLayout->addWidget(m_portSpin);
    portLayout->addStretch();
    statusLayout->addLayout(portLayout);
    
    // Start/Stop buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Server", this);
    m_stopButton = new QPushButton("Stop Server", this);
    m_stopButton->setEnabled(false);
    connect(m_startButton, &QPushButton::clicked, this, &EmulatorWindow::onStartServer);
    connect(m_stopButton, &QPushButton::clicked, this, &EmulatorWindow::onStopServer);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    statusLayout->addLayout(buttonLayout);
    
    // Connected clients
    statusLayout->addWidget(new QLabel("Connected Clients:", this));
    m_clientList = new QListWidget(this);
    m_clientList->setMaximumHeight(100);
    statusLayout->addWidget(m_clientList);
    
    rightLayout->addWidget(m_statusGroup);
    
    // Activity log group
    m_activityGroup = new QGroupBox("Activity Log", this);
    QVBoxLayout *activityLayout = new QVBoxLayout(m_activityGroup);
    
    m_activityLog = new QTextEdit(this);
    m_activityLog->setReadOnly(true);
    activityLayout->addWidget(m_activityLog);
    
    rightLayout->addWidget(m_activityGroup);
    
    m_mainSplitter->addWidget(rightPanel);
    
    // Set splitter sizes (60% left, 40% right)
    m_mainSplitter->setSizes(QList<int>() << 600 << 400);
}

void EmulatorWindow::setupMenu()
{
    QMenuBar *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    
    QAction *loadAction = fileMenu->addAction("&Load Snapshot...");
    loadAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(loadAction, &QAction::triggered, this, &EmulatorWindow::onLoadSnapshot);
    
    fileMenu->addSeparator();
    
    QAction *closeAction = fileMenu->addAction("&Close");
    closeAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    
    // Server menu
    QMenu *serverMenu = menuBar->addMenu("&Server");
    
    QAction *startServerAction = serverMenu->addAction("&Start Server");
    startServerAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(startServerAction, &QAction::triggered, this, &EmulatorWindow::onStartServer);
    
    QAction *stopServerAction = serverMenu->addAction("S&top Server");
    stopServerAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(stopServerAction, &QAction::triggered, this, &EmulatorWindow::onStopServer);
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    
    QAction *clearLogAction = viewMenu->addAction("&Clear Activity Log");
    clearLogAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(clearLogAction, &QAction::triggered, this, [this]() {
        m_activityLog->clear();
        logActivity("Activity log cleared");
    });
    
    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Ember+ Emulator",
            "Ember+ Device Emulator\n\n"
            "Part of EmberViewer - Version 1.4\n\n"
            "Load device snapshots and emulate Ember+ providers for testing.\n\n"
            "Copyright (C) 2025 Magnus Overli\n"
            "Distributed under the Boost Software License, Version 1.0.");
    });
}

void EmulatorWindow::onLoadSnapshot()
{
    QString lastDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getOpenFileName(this,
        "Load Device Snapshot",
        lastDir,
        "JSON Files (*.json);;All Files (*)");
    
    if (filePath.isEmpty()) {
        return;
    }
    
    try {
        DeviceSnapshot snapshot = DeviceSnapshot::loadFromFile(filePath);
        loadSnapshotData(snapshot);
        m_loadedSnapshotPath = filePath;
        logActivity(QString("Loaded snapshot: %1").arg(filePath));
        
        // Update device name
        m_deviceName = snapshot.deviceName;
        if (m_deviceName.isEmpty()) {
            m_deviceName = "Unknown Device";
        }
        m_deviceNameLabel->setText(m_deviceName);
        
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Error Loading Snapshot",
            QString("Failed to load snapshot:\n%1").arg(e.what()));
        logActivity(QString("ERROR: Failed to load snapshot - %1").arg(e.what()));
    }
}

void EmulatorWindow::loadSnapshotData(const DeviceSnapshot &snapshot)
{
    m_deviceTree->clear();
    
    // Load the snapshot into the provider
    if (m_provider) {
        m_provider->loadDeviceTree(snapshot);
    }
    
    // Build tree view for display
    QMap<QString, QTreeWidgetItem*> pathToItem;
    
    // Add nodes
    for (const NodeData &node : snapshot.nodes) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, node.path);
        item->setText(1, "Node");
        item->setText(2, node.identifier);
        
        // Find parent or add to root
        QStringList pathParts = node.path.split('.');
        if (pathParts.size() == 1) {
            m_deviceTree->addTopLevelItem(item);
        } else {
            pathParts.removeLast();
            QString parentPath = pathParts.join('.');
            if (pathToItem.contains(parentPath)) {
                pathToItem[parentPath]->addChild(item);
            } else {
                m_deviceTree->addTopLevelItem(item);
            }
        }
        
        pathToItem[node.path] = item;
    }
    
    // Add parameters
    for (const ParameterData &param : snapshot.parameters) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, param.path);
        item->setText(1, "Parameter");
        item->setText(2, QString("%1 = %2").arg(param.identifier).arg(param.value));
        
        // Find parent
        QStringList pathParts = param.path.split('.');
        if (pathParts.size() > 1) {
            pathParts.removeLast();
            QString parentPath = pathParts.join('.');
            if (pathToItem.contains(parentPath)) {
                pathToItem[parentPath]->addChild(item);
            } else {
                m_deviceTree->addTopLevelItem(item);
            }
        } else {
            m_deviceTree->addTopLevelItem(item);
        }
        
        pathToItem[param.path] = item;
    }
    
    // Add matrices
    for (const MatrixData &matrix : snapshot.matrices) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, matrix.path);
        item->setText(1, "Matrix");
        item->setText(2, QString("%1 (%2x%3)").arg(matrix.identifier)
            .arg(matrix.targetCount).arg(matrix.sourceCount));
        
        // Find parent
        QStringList pathParts = matrix.path.split('.');
        if (pathParts.size() > 1) {
            pathParts.removeLast();
            QString parentPath = pathParts.join('.');
            if (pathToItem.contains(parentPath)) {
                pathToItem[parentPath]->addChild(item);
            } else {
                m_deviceTree->addTopLevelItem(item);
            }
        } else {
            m_deviceTree->addTopLevelItem(item);
        }
        
        pathToItem[matrix.path] = item;
    }
    
    // Add functions
    for (const FunctionData &func : snapshot.functions) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, func.path);
        item->setText(1, "Function");
        item->setText(2, func.identifier);
        
        // Find parent
        QStringList pathParts = func.path.split('.');
        if (pathParts.size() > 1) {
            pathParts.removeLast();
            QString parentPath = pathParts.join('.');
            if (pathToItem.contains(parentPath)) {
                pathToItem[parentPath]->addChild(item);
            } else {
                m_deviceTree->addTopLevelItem(item);
            }
        } else {
            m_deviceTree->addTopLevelItem(item);
        }
        
        pathToItem[func.path] = item;
    }
    
    m_deviceTree->expandAll();
    
    logActivity(QString("Loaded device tree: %1 nodes, %2 parameters, %3 matrices, %4 functions")
        .arg(snapshot.nodeCount())
        .arg(snapshot.parameterCount())
        .arg(snapshot.matrixCount())
        .arg(snapshot.functionCount()));
}

void EmulatorWindow::onStartServer()
{
    if (m_loadedSnapshotPath.isEmpty()) {
        QMessageBox::warning(this, "No Device Loaded",
            "Please load a device snapshot before starting the server.");
        return;
    }
    
    int port = m_portSpin->value();
    
    if (m_provider->startListening(port)) {
        m_isRunning = true;
        logActivity(QString("Server started on port %1").arg(port));
    } else {
        QMessageBox::critical(this, "Server Error",
            "Failed to start server. Port may already be in use.");
        logActivity(QString("ERROR: Failed to start server on port %1").arg(port));
    }
}

void EmulatorWindow::onStopServer()
{
    if (m_provider) {
        m_provider->stopListening();
        m_isRunning = false;
        logActivity("Server stopped");
    }
}

void EmulatorWindow::onServerStateChanged(bool running)
{
    m_isRunning = running;
    updateServerStatus();
    
    // Enable/disable controls
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_portSpin->setEnabled(!running);
}

void EmulatorWindow::onClientConnected(const QString &address)
{
    m_clientList->addItem(address);
    logActivity(QString("Client connected: %1").arg(address));
}

void EmulatorWindow::onClientDisconnected(const QString &address)
{
    // Find and remove the client from the list
    for (int i = 0; i < m_clientList->count(); ++i) {
        if (m_clientList->item(i)->text() == address) {
            delete m_clientList->takeItem(i);
            break;
        }
    }
    logActivity(QString("Client disconnected: %1").arg(address));
}

void EmulatorWindow::onRequestReceived(const QString &path, const QString &command)
{
    logActivity(QString("Request: %1 on path %2").arg(command).arg(path));
}

void EmulatorWindow::onErrorOccurred(const QString &error)
{
    logActivity(QString("ERROR: %1").arg(error));
}

void EmulatorWindow::updateServerStatus()
{
    if (m_isRunning) {
        m_statusLabel->setText(QString("Status: Running on port %1").arg(m_portSpin->value()));
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_statusLabel->setText("Status: Stopped");
        m_statusLabel->setStyleSheet("color: red;");
    }
}

void EmulatorWindow::logActivity(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_activityLog->append(QString("[%1] %2").arg(timestamp).arg(message));
}

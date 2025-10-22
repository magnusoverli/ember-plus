/*
 * MainWindow.cpp - Main application window implementation
 */

#include "MainWindow.h"
#include "EmberConnection.h"
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_treeWidget(nullptr)
    , m_propertyPanel(nullptr)
    , m_consoleLog(nullptr)
    , m_consoleDock(nullptr)
    , m_propertyDock(nullptr)
    , m_hostEdit(nullptr)
    , m_portSpin(nullptr)
    , m_connectButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_statusLabel(nullptr)
    , m_connection(nullptr)
    , m_isConnected(false)
{
    setupUi();
    setupMenu();
    setupToolBar();
    setupStatusBar();
    createDockWindows();
    
    // Create connection handler
    m_connection = new EmberConnection(this);
    connect(m_connection, &EmberConnection::connected, this, [this]() {
        onConnectionStateChanged(true);
    });
    connect(m_connection, &EmberConnection::disconnected, this, [this]() {
        onConnectionStateChanged(false);
    });
    connect(m_connection, &EmberConnection::logMessage, this, &MainWindow::onLogMessage);
    connect(m_connection, &EmberConnection::treeItemReceived, this, &MainWindow::onTreeItemReceived);
    
    setWindowTitle("EmberViewer - Ember+ Protocol Viewer");
    resize(1200, 800);
    
    logMessage("EmberViewer started. Ready to connect.");
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    // Create central widget with tree view
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    m_treeWidget->setColumnCount(3);
    m_treeWidget->header()->setStretchLastSection(true);
    m_treeWidget->setAlternatingRowColors(true);
    
    // Connection controls
    QWidget *connectionWidget = new QWidget(this);
    QHBoxLayout *connLayout = new QHBoxLayout(connectionWidget);
    
    connLayout->addWidget(new QLabel("Host:"));
    m_hostEdit = new QLineEdit("localhost");
    m_hostEdit->setMaximumWidth(200);
    connLayout->addWidget(m_hostEdit);
    
    connLayout->addWidget(new QLabel("Port:"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(9092);
    m_portSpin->setMaximumWidth(80);
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
    
    // Main layout
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(connectionWidget);
    mainLayout->addWidget(m_treeWidget);
    
    setCentralWidget(centralWidget);
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
    
    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About EmberViewer",
            "<h3>EmberViewer v1.0.0</h3>"
            "<p>A modern, cross-platform Ember+ protocol viewer.</p>"
            "<p>Built with Qt5 and libember.</p>"
            "<p>© 2025 Magnus Overli<br>"
            "<a href='https://github.com/magnusoverli/ember-plus'>github.com/magnusoverli/ember-plus</a></p>");
    });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Main Toolbar");
    toolBar->setMovable(false);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
}

void MainWindow::createDockWindows()
{
    // Console/Log dock
    m_consoleDock = new QDockWidget("Console", this);
    m_consoleLog = new QTextEdit();
    m_consoleLog->setReadOnly(true);
    m_consoleLog->setMaximumHeight(150);
    m_consoleDock->setWidget(m_consoleLog);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
    
    // Property panel dock
    m_propertyDock = new QDockWidget("Properties", this);
    m_propertyPanel = new QWidget();
    QVBoxLayout *propLayout = new QVBoxLayout(m_propertyPanel);
    propLayout->addWidget(new QLabel("Select an item to view properties"));
    propLayout->addStretch();
    m_propertyDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);
    
    // Add dock toggles to View menu
    QMenu *viewMenu = menuBar()->findChild<QMenu*>("View");
    if (!viewMenu) {
        viewMenu = menuBar()->addMenu("&View");
    }
    viewMenu->addAction(m_consoleDock->toggleViewAction());
    viewMenu->addAction(m_propertyDock->toggleViewAction());
}

void MainWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    logMessage(QString("Connecting to %1:%2...").arg(host).arg(port));
    m_connection->connectToHost(host, port);
}

void MainWindow::onDisconnectClicked()
{
    logMessage("Disconnecting...");
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
        statusBar()->showMessage("Connected");
        logMessage("Connected successfully!");
    } else {
        m_statusLabel->setText("Not connected");
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        statusBar()->showMessage("Disconnected");
        logMessage("Disconnected.");
        m_treeWidget->clear();
    }
}

void MainWindow::onLogMessage(const QString &message)
{
    logMessage(message);
}

void MainWindow::onTreeItemReceived(const QString &path, const QString &identifier, const QString &description)
{
    // Add item to tree
    QStringList columns;
    columns << identifier << "Node" << description;
    
    QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget, columns);
    item->setData(0, Qt::UserRole, path);
    
    logMessage(QString("Received: %1 (%2)").arg(identifier).arg(description));
}

void MainWindow::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_consoleLog->append(QString("[%1] %2").arg(timestamp).arg(message));
}


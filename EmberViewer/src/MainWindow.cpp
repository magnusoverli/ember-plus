/*
 * MainWindow.cpp - Main application window implementation
 */

#include "MainWindow.h"
#include "EmberConnection.h"
#include "ParameterDelegate.h"
#include "MatrixWidget.h"
#include "Logger.h"
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
#include <QDesktopServices>
#include <QUrl>

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
    , m_pathLabel(nullptr)
    , m_connection(nullptr)
    , m_enableCrosspointsAction(nullptr)
    , m_activityTimer(nullptr)
    , m_tickTimer(nullptr)
    , m_activityTimeRemaining(0)
    , m_crosspointsStatusLabel(nullptr)
    , m_isConnected(false)
    , m_showOidPath(false)
    , m_crosspointsEnabled(false)
{
    setupUi();
    setupMenu();
    setupToolBar();
    setupStatusBar();
    createDockWindows();
    
    // Install event filter on application to detect user activity
    qApp->installEventFilter(this);
    
    // Setup activity timer (60 second timeout)
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(true);
    m_activityTimer->setInterval(60000); // 60 seconds
    connect(m_activityTimer, &QTimer::timeout, this, &MainWindow::onActivityTimeout);
    
    // Setup tick timer (1 second updates for status bar)
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000); // 1 second
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
    connect(m_connection, &EmberConnection::nodeReceived, this, &MainWindow::onNodeReceived);
    connect(m_connection, &EmberConnection::parameterReceived, this, &MainWindow::onParameterReceived);
    connect(m_connection, &EmberConnection::matrixReceived, this, &MainWindow::onMatrixReceived);
    connect(m_connection, &EmberConnection::matrixTargetReceived, this, &MainWindow::onMatrixTargetReceived);
    connect(m_connection, &EmberConnection::matrixSourceReceived, this, &MainWindow::onMatrixSourceReceived);
    connect(m_connection, &EmberConnection::matrixConnectionReceived, this, &MainWindow::onMatrixConnectionReceived);
    connect(m_connection, &EmberConnection::matrixConnectionsCleared, this, &MainWindow::onMatrixConnectionsCleared);
    
    setWindowTitle("EmberViewer - Ember+ Protocol Viewer");
    resize(1200, 800);
    
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
    m_treeWidget->setColumnWidth(0, 350);  // Path column - wide enough for breadcrumbs
    m_treeWidget->setColumnWidth(1, 150);  // Type column - narrow, just "Node" or "Parameter"
    m_treeWidget->header()->setStretchLastSection(true);  // Value column stretches to fill
    m_treeWidget->setAlternatingRowColors(true);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    
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
    
    m_enableCrosspointsAction = fileMenu->addAction("Enable &Crosspoints");
    m_enableCrosspointsAction->setShortcut(QKeySequence("Ctrl+E"));
    m_enableCrosspointsAction->setCheckable(true);
    m_enableCrosspointsAction->setChecked(false);
    connect(m_enableCrosspointsAction, &QAction::toggled, this, &MainWindow::onEnableCrosspointsToggled);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *openLogAction = helpMenu->addAction("Open &Log Directory");
    connect(openLogAction, &QAction::triggered, this, [this]() {
        QString logDir = Logger::instance()->logDirectory();
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
    });
    
    QAction *showLogPathAction = helpMenu->addAction("Show Log &File Path");
    connect(showLogPathAction, &QAction::triggered, this, [this]() {
        QString logPath = Logger::instance()->logFilePath();
        QMessageBox::information(this, "Log File Location",
            QString("<p><b>Current log file:</b></p>"
                    "<p style='font-family: monospace;'>%1</p>"
                    "<p>Logs are automatically rotated when they exceed 10MB.<br>"
                    "Up to 5 log files are kept.</p>").arg(logPath));
    });
    
    helpMenu->addSeparator();
    
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
    
    // Add dock toggles to View menu (create it here with content)
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_consoleDock->toggleViewAction());
    viewMenu->addAction(m_propertyDock->toggleViewAction());
}

void MainWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    
    // Clear console log from previous session
    m_consoleLog->clear();
    
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
        logMessage("Connected successfully!");
    } else {
        m_statusLabel->setText("Not connected");
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        logMessage("Disconnected.");
        m_treeWidget->clear();
        
        // Clear all matrix widgets
        qDeleteAll(m_matrixWidgets);
        m_matrixWidgets.clear();
    }
}

void MainWindow::onLogMessage(const QString &message)
{
    logMessage(message);
}

void MainWindow::onNodeReceived(const QString &path, const QString &identifier, const QString &description)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new node (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        // For nodes: prefer description as display name (often more readable than UUID identifier)
        // If no description, fall back to identifier
        QString displayName = !description.isEmpty() ? description : identifier;
        
        item->setText(0, displayName);
        item->setText(1, "Node");
        item->setText(2, "");  // Keep Value column empty for nodes
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        
        if (isNew) {
            logMessage(QString("Node: %1 [%2]").arg(displayName).arg(path));
        }
    }
}

void MainWindow::onParameterReceived(const QString &path, int /* number */, const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues)
{
    // Check if this parameter is actually a matrix label
    // Pattern: matrixPath.666999666.1.N (targets) or matrixPath.666999666.2.N (sources)
    QStringList pathParts = path.split('.');
    if (pathParts.size() >= 4 && pathParts[pathParts.size() - 3] == "666999666") {
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
                matrixWidget->setTargetLabel(number, value);
            } else if (labelType == "2") {
                // Source label
                matrixWidget->setSourceLabel(number, value);
            }
        }
        
        // Still create the tree item for the label (for completeness)
    }
    
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new parameter (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        item->setText(0, identifier);
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
                    logMessage(QString("DEBUG: Preserving Boolean widget for %1 (type was 0)").arg(path));
                } else {
                    effectiveType = 6;  // Enum
                    logMessage(QString("DEBUG: Preserving Enum widget for %1 (type was 0)").arg(path));
                }
            } else {
                logMessage(QString("DEBUG: Widget exists for %1, type=%2").arg(path).arg(effectiveType));
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
                    logMessage(QString("DEBUG: No widget for %1, inferring Boolean from value '%2'").arg(path).arg(value));
                } else {
                    logMessage(QString("DEBUG: No widget for %1, type=0, value='%2' (not boolean)").arg(path).arg(value));
                }
            } else {
                logMessage(QString("DEBUG: No widget for %1, type=%2, value='%3'").arg(path).arg(effectiveType).arg(value));
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
                logMessage(QString("Trigger invoked: %1").arg(path));
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
                        logMessage(QString("Boolean changed: %1 → %2").arg(path).arg(newValue));
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
                            logMessage(QString("Enum changed: %1 → '%2' (value %3)").arg(path).arg(displayValue).arg(newValue));
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
                logMessage(QString("DEBUG: REMOVING WIDGET for %1 (fell through to text branch, type=%2, access=%3)").arg(path).arg(effectiveType).arg(access));
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
            logMessage(QString("Parameter: %1 = %2 [%3] (Type: %4, Access: %5)").arg(identifier).arg(value).arg(path).arg(type).arg(access));
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
        
        item->setText(0, displayName);
        item->setText(1, "Matrix");
        item->setText(2, QString("%1×%2").arg(targetCount).arg(sourceCount));
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        
        // Store matrix path in user data for later retrieval
        item->setData(0, Qt::UserRole, path);
        item->setData(0, Qt::UserRole + 7, "Matrix");  // Store type as string for onTreeSelectionChanged
        
        // Create or update the matrix widget
        MatrixWidget *matrixWidget = m_matrixWidgets.value(path, nullptr);
        if (!matrixWidget) {
            matrixWidget = new MatrixWidget(this);
            m_matrixWidgets[path] = matrixWidget;
            
            // Connect crosspoint click signal
            connect(matrixWidget, &MatrixWidget::crosspointClicked,
                    this, &MainWindow::onCrosspointClicked);
        }
        
        matrixWidget->setMatrixPath(path);
        matrixWidget->setMatrixInfo(identifier, description, type, targetCount, sourceCount);
        
        if (isNew) {
            logMessage(QString("Matrix: %1 (%2×%3) [%4]").arg(displayName).arg(targetCount).arg(sourceCount).arg(path));
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

void MainWindow::onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected)
{
    logMessage(QString("DEBUG UI: Connection received - Matrix [%1], Target %2, Source %3, Connected: %4")
               .arg(matrixPath).arg(targetNumber).arg(sourceNumber).arg(connected ? "YES" : "NO"));
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        logMessage(QString("DEBUG UI: Found matrix widget, calling setConnection()"));
        matrixWidget->setConnection(targetNumber, sourceNumber, connected);
    } else {
        logMessage(QString("DEBUG UI: ERROR - No matrix widget found for path [%1]").arg(matrixPath));
    }
}

void MainWindow::onMatrixConnectionsCleared(const QString &matrixPath)
{
    logMessage(QString("Clearing all connections for matrix %1").arg(matrixPath));
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->clearConnections();
        logMessage(QString("Connections cleared for matrix %1").arg(matrixPath));
    }
}

QTreeWidgetItem* MainWindow::findOrCreateTreeItem(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
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
        
        // Search for existing item with this path
        QTreeWidgetItem *found = nullptr;
        
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
        
        parent = found;
    }
    
    return parent;
}

void MainWindow::clearTree()
{
    m_treeWidget->clear();
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
                m_propertyDock->setWidget(nullptr);
                oldWidget->deleteLater();
            }
            
            // Create scroll area for matrix widget
            QScrollArea *scrollArea = new QScrollArea();
            scrollArea->setWidget(matrixWidget);
            scrollArea->setWidgetResizable(false);  // Don't resize the matrix - let it maintain its size
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            
            m_propertyDock->setWidget(scrollArea);
            m_propertyPanel = scrollArea;
            
            // Update background based on crosspoints state
            updatePropertyPanelBackground();
        }
    } else {
        // For non-matrix items, show the default property panel
        // Check if we need to restore the default panel
        QScrollArea *currentScroll = qobject_cast<QScrollArea*>(m_propertyPanel);
        if (currentScroll && currentScroll->widget() && qobject_cast<MatrixWidget*>(currentScroll->widget())) {
            // Disable crosspoints BEFORE we delete the matrix widget
            if (m_crosspointsEnabled) {
                m_enableCrosspointsAction->setChecked(false);
            }
            
            // We're currently showing a matrix, restore default panel
            QWidget *oldWidget = m_propertyPanel;
            if (oldWidget) {
                m_propertyDock->setWidget(nullptr);
                oldWidget->deleteLater();
            }
            
            m_propertyPanel = new QWidget();
            QVBoxLayout *propLayout = new QVBoxLayout(m_propertyPanel);
            propLayout->addWidget(new QLabel("Select an item to view properties"));
            propLayout->addStretch();
            m_propertyDock->setWidget(m_propertyPanel);
        }
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    // Load connection settings
    QString host = settings.value("connection/host", "localhost").toString();
    int port = settings.value("connection/port", 9000).toInt();
    
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);
    
    // Load window geometry
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }
    if (settings.contains("window/state")) {
        restoreState(settings.value("window/state").toByteArray());
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("EmberViewer", "EmberViewer");
    
    // Save connection settings
    settings.setValue("connection/host", m_hostEdit->text());
    settings.setValue("connection/port", m_portSpin->value());
    
    // Save window geometry
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
    
    // Force immediate write to disk
    settings.sync();
}

void MainWindow::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString fullMessage = QString("[%1] %2").arg(timestamp).arg(message);
    m_consoleLog->append(fullMessage);
    
    // Also log to stderr for terminal monitoring
    qDebug().noquote() << fullMessage;
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
    QScrollArea *scrollArea = qobject_cast<QScrollArea*>(m_propertyPanel);
    if (scrollArea && scrollArea->widget()) {
        MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(scrollArea->widget());
        if (matrixWidget) {
            matrixWidget->setCrosspointsEnabled(enabled);
        }
    }
    
    // Update property panel background if matrix is currently displayed
    updatePropertyPanelBackground();
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

void MainWindow::updatePropertyPanelBackground()
{
    // The MatrixWidget itself handles its own background color
    // This method is kept for potential future use but currently does nothing
    // since MatrixWidget::setCrosspointsEnabled() already sets the background
}

void MainWindow::onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber)
{
    if (!m_crosspointsEnabled) {
        logMessage("Crosspoint click ignored - crosspoints not enabled");
        return;
    }
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (!matrixWidget) {
        logMessage("ERROR: Matrix widget not found for path: " + matrixPath);
        return;
    }
    
    bool currentlyConnected = matrixWidget->isConnected(targetNumber, sourceNumber);
    int matrixType = matrixWidget->getMatrixType();
    
    QString typeStr;
    if (matrixType == 0) typeStr = "1:N";
    else if (matrixType == 1) typeStr = "1:1";
    else typeStr = "N:N";
    
    logMessage(QString("Crosspoint clicked [%1]: Matrix=%2, Target=%3, Source=%4, Current=%5")
               .arg(typeStr).arg(matrixPath).arg(targetNumber).arg(sourceNumber)
               .arg(currentlyConnected ? "Connected" : "Disconnected"));
    
    // For all matrix types, clicking toggles the connection
    // The Ember+ device will handle the matrix type rules (disconnecting other sources, etc.)
    bool newState = !currentlyConnected;
    
    // **OPTIMISTIC UPDATE**: Update UI immediately for instant feedback
    matrixWidget->setConnection(targetNumber, sourceNumber, newState);
    logMessage(QString("Optimistic UI update: %1").arg(newState ? "Connected" : "Disconnected"));
    
    // Send command to EmberConnection
    m_connection->setMatrixConnection(matrixPath, targetNumber, sourceNumber, newState);
    
    // Request updated connection state from the device after a short delay to verify
    // Reduced from 100ms to 20ms for faster verification
    QTimer::singleShot(20, this, [this, matrixPath]() {
        m_connection->requestMatrixConnections(matrixPath);
    });
}


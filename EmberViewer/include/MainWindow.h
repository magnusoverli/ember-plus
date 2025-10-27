/*
    EmberViewer - Main application window
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QStatusBar>
#include <QSettings>
#include <QAction>
#include <QTimer>
#include <QDateTime>
#include <QSet>

#include "UpdateManager.h"

class EmberConnection;
class MatrixWidget;
class DeviceSnapshot;
class EmulatorWindow;
class UpdateDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void appendToConsole(const QString &message);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onConnectionStateChanged(bool connected);
    void onLogMessage(const QString &message);
    void onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void onParameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                            int access, int type, const QVariant &minimum, const QVariant &maximum,
                            const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline);
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void onMatrixConnectionsCleared(const QString &matrixPath);
    void onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber);
    void onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                           const QStringList &argNames, const QList<int> &argTypes,
                           const QStringList &resultNames, const QList<int> &resultTypes);
    void onInvocationResultReceived(int invocationId, bool success, const QList<QVariant> &results);
    void onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    void onTreeSelectionChanged();
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemCollapsed(QTreeWidgetItem *item);
    void onEnableCrosspointsToggled(bool enabled);
    void onActivityTimeout();
    void onActivityTimerTick();
    void onSaveEmberDevice();
    void onOpenEmulator();
    void onCheckForUpdates();
    void onUpdateAvailable(const UpdateManager::UpdateInfo &updateInfo);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString &errorMessage);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupMenu();
    void setupStatusBar();
    void createDockWindows();
    void loadSettings();
    void saveSettings();
    
    void logMessage(const QString &message);
    void setItemDisplayName(QTreeWidgetItem *item, const QString &baseName);
    QTreeWidgetItem* findOrCreateTreeItem(const QString &path);
    void resetActivityTimer();
    void updateCrosspointsStatusBar();
    void subscribeToExpandedItems();
    DeviceSnapshot captureSnapshot();
    
    // Widgets
    QTreeWidget *m_treeWidget;
    QWidget *m_propertyPanel;
    QTextEdit *m_consoleLog;
    QGroupBox *m_consoleGroup;
    QGroupBox *m_propertyGroup;
    QSplitter *m_mainSplitter;
    QSplitter *m_verticalSplitter;
    
    // Connection controls
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_statusLabel;
    QLabel *m_pathLabel;
    
    // Ember+ connection
    EmberConnection *m_connection;
    
    // Matrix widgets (path -> widget)
    QMap<QString, MatrixWidget*> m_matrixWidgets;
    
    // Function metadata storage
    struct FunctionInfo {
        QString identifier;
        QString description;
        QStringList argNames;
        QList<int> argTypes;
        QStringList resultNames;
        QList<int> resultTypes;
    };
    QMap<QString, FunctionInfo> m_functions;  // path -> function info
    QMap<int, QString> m_pendingInvocations;  // invocationId -> function path
    
    // Fast path lookup for tree items
    QMap<QString, QTreeWidgetItem*> m_pathToItem;
    
    // Subscription tracking
    QSet<QString> m_subscribedPaths;
    struct SubscriptionState {
        QDateTime subscribedAt;
        bool autoSubscribed;
    };
    QMap<QString, SubscriptionState> m_subscriptionStates;
    
    // State
    bool m_isConnected;
    bool m_showOidPath;
    bool m_crosspointsEnabled;

    // Crosspoint editing
    QAction *m_enableCrosspointsAction;
    QTimer *m_activityTimer;
    QTimer *m_tickTimer;
    int m_activityTimeRemaining;
    QLabel *m_crosspointsStatusLabel;
    
    // Emulator window
    EmulatorWindow *m_emulatorWindow;
    
    // Update system
    UpdateManager *m_updateManager;
    UpdateDialog *m_updateDialog;
    QLabel *m_updateStatusLabel;

public:
    // Constants
    static constexpr int MATRIX_LABEL_PATH_MARKER = 666999666;
    static constexpr int ACTIVITY_TIMEOUT_MS = 60000;  // 60 seconds
    static constexpr int TICK_INTERVAL_MS = 1000;       // 1 second
    static constexpr int DEFAULT_EMBER_PORT = 9092;
    static constexpr int DEFAULT_PORT_FALLBACK = 9000;  // For settings
};

#endif // MAINWINDOW_H


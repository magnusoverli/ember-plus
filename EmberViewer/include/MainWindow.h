







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
#include <QProgressDialog>

#include "UpdateManager.h"

class EmberConnection;
class MatrixWidget;
class MeterWidget;
class DeviceSnapshot;
class EmulatorWindow;
class UpdateDialog;
class ConnectionManager;
class ConnectionsTreeWidget;
class EmberTreeWidget;
class TreeViewController;
class SubscriptionManager;
class MatrixManager;
class CrosspointActivityTracker;
class SnapshotManager;
class FunctionInvoker;
class TriggerWidget;
class SliderWidget;
class GraphWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    bool isQtInternalLoggingEnabled() const { return m_enableQtInternalLogging; }
    void setQtInternalLoggingEnabled(bool enabled);

public slots:
    void appendToConsole(const QString &message);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onConnectionStateChanged(bool connected);
    void onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void onParameterReceived(const QString &path, int number, const QString &identifier, const QString &description, const QString &value, 
                            int access, int type, const QVariant &minimum, const QVariant &maximum,
                            const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier,
                            const QString &format = QString(), const QString &referenceLevel = QString(),
                            const QString &formula = QString(), int factor = 1);
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void onMatrixConnectionsCleared(const QString &matrixPath);
    void onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber);
    void onMatrixDimensionsUpdated(const QString &path, QWidget *widget);
    void onMatrixWidgetCreated(const QString &path, QWidget *widget);
    void onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                           const QStringList &argNames, const QList<int> &argTypes,
                           const QStringList &resultNames, const QList<int> &resultTypes);
    void onStreamValueReceived(int streamIdentifier, double value);
    void onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    void onTreeSelectionChanged();
    void onEnableCrosspointsToggled(bool enabled);
    void onActivityTimeout();
    void onSaveEmberDevice();
    void onOpenEmulator();
    void onCheckForUpdates();
    void onUpdateAvailable(const UpdateManager::UpdateInfo &updateInfo);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString &errorMessage);
    void onSaveCurrentConnection();
    void onImportConnections();
    void onExportConnections();
    void onSavedConnectionDoubleClicked(const QString &name, const QString &host, int port);

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
    void cleanupActiveParameterWidget();
    void cleanupPropertyPanelLayout();
    
    void logMessage(const QString &message);
    
    
    EmberTreeWidget *m_treeWidget;
    QWidget *m_propertyPanel;
    QTextEdit *m_consoleLog;
    QGroupBox *m_consoleGroup;
    QGroupBox *m_propertyGroup;
    QSplitter *m_mainSplitter;
    QSplitter *m_verticalSplitter;
    
    
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_statusLabel;
    QLabel *m_pathLabel;
    
    
    EmberConnection *m_connection;
    
    
    TreeViewController *m_treeViewController;
    SubscriptionManager *m_subscriptionManager;
    MatrixManager *m_matrixManager;
    CrosspointActivityTracker *m_activityTracker;
    SnapshotManager *m_snapshotManager;
    FunctionInvoker *m_functionInvoker;
    
    
    MeterWidget *m_activeMeter;
    QString m_activeMeterPath;  
    
    
    QWidget *m_activeParameterWidget;  
    QString m_activeParameterPath;     
    
    
    QMap<int, QString> m_streamIdToPath;
    
    
    
    
    
    bool m_isConnected;
    bool m_showOidPath;
    bool m_enableQtInternalLogging;

    
    QAction *m_enableCrosspointsAction;
    QLabel *m_crosspointsStatusLabel;
    
    
    EmulatorWindow *m_emulatorWindow;
    
    
    UpdateManager *m_updateManager;
    UpdateDialog *m_updateDialog;
    QLabel *m_updateStatusLabel;
    
    
    ConnectionManager *m_connectionManager;
    ConnectionsTreeWidget *m_connectionsTree;

public:
    
    static constexpr int MATRIX_LABEL_PATH_MARKER = 666999666;
    static constexpr int DEFAULT_EMBER_PORT = 9092;
    static constexpr int DEFAULT_PORT_FALLBACK = 9000;  
};

#endif 

/*
 * MainWindow.h - Main application window
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
#include <QMap>
#include <QTimer>

class EmberConnection;
class MatrixWidget;

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
    void onNodeReceived(const QString &path, const QString &identifier, const QString &description);
    void onParameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                            int access, int type, const QVariant &minimum, const QVariant &maximum,
                            const QStringList &enumOptions, const QList<int> &enumValues);
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void onMatrixConnectionsCleared(const QString &matrixPath);
    void onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                           const QStringList &argNames, const QList<int> &argTypes,
                           const QStringList &resultNames, const QList<int> &resultTypes);
    void onInvocationResultReceived(int invocationId, bool success, const QList<QVariant> &results);
    void onTreeSelectionChanged();
    void onEnableCrosspointsToggled(bool enabled);
    void onActivityTimeout();
    void onActivityTimerTick();
    void onCrosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupMenu();
    void setupToolBar();
    void setupStatusBar();
    void createDockWindows();

    void loadSettings();
    void saveSettings();
    
    void logMessage(const QString &message);
    QTreeWidgetItem* findOrCreateTreeItem(const QString &path);
    void clearTree();
    void resetActivityTimer();
    void updateCrosspointsStatusBar();
    void updatePropertyPanelBackground();

    // UI Components
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
    
    // Crosspoint editing
    QAction *m_enableCrosspointsAction;
    QTimer *m_activityTimer;
    QTimer *m_tickTimer;
    int m_activityTimeRemaining;
    QLabel *m_crosspointsStatusLabel;
    
    // State
    bool m_isConnected;
    bool m_showOidPath;
    bool m_crosspointsEnabled;
};

#endif // MAINWINDOW_H


/*
 * MainWindow.h - Main application window
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QDockWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QStatusBar>

class EmberConnection;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onConnectionStateChanged(bool connected);
    void onLogMessage(const QString &message);
    void onTreeItemReceived(const QString &path, const QString &identifier, const QString &description);

private:
    void setupUi();
    void setupMenu();
    void setupToolBar();
    void setupStatusBar();
    void createDockWindows();
    
    void logMessage(const QString &message);

    // UI Components
    QTreeWidget *m_treeWidget;
    QWidget *m_propertyPanel;
    QTextEdit *m_consoleLog;
    QDockWidget *m_consoleDock;
    QDockWidget *m_propertyDock;
    
    // Connection controls
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_statusLabel;
    
    // Ember+ connection
    EmberConnection *m_connection;
    
    // State
    bool m_isConnected;
};

#endif // MAINWINDOW_H


/*
    EmberViewer - Ember+ Device Emulator Window
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef EMULATORWINDOW_H
#define EMULATORWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>

class EmberProvider;
class DeviceSnapshot;

class EmulatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EmulatorWindow(QWidget *parent = nullptr);
    ~EmulatorWindow();

private slots:
    void onLoadSnapshot();
    void onStartServer();
    void onStopServer();
    void onServerStateChanged(bool running);
    void onClientConnected(const QString &address);
    void onClientDisconnected(const QString &address);
    void onRequestReceived(const QString &path, const QString &command);
    void onErrorOccurred(const QString &error);

private:
    void setupUi();
    void setupMenu();
    void loadSnapshotData(const DeviceSnapshot &snapshot);
    void updateServerStatus();
    void logActivity(const QString &message);
    
    // Widgets
    QTreeWidget *m_deviceTree;
    QTextEdit *m_activityLog;
    QListWidget *m_clientList;
    QSplitter *m_mainSplitter;
    QGroupBox *m_deviceGroup;
    QGroupBox *m_statusGroup;
    QGroupBox *m_activityGroup;
    
    // Controls
    QPushButton *m_loadButton;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QSpinBox *m_portSpin;
    QLabel *m_statusLabel;
    QLabel *m_deviceNameLabel;
    
    // Provider
    EmberProvider *m_provider;
    
    // State
    bool m_isRunning;
    QString m_loadedSnapshotPath;
    QString m_deviceName;
    
    // Constants
    static constexpr int DEFAULT_EMULATOR_PORT = 9099;
};

#endif // EMULATORWINDOW_H

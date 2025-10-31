/*
    EmberViewer - Snapshot Manager
    
    Handles device snapshot capture and tree fetch orchestration
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef SNAPSHOTMANAGER_H
#define SNAPSHOTMANAGER_H

#include <QObject>
#include <QProgressDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QMap>

class EmberConnection;
class DeviceSnapshot;
class MatrixManager;

// Forward declaration of MainWindow's internal type
struct FunctionInfo {
    QString identifier;
    QString description;
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
};

class SnapshotManager : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotManager(QTreeWidget* treeWidget,
                            EmberConnection* connection,
                            MatrixManager* matrixManager,
                            QWidget* parent = nullptr);
    ~SnapshotManager();

    // Initiate snapshot save with optional tree fetch
    void saveSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin);
    
    // Capture current tree state as snapshot
    DeviceSnapshot captureSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin, 
                                   const QMap<QString, FunctionInfo>& functions);

signals:
    void snapshotCaptured(const DeviceSnapshot& snapshot);
    void snapshotSaved(const QString& fileName);
    void snapshotError(const QString& error);

private slots:
    void onTreeFetchProgress(int fetchedCount, int totalCount);
    void onTreeFetchCompleted(bool success, const QString& message);

private:
    void proceedWithSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin);
    QString generateDefaultFilename(const QString& deviceName);
    
    QTreeWidget* m_treeWidget;
    EmberConnection* m_connection;
    MatrixManager* m_matrixManager;
    QProgressDialog* m_treeFetchProgress;
    
    // Store host/port for use after tree fetch
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;
};

#endif // SNAPSHOTMANAGER_H

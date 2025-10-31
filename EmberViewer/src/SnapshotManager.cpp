/*
    EmberViewer - Snapshot Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "SnapshotManager.h"
#include "EmberConnection.h"
#include "DeviceSnapshot.h"
#include "MatrixManager.h"
#include "MatrixWidget.h"
#include "EmberDataTypes.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QTreeWidgetItemIterator>

SnapshotManager::SnapshotManager(QTreeWidget* treeWidget,
                                 EmberConnection* connection,
                                 MatrixManager* matrixManager,
                                 QWidget* parent)
    : QObject(parent)
    , m_treeWidget(treeWidget)
    , m_connection(connection)
    , m_matrixManager(matrixManager)
    , m_treeFetchProgress(nullptr)
    , m_hostEdit(nullptr)
    , m_portSpin(nullptr)
{
    // Connect to tree fetch signals
    connect(m_connection, &EmberConnection::treeFetchProgress, 
            this, &SnapshotManager::onTreeFetchProgress);
    connect(m_connection, &EmberConnection::treeFetchCompleted, 
            this, &SnapshotManager::onTreeFetchCompleted);
}

SnapshotManager::~SnapshotManager()
{
    if (m_treeFetchProgress) {
        delete m_treeFetchProgress;
    }
}

void SnapshotManager::saveSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin)
{
    // Store for later use (after tree fetch)
    m_hostEdit = hostEdit;
    m_portSpin = portSpin;
    
    // Ask user if they want to fetch the complete tree first
    QMessageBox::StandardButton reply = QMessageBox::question(
        qobject_cast<QWidget*>(parent()), 
        "Complete Device Tree",
        "Do you want to fetch the complete device tree before saving?\n\n"
        "YES: Ensures complete snapshot (recommended, may take 10-30 seconds)\n"
        "NO: Save only currently loaded nodes (faster, may be incomplete)",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);  // Default to Yes
    
    if (reply == QMessageBox::Yes) {
        // Get all current tree items
        QStringList allPaths;
        QTreeWidgetItemIterator it(m_treeWidget);
        while (*it) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            if (!path.isEmpty()) {
                allPaths.append(path);
            }
            ++it;
        }
        
        if (allPaths.isEmpty()) {
            QMessageBox::warning(qobject_cast<QWidget*>(parent()), 
                               "No Data", "No device data to save.");
            return;
        }
        
        // Create and show progress dialog
        m_treeFetchProgress = new QProgressDialog(
            "Fetching complete device tree...",
            "Cancel",
            0, 100,
            qobject_cast<QWidget*>(parent()));
        m_treeFetchProgress->setWindowTitle("Complete Tree Fetch");
        m_treeFetchProgress->setWindowModality(Qt::WindowModal);
        m_treeFetchProgress->setMinimumDuration(0);  // Show immediately
        m_treeFetchProgress->setValue(0);
        
        // Connect cancel button
        connect(m_treeFetchProgress, &QProgressDialog::canceled, 
                m_connection, &EmberConnection::cancelTreeFetch);
        
        // Start the fetch
        m_connection->fetchCompleteTree(allPaths);
        
        // Wait for completion (handled by onTreeFetchCompleted)
        return;
    }
    
    // User chose NO - proceed with current tree
    proceedWithSnapshot(hostEdit, portSpin);
}

QString SnapshotManager::generateDefaultFilename(const QString& deviceName)
{
    // Sanitize device name for filename (remove invalid chars)
    static const QRegularExpression invalidChars("[^a-zA-Z0-9_.-]");
    QString sanitized = deviceName;
    sanitized = sanitized.replace(invalidChars, "_");
    
    // If empty after sanitization, use generic name
    if (sanitized.isEmpty()) {
        sanitized = "ember_device";
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("ddMMyyyy");
    return QString("%1_%2.json").arg(sanitized).arg(timestamp);
}

void SnapshotManager::proceedWithSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin)
{
    // Generate default filename using actual device name from tree
    QString deviceName;
    
    // Try to get device name from first root node in tree
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstRoot = m_treeWidget->topLevelItem(0);
        deviceName = firstRoot->text(0);
    }
    
    // If no tree items or empty name, fall back to hostname
    if (deviceName.isEmpty()) {
        deviceName = hostEdit->text();
    }
    
    QString defaultName = generateDefaultFilename(deviceName);
    
    QString fileName = QFileDialog::getSaveFileName(
        qobject_cast<QWidget*>(parent()),
        "Save Ember Device",
        defaultName,
        "Ember Device Files (*.json);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;  // User cancelled
    }
    
    // Capture the snapshot (need to pass functions map - will handle in MainWindow integration)
    // For now, create with empty functions
    QMap<QString, FunctionInfo> emptyFunctions;
    DeviceSnapshot snapshot = captureSnapshot(hostEdit, portSpin, emptyFunctions);
    
    // Save to file
    if (snapshot.saveToFile(fileName)) {
        QString successMsg = QString("Device saved successfully!\n\n"
            "%1 nodes\n%2 parameters\n%3 matrices\n%4 functions")
            .arg(snapshot.nodeCount())
            .arg(snapshot.parameterCount())
            .arg(snapshot.matrixCount())
            .arg(snapshot.functionCount());
        
        QMessageBox::information(qobject_cast<QWidget*>(parent()), 
                                "Save Successful", successMsg);
        emit snapshotSaved(fileName);
    } else {
        QMessageBox::critical(qobject_cast<QWidget*>(parent()), 
                            "Save Failed", 
                            "Failed to save device to file.");
        emit snapshotError("Failed to save to file");
    }
}

DeviceSnapshot SnapshotManager::captureSnapshot(QLineEdit* hostEdit, QSpinBox* portSpin,
                                                const QMap<QString, FunctionInfo>& functions)
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
        snapshot.deviceName = hostEdit->text();
    }
    
    snapshot.hostAddress = hostEdit->text();
    snapshot.port = portSpin->value();
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
            paramData.streamIdentifier = (*it)->data(0, Qt::UserRole + 9).toInt();
            
            snapshot.parameters[path] = paramData;
            
        } else if (type == "Matrix") {
            // Get matrix widget to extract connection data
            MatrixWidget* matrixWidget = m_matrixManager->getMatrix(path);
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
                
                // Get actual target and source indices
                matrixData.targetNumbers = matrixWidget->getTargetNumbers();
                matrixData.sourceNumbers = matrixWidget->getSourceNumbers();
                
                // Extract labels
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
                
                // Extract connections
                for (int targetIdx : matrixData.targetNumbers) {
                    for (int sourceIdx : matrixData.sourceNumbers) {
                        bool connected = matrixWidget->isConnected(targetIdx, sourceIdx);
                        matrixData.connections[{targetIdx, sourceIdx}] = connected;
                    }
                }
                
                snapshot.matrices[path] = matrixData;
            }
            
        } else if (type == "Function") {
            if (functions.contains(path)) {
                FunctionInfo funcInfo = functions[path];
                
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
    
    // Collect root paths in order
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
    
    emit snapshotCaptured(snapshot);
    return snapshot;
}

void SnapshotManager::onTreeFetchProgress(int fetchedCount, int totalCount)
{
    if (m_treeFetchProgress) {
        // Update progress (0-100%)
        int percent = totalCount > 0 ? (fetchedCount * 100 / totalCount) : 0;
        m_treeFetchProgress->setValue(percent);
        m_treeFetchProgress->setLabelText(
            QString("Fetching complete device tree...\n%1 of %2 nodes fetched")
                .arg(fetchedCount)
                .arg(totalCount));
    }
}

void SnapshotManager::onTreeFetchCompleted(bool success, const QString &message)
{
    // Close progress dialog
    if (m_treeFetchProgress) {
        m_treeFetchProgress->close();
        delete m_treeFetchProgress;
        m_treeFetchProgress = nullptr;
    }
    
    if (success) {
        qInfo() << "Tree fetch completed:" << message;
        // Continue with snapshot
        proceedWithSnapshot(m_hostEdit, m_portSpin);
    } else {
        qWarning() << "Tree fetch failed:" << message;
        QMessageBox::warning(qobject_cast<QWidget*>(parent()),
                           "Tree Fetch Failed",
                           QString("Failed to fetch complete tree:\n%1\n\nProceeding with current tree.").arg(message));
        // Proceed anyway with what we have
        proceedWithSnapshot(m_hostEdit, m_portSpin);
    }
}

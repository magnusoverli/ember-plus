/*
    EmberViewer - Matrix Manager Implementation
    
    Manages MatrixWidget lifecycle, matrix metadata, and label/connection updates.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "MatrixManager.h"
#include "MatrixWidget.h"
#include "EmberConnection.h"
#include <QDebug>

MatrixManager::MatrixManager(EmberConnection *connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
}

MatrixManager::~MatrixManager()
{
    qDeleteAll(m_matrixWidgets);
}

MatrixWidget* MatrixManager::getMatrix(const QString &path) const
{
    return m_matrixWidgets.value(path, nullptr);
}

void MatrixManager::clear()
{
    qDeleteAll(m_matrixWidgets);
    m_matrixWidgets.clear();
}

void MatrixManager::onMatrixReceived(const QString &path, int /* number */, const QString &identifier, 
                                     const QString &description, int type, int targetCount, int sourceCount)
{
    // Create or update the matrix widget
    MatrixWidget *matrixWidget = m_matrixWidgets.value(path, nullptr);
    if (!matrixWidget) {
        matrixWidget = new MatrixWidget();
        m_matrixWidgets[path] = matrixWidget;
        matrixWidget->setMatrixPath(path);
        
        qInfo().noquote() << QString("Matrix widget created: %1 (%2×%3)").arg(identifier).arg(sourceCount).arg(targetCount);
        
        emit matrixWidgetCreated(path, matrixWidget);
    }
    
    matrixWidget->setMatrixInfo(identifier, description, type, targetCount, sourceCount);
}

void MatrixManager::onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label)
{
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->setTargetLabel(targetNumber, label);
    }
}

void MatrixManager::onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label)
{
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->setSourceLabel(sourceNumber, label);
    }
}

void MatrixManager::onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition)
{
    QString dispositionStr;
    switch (disposition) {
        case 0: dispositionStr = "Tally"; break;
        case 1: dispositionStr = "Modified"; break;
        case 2: dispositionStr = "Pending"; break;
        case 3: dispositionStr = "Locked"; break;
        default: dispositionStr = QString("Unknown(%1)").arg(disposition); break;
    }
    
    qDebug().noquote() << QString("Connection received - Matrix [%1], Target %2, Source %3, Connected: %4, Disposition: %5")
               .arg(matrixPath).arg(targetNumber).arg(sourceNumber).arg(connected ? "YES" : "NO").arg(dispositionStr);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        qDebug().noquote() << QString("Found matrix widget, calling setConnection()");
        matrixWidget->setConnection(targetNumber, sourceNumber, connected, disposition);
    } else {
        qWarning().noquote() << QString("No matrix widget found for path [%1]").arg(matrixPath);
    }
}

void MatrixManager::onMatrixConnectionsCleared(const QString &matrixPath)
{
    qDebug().noquote() << QString("Clearing all connections for matrix %1").arg(matrixPath);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->clearConnections();
        qDebug().noquote() << QString("Connections cleared for matrix %1").arg(matrixPath);
    }
}

void MatrixManager::onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber)
{
    qDebug().noquote() << QString("Clearing connections for target %1 in matrix %2").arg(targetNumber).arg(matrixPath);
    
    MatrixWidget *matrixWidget = m_matrixWidgets.value(matrixPath, nullptr);
    if (matrixWidget) {
        matrixWidget->clearTargetConnections(targetNumber);
        qDebug().noquote() << QString("Target %1 connections cleared for matrix %2").arg(targetNumber).arg(matrixPath);
    }
}











#include "MatrixManager.h"
#include "VirtualizedMatrixWidget.h"
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

QWidget* MatrixManager::getMatrix(const QString &path) const
{
    return m_matrixWidgets.value(path, nullptr);
}

void MatrixManager::clear()
{
    qDeleteAll(m_matrixWidgets);
    m_matrixWidgets.clear();
}

void MatrixManager::onMatrixReceived(const QString &path, int , const QString &identifier, 
                                     const QString &description, int type, int targetCount, int sourceCount)
{
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(path, nullptr));
    bool isNew = false;
    bool dimensionsChanged = false;
    
    if (!widget) {
        // Always use virtualized widget for all matrices (efficient for any size)
        int totalCrosspoints = targetCount * sourceCount;
        qInfo().noquote() << QString("Creating VIRTUALIZED matrix widget: %1 (%2×%3 = %4 crosspoints)")
            .arg(identifier).arg(sourceCount).arg(targetCount).arg(totalCrosspoints);
        
        widget = new VirtualizedMatrixWidget();
        m_matrixWidgets[path] = widget;
        isNew = true;
    }
    
    // Get old dimensions for change detection
    int oldTargetCount = widget->getTargetNumbers().size();
    int oldSourceCount = widget->getSourceNumbers().size();
    
    if (!isNew && (oldTargetCount == 0 || oldSourceCount == 0) && (targetCount > 0 && sourceCount > 0)) {
        dimensionsChanged = true;
        qInfo().noquote() << QString("Matrix dimensions updated: %1 (%2×%3 -> %4×%5)")
            .arg(identifier).arg(oldSourceCount).arg(oldTargetCount).arg(sourceCount).arg(targetCount);
    }
    
    // Set matrix info
    widget->setMatrixPath(path);
    widget->setMatrixInfo(identifier, description, type, targetCount, sourceCount);
    
    if (isNew) {
        emit matrixWidgetCreated(path, widget);
    } else if (dimensionsChanged) {
        emit matrixDimensionsUpdated(path, widget);
    }
}

void MatrixManager::onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label)
{
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(matrixPath, nullptr));
    if (widget) {
        widget->setTargetLabel(targetNumber, label);
    }
}

void MatrixManager::onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label)
{
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(matrixPath, nullptr));
    if (widget) {
        widget->setSourceLabel(sourceNumber, label);
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
    
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(matrixPath, nullptr));
    if (widget) {
        qDebug().noquote() << QString("Found matrix widget, calling setConnection()");
        widget->setConnection(targetNumber, sourceNumber, connected, disposition);
    } else {
        qWarning().noquote() << QString("No matrix widget found for path [%1]").arg(matrixPath);
    }
}

void MatrixManager::onMatrixConnectionsCleared(const QString &matrixPath)
{
    qDebug().noquote() << QString("Clearing all connections for matrix %1").arg(matrixPath);
    
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(matrixPath, nullptr));
    if (widget) {
        widget->clearConnections();
        qDebug().noquote() << QString("Connections cleared for matrix %1").arg(matrixPath);
    }
}

void MatrixManager::onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber)
{
    qDebug().noquote() << QString("Clearing connections for target %1 in matrix %2").arg(targetNumber).arg(matrixPath);
    
    VirtualizedMatrixWidget *widget = qobject_cast<VirtualizedMatrixWidget*>(m_matrixWidgets.value(matrixPath, nullptr));
    if (widget) {
        widget->clearTargetConnections(targetNumber);
        qDebug().noquote() << QString("Target %1 connections cleared for matrix %2").arg(targetNumber).arg(matrixPath);
    }
}

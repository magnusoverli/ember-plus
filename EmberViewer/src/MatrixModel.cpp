/*
    MatrixModel.cpp - Implementation of pure data model for matrix state
*/

#include "MatrixModel.h"
#include <QDebug>

MatrixModel::MatrixModel(QObject *parent)
    : QObject(parent)
    , m_matrixType(2)  // Default to N:N
    , m_targetCount(0)
    , m_sourceCount(0)
{
}

MatrixModel::~MatrixModel()
{
}

void MatrixModel::setMatrixInfo(const QString &identifier, const QString &description, 
                                  int type, int targetCount, int sourceCount)
{
    m_identifier = identifier;
    m_description = description;
    m_matrixType = type;
    
    bool dimensionsChanged = (m_targetCount != targetCount || m_sourceCount != sourceCount);
    bool wasEmpty = (m_targetCount == 0 || m_sourceCount == 0);
    bool nowPopulated = (targetCount > 0 && sourceCount > 0);
    
    m_targetCount = targetCount;
    m_sourceCount = sourceCount;
    
    // Auto-generate target/source lists if dimensions changed from empty to populated
    if (dimensionsChanged && wasEmpty && nowPopulated) {
        if (m_targetNumbers.isEmpty() && targetCount > 0) {
            m_targetNumbers.clear();
            for (int i = 0; i < targetCount; ++i) {
                m_targetNumbers.append(i);
                if (!m_targetLabels.contains(i)) {
                    m_targetLabels[i] = QString("Target %1").arg(i);
                }
            }
        }
        
        if (m_sourceNumbers.isEmpty() && sourceCount > 0) {
            m_sourceNumbers.clear();
            for (int i = 0; i < sourceCount; ++i) {
                m_sourceNumbers.append(i);
                if (!m_sourceLabels.contains(i)) {
                    m_sourceLabels[i] = QString("Source %1").arg(i);
                }
            }
        }
    }

    emit dataChanged();
}

void MatrixModel::setMatrixPath(const QString &path)
{
    m_matrixPath = path;
}

void MatrixModel::setTargetNumbers(const QList<int> &numbers)
{
    m_targetNumbers = numbers;
    emit dataChanged();
}

void MatrixModel::setSourceNumbers(const QList<int> &numbers)
{
    m_sourceNumbers = numbers;
    emit dataChanged();
}

void MatrixModel::setTargetLabel(int targetNumber, const QString &label)
{
    m_targetLabels[targetNumber] = label;
    
    // Add to numbers list if not present
    if (!m_targetNumbers.contains(targetNumber)) {
        m_targetNumbers.append(targetNumber);
        std::sort(m_targetNumbers.begin(), m_targetNumbers.end());
    }
    
    emit dataChanged();
}

void MatrixModel::setSourceLabel(int sourceNumber, const QString &label)
{
    m_sourceLabels[sourceNumber] = label;
    
    // Add to numbers list if not present
    if (!m_sourceNumbers.contains(sourceNumber)) {
        m_sourceNumbers.append(sourceNumber);
        std::sort(m_sourceNumbers.begin(), m_sourceNumbers.end());
    }
    
    emit dataChanged();
}

void MatrixModel::setConnection(int targetNumber, int sourceNumber, bool connected, int disposition)
{
    QPair<int, int> key(targetNumber, sourceNumber);
    
    if (connected) {
        ConnectionState state;
        state.connected = true;
        state.disposition = disposition;
        m_connections[key] = state;
    } else {
        m_connections.remove(key);
    }
    
    emit connectionChanged(targetNumber, sourceNumber, connected);
}

void MatrixModel::clearConnections()
{
    m_connections.clear();
    emit dataChanged();
}

void MatrixModel::clearTargetConnections(int targetNumber)
{
    QList<QPair<int, int>> keysToRemove;
    
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.key().first == targetNumber) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const auto &key : keysToRemove) {
        m_connections.remove(key);
    }
    
    emit dataChanged();
}

QString MatrixModel::targetLabel(int targetNumber) const
{
    return m_targetLabels.value(targetNumber, QString("Target %1").arg(targetNumber));
}

QString MatrixModel::sourceLabel(int sourceNumber) const
{
    return m_sourceLabels.value(sourceNumber, QString("Source %1").arg(sourceNumber));
}

bool MatrixModel::isConnected(int targetNumber, int sourceNumber) const
{
    QPair<int, int> key(targetNumber, sourceNumber);
    return m_connections.contains(key) && m_connections[key].connected;
}

int MatrixModel::connectionDisposition(int targetNumber, int sourceNumber) const
{
    QPair<int, int> key(targetNumber, sourceNumber);
    if (m_connections.contains(key)) {
        return m_connections[key].disposition;
    }
    return 0;
}

QList<QPair<int, int>> MatrixModel::getAllConnections() const
{
    QList<QPair<int, int>> result;
    
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value().connected) {
            result.append(it.key());
        }
    }
    
    return result;
}


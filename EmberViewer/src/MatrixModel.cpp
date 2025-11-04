

#include "MatrixModel.h"
#include <QDebug>

MatrixModel::MatrixModel(QObject *parent)
    : QObject(parent)
    , m_matrixType(2)  
    , m_targetCount(0)
    , m_sourceCount(0)
    , m_updatesDeferred(false)
    , m_hasPendingUpdate(false)
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
    
    
    if (dimensionsChanged && wasEmpty && nowPopulated) {
        if (m_targetNumbers.isEmpty() && targetCount > 0) {
            qDebug().noquote() << QString("MatrixModel: AUTO-GENERATING placeholder labels for %1 targets (matrix: %2)")
                .arg(targetCount).arg(m_matrixPath);
            
            m_targetNumbers.clear();
            for (int i = 0; i < targetCount; ++i) {
                m_targetNumbers.append(i);
                if (!m_targetLabels.contains(i)) {
                    m_targetLabels[i] = QString("Target %1").arg(i);
                }
            }
        }
        
        if (m_sourceNumbers.isEmpty() && sourceCount > 0) {
            qDebug().noquote() << QString("MatrixModel: AUTO-GENERATING placeholder labels for %1 sources (matrix: %2)")
                .arg(sourceCount).arg(m_matrixPath);
            
            m_sourceNumbers.clear();
            for (int i = 0; i < sourceCount; ++i) {
                m_sourceNumbers.append(i);
                if (!m_sourceLabels.contains(i)) {
                    m_sourceLabels[i] = QString("Source %1").arg(i);
                }
            }
        }
    }

    emitDataChangedIfNotDeferred();
}

void MatrixModel::setMatrixPath(const QString &path)
{
    m_matrixPath = path;
}

void MatrixModel::setTargetNumbers(const QList<int> &numbers)
{
    m_targetNumbers = numbers;
    emitDataChangedIfNotDeferred();
}

void MatrixModel::setSourceNumbers(const QList<int> &numbers)
{
    m_sourceNumbers = numbers;
    emitDataChangedIfNotDeferred();
}

void MatrixModel::setTargetLabel(int targetNumber, const QString &label)
{
    
    
    QString currentLabel = m_targetLabels.value(targetNumber);
    bool isPlaceholder = currentLabel.isEmpty() || currentLabel.startsWith("Target ");
    
    if (isPlaceholder) {
        qDebug().noquote() << QString("MatrixModel: setTargetLabel - Target: %1, Label: '%2', Matrix: %3")
            .arg(targetNumber).arg(label).arg(m_matrixPath);
        
        m_targetLabels[targetNumber] = label;
        
        
        // Use QSet for O(1) lookup instead of QList::contains() which is O(n)
        if (!m_targetNumbersSet.contains(targetNumber)) {
            m_targetNumbersSet.insert(targetNumber);
            // Only append, don't sort yet (will be sorted in endBatchUpdate)
            if (!m_updatesDeferred) {
                m_targetNumbers.append(targetNumber);
                std::sort(m_targetNumbers.begin(), m_targetNumbers.end());
            }
        }
        
        emitDataChangedIfNotDeferred();
    } else {
        qDebug().noquote() << QString("MatrixModel: SKIPPED setTargetLabel - Target: %1 already has label '%2', ignoring '%3'")
            .arg(targetNumber).arg(currentLabel).arg(label);
    }
}

void MatrixModel::setSourceLabel(int sourceNumber, const QString &label)
{
    
    
    QString currentLabel = m_sourceLabels.value(sourceNumber);
    bool isPlaceholder = currentLabel.isEmpty() || currentLabel.startsWith("Source ");
    
    if (isPlaceholder) {
        qDebug().noquote() << QString("MatrixModel: setSourceLabel - Source: %1, Label: '%2', Matrix: %3")
            .arg(sourceNumber).arg(label).arg(m_matrixPath);
        
        m_sourceLabels[sourceNumber] = label;
        
        
        // Use QSet for O(1) lookup instead of QList::contains() which is O(n)
        if (!m_sourceNumbersSet.contains(sourceNumber)) {
            m_sourceNumbersSet.insert(sourceNumber);
            // Only append, don't sort yet (will be sorted in endBatchUpdate)
            if (!m_updatesDeferred) {
                m_sourceNumbers.append(sourceNumber);
                std::sort(m_sourceNumbers.begin(), m_sourceNumbers.end());
            }
        }
        
        emitDataChangedIfNotDeferred();
    } else {
        qDebug().noquote() << QString("MatrixModel: SKIPPED setSourceLabel - Source: %1 already has label '%2', ignoring '%3'")
            .arg(sourceNumber).arg(currentLabel).arg(label);
    }
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
    emitDataChangedIfNotDeferred();
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
    
    emitDataChangedIfNotDeferred();
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

void MatrixModel::setUpdatesDeferred(bool deferred)
{
    m_updatesDeferred = deferred;
    
    // When re-enabling updates, emit pending dataChanged if needed
    if (!deferred && m_hasPendingUpdate) {
        m_hasPendingUpdate = false;
        emit dataChanged();
    }
}

void MatrixModel::beginBatchUpdate()
{
    m_updatesDeferred = true;
    m_hasPendingUpdate = false;
    
    // Initialize sets with current numbers for fast lookups during batch
    m_targetNumbersSet = QSet<int>(m_targetNumbers.begin(), m_targetNumbers.end());
    m_sourceNumbersSet = QSet<int>(m_sourceNumbers.begin(), m_sourceNumbers.end());
}

void MatrixModel::endBatchUpdate()
{
    m_updatesDeferred = false;
    
    // Rebuild sorted lists from sets (only contains numbers added during batch)
    m_targetNumbers = m_targetNumbersSet.values();
    m_sourceNumbers = m_sourceNumbersSet.values();
    std::sort(m_targetNumbers.begin(), m_targetNumbers.end());
    std::sort(m_sourceNumbers.begin(), m_sourceNumbers.end());
    
    if (m_hasPendingUpdate) {
        m_hasPendingUpdate = false;
        emit dataChanged();
    }
}

void MatrixModel::emitDataChangedIfNotDeferred()
{
    if (m_updatesDeferred) {
        m_hasPendingUpdate = true;
    } else {
        emit dataChanged();
    }
}


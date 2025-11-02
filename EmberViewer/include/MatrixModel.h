/*
    MatrixModel.h - Pure data model for matrix state
    Stores connections, labels, and metadata without any UI components
*/

#ifndef MATRIXMODEL_H
#define MATRIXMODEL_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>
#include <QPair>

class MatrixModel : public QObject
{
    Q_OBJECT

public:
    explicit MatrixModel(QObject *parent = nullptr);
    ~MatrixModel();

    // Matrix configuration
    void setMatrixInfo(const QString &identifier, const QString &description, 
                       int type, int targetCount, int sourceCount);
    void setMatrixPath(const QString &path);

    // Target/Source management
    void setTargetNumbers(const QList<int> &numbers);
    void setSourceNumbers(const QList<int> &numbers);
    void setTargetLabel(int targetNumber, const QString &label);
    void setSourceLabel(int sourceNumber, const QString &label);

    // Connection management
    void setConnection(int targetNumber, int sourceNumber, bool connected, int disposition = 0);
    void clearConnections();
    void clearTargetConnections(int targetNumber);

    // Getters - Matrix info
    QString identifier() const { return m_identifier; }
    QString description() const { return m_description; }
    QString matrixPath() const { return m_matrixPath; }
    int matrixType() const { return m_matrixType; }
    int targetCount() const { return m_targetCount; }
    int sourceCount() const { return m_sourceCount; }

    // Getters - Targets/Sources
    const QList<int>& targetNumbers() const { return m_targetNumbers; }
    const QList<int>& sourceNumbers() const { return m_sourceNumbers; }
    QString targetLabel(int targetNumber) const;
    QString sourceLabel(int sourceNumber) const;

    // Getters - Connections
    bool isConnected(int targetNumber, int sourceNumber) const;
    int connectionDisposition(int targetNumber, int sourceNumber) const;
    QList<QPair<int, int>> getAllConnections() const;

signals:
    void dataChanged();
    void connectionChanged(int targetNumber, int sourceNumber, bool connected);

private:
    // Matrix metadata
    QString m_identifier;
    QString m_description;
    QString m_matrixPath;
    int m_matrixType;
    int m_targetCount;
    int m_sourceCount;

    // Target/Source data
    QList<int> m_targetNumbers;
    QList<int> m_sourceNumbers;
    QHash<int, QString> m_targetLabels;
    QHash<int, QString> m_sourceLabels;

    // Connection state - sparse storage (only stores connected crosspoints)
    struct ConnectionState {
        bool connected;
        int disposition;
    };
    QHash<QPair<int, int>, ConnectionState> m_connections;
};

// Hash function for QPair<int, int> to use in QHash
inline uint qHash(const QPair<int, int> &key, uint seed = 0)
{
    return qHash(key.first, seed) ^ qHash(key.second, seed);
}

#endif // MATRIXMODEL_H


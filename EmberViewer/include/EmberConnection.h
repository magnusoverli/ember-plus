







#ifndef EMBERCONNECTION_H
#define EMBERCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>
#include <QMap>
#include <QTimer>
#include <QDateTime>
#include "S101Protocol.h"
#include "GlowParser.h"
#include "EmberDataTypes.h"


class S101Protocol;
class GlowParser;
class TreeFetchService;
class CacheManager;

class EmberConnection : public QObject
{
    Q_OBJECT



public:
    
    struct SubscriptionRequest {
        QString path;
        QString type;  
    };

    explicit EmberConnection(QObject *parent = nullptr);
    ~EmberConnection();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;
    
    
    void sendParameterValue(const QString &path, const QString &value, int type);
    
    
    void setMatrixConnection(const QString &matrixPath, int targetNumber, int sourceNumber, bool connect);
    
    
    void invokeFunction(const QString &path, const QList<QVariant> &arguments);
    
    
    void sendGetDirectoryForPath(const QString& path, bool optimizedForNameDiscovery = false);
    
    
    void sendBatchGetDirectory(const QStringList& paths, bool optimizedForNameDiscovery = false);
    
    
    void sendBatchSubscribe(const QList<SubscriptionRequest>& requests);
    
    
    void fetchCompleteTree(const QStringList &initialNodePaths);
    void cancelTreeFetch();
    bool isTreeFetchActive() const;

signals:
    void connected();
    void disconnected();
    void treePopulated();  
    void nodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void parameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                          int access, int type, const QVariant &minimum, const QVariant &maximum,
                          const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier,
                          const QString &format, const QString &referenceLevel, const QString &formula, int factor);
    void matrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                       int type, int targetCount, int sourceCount);
    void matrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void matrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void matrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void matrixConnectionsCleared(const QString &matrixPath);
    void matrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber);
    void functionReceived(const QString &path, const QString &identifier, const QString &description,
                         const QStringList &argNames, const QList<int> &argTypes,
                         const QStringList &resultNames, const QList<int> &resultTypes);
    void invocationResultReceived(int invocationId, bool success, const QList<QVariant> &results);
    void streamValueReceived(int streamIdentifier, double value);  
    
    
    void treeFetchProgress(int fetchedCount, int totalCount);
    void treeFetchCompleted(bool success, const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onDataReceived();
    void onConnectionTimeout();
    void onProtocolTimeout();

private:
    
    struct ParameterCache {
        QString identifier;  
        int access = -1;     
        int type = 0;        
    };
    
    
    void onS101MessageReceived(const QByteArray& emberData);
    void onKeepAliveReceived();
    void onParserNodeReceived(const EmberData::NodeInfo& node);
    void onParserParameterReceived(const EmberData::ParameterInfo& param);
    void onParserMatrixReceived(const EmberData::MatrixInfo& matrix);
    void sendGetDirectory();
    
    
    bool isGenericNodeName(const QString &name);

    QTcpSocket *m_socket;
    
    
    S101Protocol *m_s101Protocol;
    GlowParser *m_glowParser;
    
    
    TreeFetchService *m_treeFetchService;
    CacheManager *m_cacheManager;
    
    
    QString m_host;
    int m_port;
    bool m_connected;
    bool m_emberDataReceived;  
    
    
    QTimer *m_connectionTimer;
    QTimer *m_protocolTimer;  
    
    
    QSet<QString> m_requestedPaths;
    
    // Track matrix label basePaths to automatically expand their children
    QSet<QString> m_labelBasePaths;
    
    
    int m_nextInvocationId;
    QMap<int, QString> m_pendingInvocations;  
    
    
    struct SubscriptionState {
        QDateTime subscribedAt;
        bool autoSubscribed;  
    };
    QMap<QString, SubscriptionState> m_subscriptions;  
    


public:
    
    void subscribeToParameter(const QString &path, bool autoSubscribed = false);
    void subscribeToNode(const QString &path, bool autoSubscribed = false);
    void subscribeToMatrix(const QString &path, bool autoSubscribed = false);
    void unsubscribeFromParameter(const QString &path);
    void unsubscribeFromNode(const QString &path);
    void unsubscribeFromMatrix(const QString &path);
    bool isSubscribed(const QString &path) const;
    
    
    static constexpr int CONNECTION_TIMEOUT_MS = 5000;   
    static constexpr int PROTOCOL_TIMEOUT_MS = 10000;    
};

#endif 

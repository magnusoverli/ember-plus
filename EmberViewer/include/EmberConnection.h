/*
    EmberViewer - Ember+ protocol communication handler
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

// Forward declarations
class S101Protocol;
class GlowParser;
class TreeFetchService;
class CacheManager;

class EmberConnection : public QObject
{
    Q_OBJECT



public:
    // Subscription request for batch operations
    struct SubscriptionRequest {
        QString path;
        QString type;  // "Node", "Parameter", "Matrix", or "Function"
    };

    explicit EmberConnection(QObject *parent = nullptr);
    ~EmberConnection();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;
    
    // Send parameter value update to device
    void sendParameterValue(const QString &path, const QString &value, int type);
    
    // Send matrix connection command
    void setMatrixConnection(const QString &matrixPath, int targetNumber, int sourceNumber, bool connect);
    
    // Invoke a function with arguments
    void invokeFunction(const QString &path, const QList<QVariant> &arguments);
    
    // Request children for a specific path (for lazy loading)
    void sendGetDirectoryForPath(const QString& path, bool optimizedForNameDiscovery = false);
    
    // Batch request multiple paths in a single message (for reduced round trips)
    void sendBatchGetDirectory(const QStringList& paths, bool optimizedForNameDiscovery = false);
    
    // Batch subscribe to multiple paths in a single message (for reduced round trips)
    void sendBatchSubscribe(const QList<SubscriptionRequest>& requests);
    
    // Complete tree fetch for comprehensive snapshots
    void fetchCompleteTree(const QStringList &initialNodePaths);
    void cancelTreeFetch();
    bool isTreeFetchActive() const;

signals:
    void connected();
    void disconnected();
    void treePopulated();  // Emitted when initial tree is received
    void nodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void parameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                          int access, int type, const QVariant &minimum, const QVariant &maximum,
                          const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier);
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
    void streamValueReceived(int streamIdentifier, double value);  // For GlowStreamCollection updates
    
    // Complete tree fetch progress signals
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
    // Cached parameter metadata to preserve properties across updates
    struct ParameterCache {
        QString identifier;  // Parameter name/identifier
        int access = -1;     // -1 means unknown, use default ReadWrite (3) when first seen
        int type = 0;        // Type: 0=None
    };
    
    // Protocol layer handlers
    void onS101MessageReceived(const QByteArray& emberData);
    void onKeepAliveReceived();
    void onParserNodeReceived(const EmberData::NodeInfo& node);
    void onParserParameterReceived(const EmberData::ParameterInfo& param);
    void onParserMatrixReceived(const EmberData::MatrixInfo& matrix);
    void sendGetDirectory();
    
    // Device name detection helper
    bool isGenericNodeName(const QString &name);

    QTcpSocket *m_socket;
    
    // Protocol layer
    S101Protocol *m_s101Protocol;
    GlowParser *m_glowParser;
    
    // Services
    TreeFetchService *m_treeFetchService;
    CacheManager *m_cacheManager;
    
    // Connection state
    QString m_host;
    int m_port;
    bool m_connected;
    bool m_emberDataReceived;  // Track if we've received any valid Ember+ data
    
    // Connection timeout
    QTimer *m_connectionTimer;
    QTimer *m_protocolTimer;  // Timeout for waiting for Ember+ protocol response
    
    // Track requested paths to avoid infinite loops
    QSet<QString> m_requestedPaths;
    
    // Function invocation tracking
    int m_nextInvocationId;
    QMap<int, QString> m_pendingInvocations;  // invocationId -> function path
    
    // Subscription tracking
    struct SubscriptionState {
        QDateTime subscribedAt;
        bool autoSubscribed;  // true if auto-subscribed on expand, false if manual
    };
    QMap<QString, SubscriptionState> m_subscriptions;  // path -> subscription info
    


public:
    // Subscription methods
    void subscribeToParameter(const QString &path, bool autoSubscribed = false);
    void subscribeToNode(const QString &path, bool autoSubscribed = false);
    void subscribeToMatrix(const QString &path, bool autoSubscribed = false);
    void unsubscribeFromParameter(const QString &path);
    void unsubscribeFromNode(const QString &path);
    void unsubscribeFromMatrix(const QString &path);
    bool isSubscribed(const QString &path) const;
    
    // Constants
    static constexpr int CONNECTION_TIMEOUT_MS = 5000;   // 5 seconds for TCP connection
    static constexpr int PROTOCOL_TIMEOUT_MS = 10000;    // 10 seconds for Ember+ protocol response
};

#endif // EMBERCONNECTION_H

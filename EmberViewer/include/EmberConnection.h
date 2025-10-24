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
#include <ember/dom/AsyncDomReader.hpp>
#include <s101/StreamDecoder.hpp>

// Log levels for controlling verbosity
enum class LogLevel {
    Error = 0,    // Only errors
    Warning = 1,  // Errors and warnings
    Info = 2,     // Errors, warnings, and info (default)
    Debug = 3,    // All messages including debug
    Trace = 4     // Everything including protocol details
};

// Forward declarations
namespace libember { 
    namespace dom {
        class Node;
    }
    namespace glow {
        class GlowContainer;
        class GlowNode;
        class GlowQualifiedNode;
        class GlowParameter;
        class GlowQualifiedParameter;
        class GlowMatrix;
        class GlowQualifiedMatrix;
        class GlowFunction;
        class GlowQualifiedFunction;
    }
}

class EmberConnection : public QObject
{
    Q_OBJECT

    // Nested DomReader class
    class DomReader : public libember::dom::AsyncDomReader
    {
    public:
        explicit DomReader(EmberConnection* connection);
        virtual ~DomReader();
    
    protected:
        virtual void rootReady(libember::dom::Node* root) override;
    
    private:
        EmberConnection *m_connection;
    };

public:
    explicit EmberConnection(QObject *parent = nullptr);
    ~EmberConnection();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;
    
    // Log level control
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const { return m_logLevel; }
    
    // Send parameter value update to device
    void sendParameterValue(const QString &path, const QString &value, int type);
    
    // Send matrix connection command
    void setMatrixConnection(const QString &matrixPath, int targetNumber, int sourceNumber, bool connect);
    
    // Request matrix connection state (forces refresh)
    void requestMatrixConnections(const QString &matrixPath);
    
    // Invoke a function with arguments
    void invokeFunction(const QString &path, const QList<QVariant> &arguments);

signals:
    void connected();
    void disconnected();
    void logMessage(const QString &message);
    void treePopulated();  // Emitted when initial tree is received
    void nodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void parameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                          int access, int type, const QVariant &minimum, const QVariant &maximum,
                          const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline);
    void matrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                       int type, int targetCount, int sourceCount);
    void matrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void matrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void matrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void matrixConnectionsCleared(const QString &matrixPath);
    void functionReceived(const QString &path, const QString &identifier, const QString &description,
                         const QStringList &argNames, const QList<int> &argTypes,
                         const QStringList &resultNames, const QList<int> &resultTypes);
    void invocationResultReceived(int invocationId, bool success, const QList<QVariant> &results);

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
    
    void handleS101Message(libs101::StreamDecoder<unsigned char>::const_iterator first,
                           libs101::StreamDecoder<unsigned char>::const_iterator last);
    void processRoot(libember::dom::Node* root);
    void processElementCollection(libember::glow::GlowContainer* container, const QString& parentPath);
    void processQualifiedNode(libember::glow::GlowQualifiedNode* node);
    void processNode(libember::glow::GlowNode* node, const QString& parentPath);
    void processQualifiedParameter(libember::glow::GlowQualifiedParameter* param);
    void processParameter(libember::glow::GlowParameter* param, const QString& parentPath);
    void processQualifiedMatrix(libember::glow::GlowQualifiedMatrix* matrix);
    void processMatrix(libember::glow::GlowMatrix* matrix, const QString& parentPath);
    void processQualifiedFunction(libember::glow::GlowQualifiedFunction* function);
    void processFunction(libember::glow::GlowFunction* function, const QString& parentPath);
    void processInvocationResult(libember::dom::Node* result);
    void sendGetDirectory();
    void sendGetDirectoryForPath(const QString& path);
    
    // Logging helper
    void log(LogLevel level, const QString &message);
    
    // Device name detection helper
    bool isGenericNodeName(const QString &name);
    void checkAndUpdateDeviceName(const QString &nodePath, const QString &paramPath, const QString &value);

    QTcpSocket *m_socket;
    
    // S101 decoder
    libs101::StreamDecoder<unsigned char> *m_s101Decoder;
    
    // Glow DOM reader
    DomReader *m_domReader;
    
    // Connection state
    QString m_host;
    int m_port;
    bool m_connected;
    bool m_emberDataReceived;  // Track if we've received any valid Ember+ data
    
    // Connection timeout
    QTimer *m_connectionTimer;
    QTimer *m_protocolTimer;  // Timeout for waiting for Ember+ protocol response
    
    // Cache of parameter metadata (path -> cache)
    QMap<QString, ParameterCache> m_parameterCache;
    
    // Track requested paths to avoid infinite loops
    QSet<QString> m_requestedPaths;
    
    // Device name tracking for smart root node naming
    struct RootNodeInfo {
        QString path;           // Node path (e.g., "1")
        QString displayName;    // Current display name
        bool isGeneric;         // Whether the name is generic and needs improvement
        QString identityPath;   // Path to identity node (e.g., "1.4")
    };
    QMap<QString, RootNodeInfo> m_rootNodes;  // Track root-level nodes
    
    // Log level
    LogLevel m_logLevel;
    
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
    void unsubscribeAll();
    bool isSubscribed(const QString &path) const;
    
    // Constants
    static constexpr int CONNECTION_TIMEOUT_MS = 5000;   // 5 seconds for TCP connection
    static constexpr int PROTOCOL_TIMEOUT_MS = 3000;     // 3 seconds for Ember+ protocol response
};

#endif // EMBERCONNECTION_H

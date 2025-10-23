/*
 * EmberConnection.h - Handles Ember+ protocol communication
 */

#ifndef EMBERCONNECTION_H
#define EMBERCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>
#include <QMap>
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

signals:
    void connected();
    void disconnected();
    void logMessage(const QString &message);
    void nodeReceived(const QString &path, const QString &identifier, const QString &description);
    void parameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                          int access, int type, const QVariant &minimum, const QVariant &maximum,
                          const QStringList &enumOptions, const QList<int> &enumValues);
    void matrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                       int type, int targetCount, int sourceCount);
    void matrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void matrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void matrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected);
    void matrixConnectionsCleared(const QString &matrixPath);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onDataReceived();

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
    void sendGetDirectory();
    void sendGetDirectoryForPath(const QString& path);
    
    // Logging helper
    void log(LogLevel level, const QString &message);

    QTcpSocket *m_socket;
    
    // S101 decoder
    libs101::StreamDecoder<unsigned char> *m_s101Decoder;
    
    // Glow DOM reader
    DomReader *m_domReader;
    
    // Connection state
    QString m_host;
    int m_port;
    bool m_connected;
    
    // Cache of parameter metadata (path -> cache)
    QMap<QString, ParameterCache> m_parameterCache;
    
    // Track requested paths to avoid infinite loops
    QSet<QString> m_requestedPaths;
    
    // Log level
    LogLevel m_logLevel;
};

#endif // EMBERCONNECTION_H


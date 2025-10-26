/*
    EmberViewer - Ember+ Provider (Device Emulator)
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef EMBERPROVIDER_H
#define EMBERPROVIDER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>
#include <QMap>
#include <QList>
#include <ember/dom/AsyncDomReader.hpp>
#include <s101/StreamDecoder.hpp>

class DeviceSnapshot;
struct NodeData;
struct ParameterData;
struct MatrixData;
struct FunctionData;

// Forward declarations
namespace libember {
    namespace dom {
        class Node;
    }
    namespace glow {
        class GlowContainer;
        class GlowCommand;
        class GlowParameter;
        class GlowQualifiedParameter;
        class GlowMatrix;
    }
}

// Forward declarations
class EmberProvider;
class DomReader;

// Client connection handler (must be outside EmberProvider for Qt MOC)
class ClientConnection : public QObject
{
    Q_OBJECT
public:
    explicit ClientConnection(QTcpSocket *socket, EmberProvider *provider);
    ~ClientConnection();

    QString address() const;
    QTcpSocket* socket() { return m_socket; }
    
    void sendData(const QByteArray &data);
    void handleS101Message(libs101::StreamDecoder<unsigned char>::const_iterator first,
                          libs101::StreamDecoder<unsigned char>::const_iterator last);
    
    // Subscription tracking
    QSet<QString> subscriptions;

signals:
    void dataReceived();
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    EmberProvider *m_provider;
    libs101::StreamDecoder<unsigned char> *m_s101Decoder;
    libember::dom::AsyncDomReader *m_domReader;
    QString m_address;

    friend class EmberProvider;
    friend class DomReader;
};

// DOM Reader for parsing incoming requests
class DomReader : public libember::dom::AsyncDomReader
{
public:
    explicit DomReader(ClientConnection* client);
    virtual ~DomReader();

protected:
    virtual void rootReady(libember::dom::Node* root) override;

private:
    ClientConnection *m_client;
};

class EmberProvider : public QObject
{
    Q_OBJECT

public:
    explicit EmberProvider(QObject *parent = nullptr);
    ~EmberProvider();

    bool startListening(quint16 port);
    void stopListening();
    bool isListening() const { return m_server && m_server->isListening(); }
    
    void loadDeviceTree(const DeviceSnapshot &snapshot);
    
    // Methods needed by ClientConnection and DomReader
    void processRoot(libember::dom::Node* root, ClientConnection *client);
    void sendKeepAliveResponse(ClientConnection *client);

signals:
    void serverStateChanged(bool running);
    void clientConnected(const QString &address);
    void clientDisconnected(const QString &address);
    void requestReceived(const QString &path, const QString &command);
    void errorOccurred(const QString &error);

private slots:
    void onNewConnection();
    void onClientDisconnected();

private:
    void processCommand(libember::glow::GlowCommand* command, ClientConnection *client);
    void processParameter(libember::glow::GlowParameter* param, const QString &parentPath, ClientConnection *client);
    void processQualifiedParameter(libember::glow::GlowQualifiedParameter* param, ClientConnection *client);
    void processMatrix(libember::glow::GlowMatrix* matrix, const QString &parentPath, ClientConnection *client);
    
    void sendGetDirectoryResponse(const QString &path, ClientConnection *client);
    void sendParameterResponse(const QString &path, ClientConnection *client);
    void sendNodeResponse(const QString &path, ClientConnection *client);
    void sendMatrixResponse(const QString &path, ClientConnection *client);
    void sendFunctionResponse(const QString &path, ClientConnection *client);
    
    // Matrix label virtual node helpers
    void sendMatrixLabelNode(const QString &matrixPath, ClientConnection *client);
    void sendMatrixLabelTypeNode(const QString &containerPath, const QString &labelType, ClientConnection *client);
    void sendMatrixLabelParameters(const QString &matrixPath, const QString &labelType, ClientConnection *client);
    
    void handleSubscribe(const QString &path, ClientConnection *client);
    void handleUnsubscribe(const QString &path, ClientConnection *client);
    
    void sendEncodedMessage(const libember::glow::GlowContainer *container, ClientConnection *client);
    void broadcastToSubscribers(const QString &path, const libember::glow::GlowContainer *container);
    
    QTcpServer *m_server;
    QList<ClientConnection*> m_clients;
    
    // Device tree data (loaded from snapshot)
    QMap<QString, NodeData> m_nodes;
    QMap<QString, ParameterData> m_parameters;
    QMap<QString, MatrixData> m_matrices;
    QMap<QString, FunctionData> m_functions;
    
    // Root paths tracking
    QStringList m_rootPaths;
};

#endif // EMBERPROVIDER_H

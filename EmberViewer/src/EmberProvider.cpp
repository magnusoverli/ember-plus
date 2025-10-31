/*
    EmberViewer - Ember+ Provider (Device Emulator)
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "EmberProvider.h"
#include "DeviceSnapshot.h"
#include <ember/Ember.hpp>
#include <ember/glow/GlowRootElementCollection.hpp>
#include <ember/glow/GlowQualifiedNode.hpp>
#include <ember/glow/GlowQualifiedParameter.hpp>
#include <ember/glow/GlowQualifiedMatrix.hpp>
#include <ember/glow/GlowQualifiedFunction.hpp>
#include <ember/glow/GlowCommand.hpp>
#include <ember/glow/GlowNodeFactory.hpp>
#include <ember/glow/GlowConnection.hpp>
#include <ember/glow/GlowTarget.hpp>
#include <ember/glow/GlowSource.hpp>
#include <ember/glow/GlowTupleItemDescription.hpp>
#include <s101/StreamEncoder.hpp>
#include <s101/CommandType.hpp>
#include <s101/MessageType.hpp>
#include <s101/PackageFlag.hpp>
#include <QDebug>

// Static dispatch function for S101 decoder
static void s101MessageDispatch(libs101::StreamDecoder<unsigned char>::const_iterator first,
                                libs101::StreamDecoder<unsigned char>::const_iterator last,
                                ClientConnection* state)
{
    state->handleS101Message(first, last);
}

// ========== ClientConnection Implementation ==========

ClientConnection::ClientConnection(QTcpSocket *socket, EmberProvider *provider)
    : QObject(provider)
    , m_socket(socket)
    , m_provider(provider)
    , m_s101Decoder(new libs101::StreamDecoder<unsigned char>())
    , m_domReader(new DomReader(this))
{
    m_address = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
    
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientConnection::onDisconnected);
}

ClientConnection::~ClientConnection()
{
    if (m_socket) {
        m_socket->disconnect();
        m_socket->deleteLater();
    }
    delete m_domReader;
    delete m_s101Decoder;
}

QString ClientConnection::address() const
{
    return m_address;
}

void ClientConnection::sendData(const QByteArray &data)
{
    if (m_socket && m_socket->isWritable()) {
        m_socket->write(data);
        m_socket->flush();
    }
}

void ClientConnection::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    const unsigned char *bytes = reinterpret_cast<const unsigned char*>(data.constData());
    m_s101Decoder->read(bytes, bytes + data.size(), s101MessageDispatch, this);
}

void ClientConnection::onDisconnected()
{
    emit disconnected();
}

void ClientConnection::handleS101Message(
    libs101::StreamDecoder<unsigned char>::const_iterator first,
    libs101::StreamDecoder<unsigned char>::const_iterator last)
{
    // Parse S101 frame
    first++;  // Skip slot
    auto message = *first++;  // Message type
    
    if (message == libs101::MessageType::EmBER) {
        auto command = *first++;  // Command
        first++;  // Version
        auto flags = libs101::PackageFlag(*first++);  // Flags
        first++;  // DTD
        
        if (command == libs101::CommandType::EmBER) {
            // Skip app bytes
            auto appbytes = *first++;
            while (appbytes-- > 0) {
                ++first;
            }
            
            // Feed to DOM reader
            try {
                m_domReader->read(first, last);
                
                if (flags.value() & libs101::PackageFlag::LastPackage) {
                    m_domReader->reset();
                }
            } catch (const std::exception &e) {
                qWarning() << "EmberProvider: Error parsing DOM:" << e.what();
            }
        }
        else if (command == libs101::CommandType::KeepAliveRequest) {
            m_provider->sendKeepAliveResponse(this);
        }
    }
}

// ========== DomReader Implementation ==========

DomReader::DomReader(ClientConnection* client)
    : libember::dom::AsyncDomReader(libember::glow::GlowNodeFactory::getFactory())
    , m_client(client)
{
}

DomReader::~DomReader()
{
}

void DomReader::rootReady(libember::dom::Node* root)
{
    detachRoot();
    if (root && m_client && m_client->m_provider) {
        m_client->m_provider->processRoot(root, m_client);
    }
    delete root;
}

// ========== EmberProvider Implementation ==========

EmberProvider::EmberProvider(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
{
}

EmberProvider::~EmberProvider()
{
    stopListening();
}

bool EmberProvider::startListening(quint16 port)
{
    if (m_server && m_server->isListening()) {
        return true;  // Already listening
    }
    
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &EmberProvider::onNewConnection);
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(QString("Failed to listen on port %1: %2")
            .arg(port).arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    emit serverStateChanged(true);
    return true;
}

void EmberProvider::stopListening()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    
    // Disconnect all clients
    for (ClientConnection *client : m_clients) {
        delete client;
    }
    m_clients.clear();
    
    emit serverStateChanged(false);
}

void EmberProvider::loadDeviceTree(const DeviceSnapshot &snapshot)
{
    m_nodes = snapshot.nodes;
    m_parameters = snapshot.parameters;
    m_matrices = snapshot.matrices;
    m_functions = snapshot.functions;
    
    // Use root paths from snapshot to preserve order
    m_rootPaths = snapshot.rootPaths;
    
    // Fallback: if rootPaths is empty (old format), extract from nodes
    if (m_rootPaths.isEmpty()) {
        for (const QString &path : m_nodes.keys()) {
            if (!path.contains('.')) {
                m_rootPaths.append(path);
            }
        }
    }
}

void EmberProvider::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        ClientConnection *client = new ClientConnection(socket, this);
        
        connect(client, &ClientConnection::disconnected, this, &EmberProvider::onClientDisconnected);
        
        m_clients.append(client);
        emit clientConnected(client->address());
    }
}

void EmberProvider::onClientDisconnected()
{
    ClientConnection *client = qobject_cast<ClientConnection*>(sender());
    if (client) {
        emit clientDisconnected(client->address());
        m_clients.removeAll(client);
        client->deleteLater();
    }
}

void EmberProvider::processRoot(libember::dom::Node* root, ClientConnection *client)
{
    auto rootCollection = dynamic_cast<libember::glow::GlowContainer*>(root);
    if (!rootCollection) {
        return;
    }
    
    // Process all elements in the root collection
    for (auto it = rootCollection->begin(); it != rootCollection->end(); ++it) {
        if (auto command = dynamic_cast<libember::glow::GlowCommand*>(&(*it))) {
            processCommand(command, client);
        }
        else if (auto qualNode = dynamic_cast<libember::glow::GlowQualifiedNode*>(&(*it))) {
            // Process qualified node (usually contains commands)
            if (qualNode->children()) {
                for (auto childIt = qualNode->children()->begin(); childIt != qualNode->children()->end(); ++childIt) {
                    if (auto cmd = dynamic_cast<libember::glow::GlowCommand*>(&(*childIt))) {
                        // Extract path from qualified node
                        QString path;
                        for (int val : qualNode->path()) {
                            if (!path.isEmpty()) path += ".";
                            path += QString::number(val);
                        }
                        
                        if (cmd->number().value() == libember::glow::CommandType::GetDirectory) {
                            sendGetDirectoryResponse(path, client);
                            emit requestReceived(path, "GetDirectory");
                        }
                        else if (cmd->number().value() == libember::glow::CommandType::Subscribe) {
                            handleSubscribe(path, client);
                            emit requestReceived(path, "Subscribe");
                        }
                        else if (cmd->number().value() == libember::glow::CommandType::Unsubscribe) {
                            handleUnsubscribe(path, client);
                            emit requestReceived(path, "Unsubscribe");
                        }
                    }
                }
            }
        }
        else if (auto qualParam = dynamic_cast<libember::glow::GlowQualifiedParameter*>(&(*it))) {
            processQualifiedParameter(qualParam, client);
        }
        else if (auto qualMatrix = dynamic_cast<libember::glow::GlowQualifiedMatrix*>(&(*it))) {
            // Handle matrix connection commands
            if (qualMatrix->connections()) {
                QString path;
                for (int val : qualMatrix->path()) {
                    if (!path.isEmpty()) path += ".";
                    path += QString::number(val);
                }
                
                // Process connection changes
                for (auto connIt = qualMatrix->connections()->begin(); connIt != qualMatrix->connections()->end(); ++connIt) {
                    if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*connIt))) {
                        int targetNumber = connection->target();
                        libember::ber::ObjectIdentifier sources = connection->sources();
                        
                        // Get operation type (Absolute=0, Connect=1, Disconnect=2)
                        int operation = connection->operation().value();
                        
                        // Update matrix connections in our tree
                        if (m_matrices.contains(path)) {
                            auto& matrix = m_matrices[path];
                            
                            if (operation == libember::glow::ConnectionOperation::Absolute) {
                                // Absolute: Clear all connections for this target, then add new ones
                                for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                    if (it.key().first == targetNumber) {
                                        it = matrix.connections.erase(it);
                                    } else {
                                        ++it;
                                    }
                                }
                                
                                // Add new connections
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections[{targetNumber, sourceNumber}] = true;
                                }
                                
                                emit requestReceived(path, QString("Matrix Absolute: Target %1").arg(targetNumber));
                            }
                            else if (operation == libember::glow::ConnectionOperation::Connect) {
                                // Connect: Add specified connections
                                
                                // For OneToN (0): Each target can have only one source
                                // For OneToOne (1): Each target AND each source can have only one connection
                                // For NToN (2): Multiple sources per target allowed
                                
                                if (matrix.type == 0) {  // OneToN
                                    // Clear existing connections for this target
                                    for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                        if (it.key().first == targetNumber) {
                                            it = matrix.connections.erase(it);
                                        } else {
                                            ++it;
                                        }
                                    }
                                }
                                else if (matrix.type == 1) {  // OneToOne
                                    // Clear connections for this target AND for the new sources
                                    for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                        int sourceNumber = *srcIt;
                                        
                                        // Clear any target connected to this source
                                        for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                            if (it.key().second == sourceNumber || it.key().first == targetNumber) {
                                                it = matrix.connections.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                                // For NToN (type == 2), don't clear anything - just add
                                
                                // Add new connections
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections[{targetNumber, sourceNumber}] = true;
                                }
                                
                                emit requestReceived(path, QString("Matrix Connect: Target %1").arg(targetNumber));
                            }
                            else if (operation == libember::glow::ConnectionOperation::Disconnect) {
                                // Disconnect: Remove specified connections
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections.remove({targetNumber, sourceNumber});
                                }
                                
                                emit requestReceived(path, QString("Matrix Disconnect: Target %1").arg(targetNumber));
                            }
                            
                            // Send updated matrix state back
                            sendMatrixResponse(path, client);
                        }
                    }
                }
            }
        }
    }
}

void EmberProvider::processCommand(libember::glow::GlowCommand* command, ClientConnection *client)
{
    if (command->number().value() == libember::glow::CommandType::GetDirectory) {
        // Root GetDirectory - send all root nodes
        sendGetDirectoryResponse("", client);
        emit requestReceived("", "GetDirectory (root)");
    }
}

void EmberProvider::processQualifiedParameter(libember::glow::GlowQualifiedParameter* param, ClientConnection *client)
{
    // Extract path
    QString path;
    for (int val : param->path()) {
        if (!path.isEmpty()) path += ".";
        path += QString::number(val);
    }
    
    // Check if this is a value update
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        if (m_parameters.contains(path)) {
            // Update parameter value in our tree
            auto value = param->value();
            QString valueStr;
            
            switch (value.type().value()) {
                case libember::glow::ParameterType::Integer:
                    valueStr = QString::number(value.toInteger());
                    m_parameters[path].value = valueStr;
                    break;
                case libember::glow::ParameterType::Real:
                    valueStr = QString::number(value.toReal());
                    m_parameters[path].value = valueStr;
                    break;
                case libember::glow::ParameterType::String:
                    valueStr = QString::fromStdString(value.toString());
                    m_parameters[path].value = valueStr;
                    break;
                case libember::glow::ParameterType::Boolean:
                    valueStr = value.toBoolean() ? "true" : "false";
                    m_parameters[path].value = valueStr;
                    break;
                default:
                    break;
            }
            
            emit requestReceived(path, QString("SetValue: %1").arg(valueStr));
            
            // Send updated parameter to all subscribers
            sendParameterResponse(path, client);
        }
    }
    
    // Process commands in qualified parameter
    if (param->children()) {
        for (auto it = param->children()->begin(); it != param->children()->end(); ++it) {
            if (auto cmd = dynamic_cast<libember::glow::GlowCommand*>(&(*it))) {
                if (cmd->number().value() == libember::glow::CommandType::Subscribe) {
                    handleSubscribe(path, client);
                    emit requestReceived(path, "Subscribe");
                }
                else if (cmd->number().value() == libember::glow::CommandType::Unsubscribe) {
                    handleUnsubscribe(path, client);
                    emit requestReceived(path, "Unsubscribe");
                }
            }
        }
    }
}

void EmberProvider::sendGetDirectoryResponse(const QString &path, ClientConnection *client)
{
    if (path.isEmpty()) {
        // Send all root elements in order
        for (const QString &rootPath : m_rootPaths) {
            if (m_nodes.contains(rootPath)) {
                sendNodeResponse(rootPath, client);
            }
        }
    } else {
        // Send children using the pre-ordered childPaths list from NodeData
        // This preserves the original device's tree order
        if (m_nodes.contains(path)) {
            const NodeData &node = m_nodes[path];
            
            for (const QString &childPath : node.childPaths) {
                // Determine child type and send appropriate response
                if (m_nodes.contains(childPath)) {
                    sendNodeResponse(childPath, client);
                } else if (m_parameters.contains(childPath)) {
                    sendParameterResponse(childPath, client);
                } else if (m_matrices.contains(childPath)) {
                    sendMatrixResponse(childPath, client);
                    // Also send label container node if matrix has labels
                    sendMatrixLabelNode(childPath, client);
                } else if (m_functions.contains(childPath)) {
                    sendFunctionResponse(childPath, client);
                }
            }
        }
        // Check if this is a matrix label container (matrixPath.666999666)
        else if (path.endsWith(".666999666")) {
            QString matrixPath = path.left(path.length() - 10); // Remove ".666999666"
            if (m_matrices.contains(matrixPath)) {
                // Send target and source type nodes
                sendMatrixLabelTypeNode(path, "1", client); // targets
                sendMatrixLabelTypeNode(path, "2", client); // sources
            }
        }
        // Check if this is a target/source label type node (matrixPath.666999666.1 or .2)
        else if (path.contains(".666999666.")) {
            QStringList parts = path.split('.');
            if (parts.size() >= 2) {
                QString labelType = parts.last();
                QString matrixPath = parts.mid(0, parts.size() - 2).join('.');
                
                if (m_matrices.contains(matrixPath)) {
                    sendMatrixLabelParameters(matrixPath, labelType, client);
                }
            }
        }
    }
}

// Helper: Parse path string to OID
static libember::ber::ObjectIdentifier pathToOid(const QString &path)
{
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    return oid;
}

void EmberProvider::sendNodeResponse(const QString &path, ClientConnection *client)
{
    if (!m_nodes.contains(path)) {
        return;
    }
    
    const NodeData &node = m_nodes[path];
    
    // Create and populate qualified node
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(path));
    qualNode->setIdentifier(node.identifier.toStdString());
    if (!node.description.isEmpty()) {
        qualNode->setDescription(node.description.toStdString());
    }
    qualNode->setIsOnline(node.isOnline);
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualNode);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendParameterResponse(const QString &path, ClientConnection *client)
{
    if (!m_parameters.contains(path)) {
        return;
    }
    
    const ParameterData &param = m_parameters[path];
    
    // Create and populate qualified parameter
    auto qualParam = new libember::glow::GlowQualifiedParameter(pathToOid(path));
    qualParam->setIdentifier(param.identifier.toStdString());
    
    // Set value based on type
    switch (param.type) {
        case 1:  // Integer
            qualParam->setValue(static_cast<long>(param.value.toLongLong()));
            break;
        case 2:  // Real
            qualParam->setValue(param.value.toDouble());
            break;
        case 3:  // String
            qualParam->setValue(param.value.toStdString());
            break;
        case 4:  // Boolean
            qualParam->setValue(param.value == "true" || param.value == "1");
            break;
        default:
            qualParam->setValue(param.value.toStdString());
            break;
    }
    
    qualParam->setAccess(static_cast<libember::glow::Access::_Domain>(param.access));
    
    // Set min/max if available
    if (!param.minimum.isNull()) {
        qualParam->setMinimum(static_cast<long>(param.minimum.toLongLong()));
    }
    if (!param.maximum.isNull()) {
        qualParam->setMaximum(static_cast<long>(param.maximum.toLongLong()));
    }
    
    // Set enum options if available
    if (!param.enumOptions.isEmpty()) {
        std::vector<std::pair<std::string, int>> enumMap;
        for (int i = 0; i < param.enumOptions.size(); ++i) {
            int enumValue = i < param.enumValues.size() ? param.enumValues[i] : i;
            enumMap.push_back(std::make_pair(param.enumOptions[i].toStdString(), enumValue));
        }
        qualParam->setEnumerationMap(enumMap.begin(), enumMap.end());
    }
    
    qualParam->setIsOnline(param.isOnline);
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualParam);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendMatrixResponse(const QString &path, ClientConnection *client)
{
    if (!m_matrices.contains(path)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[path];
    
    // Create and populate qualified matrix
    auto qualMatrix = new libember::glow::GlowQualifiedMatrix(pathToOid(path));
    qualMatrix->setIdentifier(matrix.identifier.toStdString());
    
    if (!matrix.description.isEmpty()) {
        qualMatrix->setDescription(matrix.description.toStdString());
    }
    
    // Set matrix type and dimensions
    qualMatrix->setType(static_cast<libember::glow::MatrixType::_Domain>(matrix.type));
    qualMatrix->setTargetCount(matrix.targetCount);
    qualMatrix->setSourceCount(matrix.sourceCount);
    
    // Add targets - send only the targets that actually exist
    if (!matrix.targetNumbers.isEmpty()) {
        auto targetsSeq = qualMatrix->targets();
        for (int targetNumber : matrix.targetNumbers) {
            auto target = new libember::glow::GlowTarget(targetNumber);
            targetsSeq->insert(targetsSeq->end(), target);
        }
    }
    
    // Add sources - send only the sources that actually exist
    if (!matrix.sourceNumbers.isEmpty()) {
        auto sourcesSeq = qualMatrix->sources();
        for (int sourceNumber : matrix.sourceNumbers) {
            auto source = new libember::glow::GlowSource(sourceNumber);
            sourcesSeq->insert(sourcesSeq->end(), source);
        }
    }
    
    // Add connections - group by target
    QMap<int, QList<int>> targetToSources;
    for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ++it) {
        if (it.value()) {  // Only connected crosspoints
            int target = it.key().first;
            int source = it.key().second;
            targetToSources[target].append(source);
        }
    }
    
    if (!targetToSources.isEmpty()) {
        auto connectionsSeq = qualMatrix->connections();
        for (auto it = targetToSources.begin(); it != targetToSources.end(); ++it) {
            int targetNumber = it.key();
            auto connection = new libember::glow::GlowConnection(targetNumber);
            
            // Set sources for this target
            libember::ber::ObjectIdentifier sources;
            for (int sourceNumber : it.value()) {
                sources.push_back(sourceNumber);
            }
            connection->setSources(sources);
            
            // Set disposition (0 = Tally, actual connection state)
            connection->setDisposition(libember::glow::ConnectionDisposition::Tally);
            
            connectionsSeq->insert(connectionsSeq->end(), connection);
        }
    }
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualMatrix);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendFunctionResponse(const QString &path, ClientConnection *client)
{
    if (!m_functions.contains(path)) {
        return;
    }
    
    const FunctionData &func = m_functions[path];
    
    // Create and populate qualified function
    auto qualFunc = new libember::glow::GlowQualifiedFunction(pathToOid(path));
    qualFunc->setIdentifier(func.identifier.toStdString());
    
    if (!func.description.isEmpty()) {
        qualFunc->setDescription(func.description.toStdString());
    }
    
    // Add arguments
    if (!func.argNames.isEmpty()) {
        auto argsSeq = qualFunc->arguments();
        for (int i = 0; i < func.argNames.size(); ++i) {
            libember::glow::ParameterType paramType(static_cast<libember::glow::ParameterType::_Domain>(func.argTypes[i]));
            auto argDesc = new libember::glow::GlowTupleItemDescription(paramType, func.argNames[i].toStdString());
            argsSeq->insert(argsSeq->end(), argDesc);
        }
    }
    
    // Add results
    if (!func.resultNames.isEmpty()) {
        auto resultsSeq = qualFunc->result();
        for (int i = 0; i < func.resultNames.size(); ++i) {
            libember::glow::ParameterType paramType(static_cast<libember::glow::ParameterType::_Domain>(func.resultTypes[i]));
            auto resultDesc = new libember::glow::GlowTupleItemDescription(paramType, func.resultNames[i].toStdString());
            resultsSeq->insert(resultsSeq->end(), resultDesc);
        }
    }
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualFunc);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendKeepAliveResponse(ClientConnection *client)
{
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);  // Slot
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::KeepAliveResponse);
    encoder.encode(0x01);  // Version
    encoder.finish();
    
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    client->sendData(qdata);
}

void EmberProvider::handleSubscribe(const QString &path, ClientConnection *client)
{
    client->subscriptions.insert(path);
}

void EmberProvider::handleUnsubscribe(const QString &path, ClientConnection *client)
{
    client->subscriptions.remove(path);
}

void EmberProvider::sendEncodedMessage(const libember::glow::GlowContainer *container, ClientConnection *client)
{
    // Encode to BER
    libember::util::OctetStream stream;
    container->encode(stream);
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);  // Slot
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);  // Version
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);  // DTD
    encoder.encode(0x00);  // AppBytes
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    client->sendData(qdata);
}

void EmberProvider::sendMatrixLabelNode(const QString &matrixPath, ClientConnection *client)
{
    // Check if matrix has any labels
    if (!m_matrices.contains(matrixPath)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[matrixPath];
    if (matrix.targetLabels.isEmpty() && matrix.sourceLabels.isEmpty()) {
        return; // No labels to send
    }
    
    // Create virtual label container node at matrixPath.666999666
    QString labelContainerPath = matrixPath + ".666999666";
    
    // Create qualified node for label container
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(labelContainerPath));
    qualNode->setIdentifier("labels");
    qualNode->setDescription("Matrix Labels");
    qualNode->setIsOnline(true);
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualNode);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendMatrixLabelTypeNode(const QString &containerPath, const QString &labelType, ClientConnection *client)
{
    // Extract matrix path from container path (remove .666999666)
    QString matrixPath = containerPath.left(containerPath.length() - 10);
    
    if (!m_matrices.contains(matrixPath)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[matrixPath];
    
    // Check if this type has labels
    if (labelType == "1" && matrix.targetLabels.isEmpty()) {
        return;
    }
    if (labelType == "2" && matrix.sourceLabels.isEmpty()) {
        return;
    }
    
    // Create type node path: containerPath.1 or containerPath.2
    QString typePath = containerPath + "." + labelType;
    
    // Create qualified node
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(typePath));
    qualNode->setIdentifier(labelType == "1" ? "targets" : "sources");
    qualNode->setDescription(labelType == "1" ? "Target Labels" : "Source Labels");
    qualNode->setIsOnline(true);
    
    // Send
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualNode);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendMatrixLabelParameters(const QString &matrixPath, const QString &labelType, ClientConnection *client)
{
    if (!m_matrices.contains(matrixPath)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[matrixPath];
    const QMap<int, QString> &labels = (labelType == "1") ? matrix.targetLabels : matrix.sourceLabels;
    
    if (labels.isEmpty()) {
        return;
    }
    
    // Send a parameter for each label
    for (auto it = labels.begin(); it != labels.end(); ++it) {
        int number = it.key();
        QString label = it.value();
        
        // Create parameter path: matrixPath.666999666.labelType.number
        QString paramPath = QString("%1.666999666.%2.%3").arg(matrixPath).arg(labelType).arg(number);
        
        // Create qualified parameter
        auto qualParam = new libember::glow::GlowQualifiedParameter(pathToOid(paramPath));
        qualParam->setIdentifier(QString::number(number).toStdString());
        qualParam->setValue(label.toStdString());
        qualParam->setType(libember::glow::ParameterType::String);
        qualParam->setAccess(libember::glow::Access::ReadOnly);
        qualParam->setIsOnline(true);
        
        // Send
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), qualParam);
        sendEncodedMessage(root, client);
        delete root;
    }
}

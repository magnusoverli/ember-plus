







#include "EmberProvider.h"
#include "DeviceSnapshot.h"
#include "S101Protocol.h"
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
#include <s101/MessageType.hpp>
#include <s101/CommandType.hpp>
#include <s101/PackageFlag.hpp>
#include <QDebug>


static void s101MessageDispatch(libs101::StreamDecoder<unsigned char>::const_iterator first,
                                libs101::StreamDecoder<unsigned char>::const_iterator last,
                                ClientConnection* state)
{
    state->handleS101Message(first, last);
}



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
    
    first++;  
    auto message = *first++;  
    
    if (message == libs101::MessageType::EmBER) {
        auto command = *first++;  
        first++;  
        auto flags = libs101::PackageFlag(*first++);  
        first++;  
        
        if (command == libs101::CommandType::EmBER) {
            
            auto appbytes = *first++;
            while (appbytes-- > 0) {
                ++first;
            }
            
            
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



EmberProvider::EmberProvider(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_s101Protocol(new S101Protocol(this))
{
}

EmberProvider::~EmberProvider()
{
    stopListening();
    delete m_s101Protocol;
}

bool EmberProvider::startListening(quint16 port)
{
    if (m_server && m_server->isListening()) {
        return true;  
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
    
    
    m_rootPaths = snapshot.rootPaths;
    
    
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
    
    
    for (auto it = rootCollection->begin(); it != rootCollection->end(); ++it) {
        if (auto command = dynamic_cast<libember::glow::GlowCommand*>(&(*it))) {
            processCommand(command, client);
        }
        else if (auto qualNode = dynamic_cast<libember::glow::GlowQualifiedNode*>(&(*it))) {
            
            if (qualNode->children()) {
                for (auto childIt = qualNode->children()->begin(); childIt != qualNode->children()->end(); ++childIt) {
                    if (auto cmd = dynamic_cast<libember::glow::GlowCommand*>(&(*childIt))) {
                        
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
            
            if (qualMatrix->connections()) {
                QString path;
                for (int val : qualMatrix->path()) {
                    if (!path.isEmpty()) path += ".";
                    path += QString::number(val);
                }
                
                
                for (auto connIt = qualMatrix->connections()->begin(); connIt != qualMatrix->connections()->end(); ++connIt) {
                    if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*connIt))) {
                        int targetNumber = connection->target();
                        libember::ber::ObjectIdentifier sources = connection->sources();
                        
                        
                        int operation = connection->operation().value();
                        
                        
                        if (m_matrices.contains(path)) {
                            auto& matrix = m_matrices[path];
                            
                            if (operation == libember::glow::ConnectionOperation::Absolute) {
                                
                                for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                    if (it.key().first == targetNumber) {
                                        it = matrix.connections.erase(it);
                                    } else {
                                        ++it;
                                    }
                                }
                                
                                
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections[{targetNumber, sourceNumber}] = true;
                                }
                                
                                emit requestReceived(path, QString("Matrix Absolute: Target %1").arg(targetNumber));
                            }
                            else if (operation == libember::glow::ConnectionOperation::Connect) {
                                
                                
                                
                                
                                
                                
                                if (matrix.type == 0) {  
                                    
                                    for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                        if (it.key().first == targetNumber) {
                                            it = matrix.connections.erase(it);
                                        } else {
                                            ++it;
                                        }
                                    }
                                }
                                else if (matrix.type == 1) {  
                                    
                                    for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                        int sourceNumber = *srcIt;
                                        
                                        
                                        for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ) {
                                            if (it.key().second == sourceNumber || it.key().first == targetNumber) {
                                                it = matrix.connections.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                                
                                
                                
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections[{targetNumber, sourceNumber}] = true;
                                }
                                
                                emit requestReceived(path, QString("Matrix Connect: Target %1").arg(targetNumber));
                            }
                            else if (operation == libember::glow::ConnectionOperation::Disconnect) {
                                
                                for (auto srcIt = sources.begin(); srcIt != sources.end(); ++srcIt) {
                                    int sourceNumber = *srcIt;
                                    matrix.connections.remove({targetNumber, sourceNumber});
                                }
                                
                                emit requestReceived(path, QString("Matrix Disconnect: Target %1").arg(targetNumber));
                            }
                            
                            
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
        
        sendGetDirectoryResponse("", client);
        emit requestReceived("", "GetDirectory (root)");
    }
}

void EmberProvider::processQualifiedParameter(libember::glow::GlowQualifiedParameter* param, ClientConnection *client)
{
    
    QString path;
    for (int val : param->path()) {
        if (!path.isEmpty()) path += ".";
        path += QString::number(val);
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        if (m_parameters.contains(path)) {
            
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
            
            
            sendParameterResponse(path, client);
        }
    }
    
    
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
        
        for (const QString &rootPath : m_rootPaths) {
            if (m_nodes.contains(rootPath)) {
                sendNodeResponse(rootPath, client);
            }
        }
    } else {
        
        
        if (m_nodes.contains(path)) {
            const NodeData &node = m_nodes[path];
            
            for (const QString &childPath : node.childPaths) {
                
                if (m_nodes.contains(childPath)) {
                    sendNodeResponse(childPath, client);
                } else if (m_parameters.contains(childPath)) {
                    sendParameterResponse(childPath, client);
                } else if (m_matrices.contains(childPath)) {
                    sendMatrixResponse(childPath, client);
                    
                    sendMatrixLabelNode(childPath, client);
                } else if (m_functions.contains(childPath)) {
                    sendFunctionResponse(childPath, client);
                }
            }
        }
        
        else if (path.endsWith(".666999666")) {
            QString matrixPath = path.left(path.length() - 10); 
            if (m_matrices.contains(matrixPath)) {
                
                sendMatrixLabelTypeNode(path, "1", client); 
                sendMatrixLabelTypeNode(path, "2", client); 
            }
        }
        
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
    
    
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(path));
    qualNode->setIdentifier(node.identifier.toStdString());
    if (!node.description.isEmpty()) {
        qualNode->setDescription(node.description.toStdString());
    }
    qualNode->setIsOnline(node.isOnline);
    
    
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
    
    
    auto qualParam = new libember::glow::GlowQualifiedParameter(pathToOid(path));
    qualParam->setIdentifier(param.identifier.toStdString());
    
    
    switch (param.type) {
        case 1:  
            qualParam->setValue(static_cast<long>(param.value.toLongLong()));
            break;
        case 2:  
            qualParam->setValue(param.value.toDouble());
            break;
        case 3:  
            qualParam->setValue(param.value.toStdString());
            break;
        case 4:  
            qualParam->setValue(param.value == "true" || param.value == "1");
            break;
        default:
            qualParam->setValue(param.value.toStdString());
            break;
    }
    
    qualParam->setAccess(static_cast<libember::glow::Access::_Domain>(param.access));
    
    
    if (!param.minimum.isNull()) {
        qualParam->setMinimum(static_cast<long>(param.minimum.toLongLong()));
    }
    if (!param.maximum.isNull()) {
        qualParam->setMaximum(static_cast<long>(param.maximum.toLongLong()));
    }
    
    
    if (!param.enumOptions.isEmpty()) {
        std::vector<std::pair<std::string, int>> enumMap;
        for (int i = 0; i < param.enumOptions.size(); ++i) {
            int enumValue = i < param.enumValues.size() ? param.enumValues[i] : i;
            enumMap.push_back(std::make_pair(param.enumOptions[i].toStdString(), enumValue));
        }
        qualParam->setEnumerationMap(enumMap.begin(), enumMap.end());
    }
    
    qualParam->setIsOnline(param.isOnline);
    
    
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
    
    
    auto qualMatrix = new libember::glow::GlowQualifiedMatrix(pathToOid(path));
    qualMatrix->setIdentifier(matrix.identifier.toStdString());
    
    if (!matrix.description.isEmpty()) {
        qualMatrix->setDescription(matrix.description.toStdString());
    }
    
    
    qualMatrix->setType(static_cast<libember::glow::MatrixType::_Domain>(matrix.type));
    qualMatrix->setTargetCount(matrix.targetCount);
    qualMatrix->setSourceCount(matrix.sourceCount);
    
    
    if (!matrix.targetNumbers.isEmpty()) {
        auto targetsSeq = qualMatrix->targets();
        for (int targetNumber : matrix.targetNumbers) {
            auto target = new libember::glow::GlowTarget(targetNumber);
            targetsSeq->insert(targetsSeq->end(), target);
        }
    }
    
    
    if (!matrix.sourceNumbers.isEmpty()) {
        auto sourcesSeq = qualMatrix->sources();
        for (int sourceNumber : matrix.sourceNumbers) {
            auto source = new libember::glow::GlowSource(sourceNumber);
            sourcesSeq->insert(sourcesSeq->end(), source);
        }
    }
    
    
    QMap<int, QList<int>> targetToSources;
    for (auto it = matrix.connections.begin(); it != matrix.connections.end(); ++it) {
        if (it.value()) {  
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
            
            
            libember::ber::ObjectIdentifier sources;
            for (int sourceNumber : it.value()) {
                sources.push_back(sourceNumber);
            }
            connection->setSources(sources);
            
            
            connection->setDisposition(libember::glow::ConnectionDisposition::Tally);
            
            connectionsSeq->insert(connectionsSeq->end(), connection);
        }
    }
    
    
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
    
    
    auto qualFunc = new libember::glow::GlowQualifiedFunction(pathToOid(path));
    qualFunc->setIdentifier(func.identifier.toStdString());
    
    if (!func.description.isEmpty()) {
        qualFunc->setDescription(func.description.toStdString());
    }
    
    
    if (!func.argNames.isEmpty()) {
        auto argsSeq = qualFunc->arguments();
        for (int i = 0; i < func.argNames.size(); ++i) {
            libember::glow::ParameterType paramType(static_cast<libember::glow::ParameterType::_Domain>(func.argTypes[i]));
            auto argDesc = new libember::glow::GlowTupleItemDescription(paramType, func.argNames[i].toStdString());
            argsSeq->insert(argsSeq->end(), argDesc);
        }
    }
    
    
    if (!func.resultNames.isEmpty()) {
        auto resultsSeq = qualFunc->result();
        for (int i = 0; i < func.resultNames.size(); ++i) {
            libember::glow::ParameterType paramType(static_cast<libember::glow::ParameterType::_Domain>(func.resultTypes[i]));
            auto resultDesc = new libember::glow::GlowTupleItemDescription(paramType, func.resultNames[i].toStdString());
            resultsSeq->insert(resultsSeq->end(), resultDesc);
        }
    }
    
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualFunc);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendKeepAliveResponse(ClientConnection *client)
{
    QByteArray frame = m_s101Protocol->encodeKeepAliveResponse();
    client->sendData(frame);
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
    
    libember::util::OctetStream stream;
    container->encode(stream);
    
    
    QByteArray frame = m_s101Protocol->encodeEmberData(stream);
    client->sendData(frame);
}

void EmberProvider::sendMatrixLabelNode(const QString &matrixPath, ClientConnection *client)
{
    
    if (!m_matrices.contains(matrixPath)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[matrixPath];
    if (matrix.targetLabels.isEmpty() && matrix.sourceLabels.isEmpty()) {
        return; 
    }
    
    
    QString labelContainerPath = matrixPath + ".666999666";
    
    
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(labelContainerPath));
    qualNode->setIdentifier("labels");
    qualNode->setDescription("Matrix Labels");
    qualNode->setIsOnline(true);
    
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), qualNode);
    sendEncodedMessage(root, client);
    delete root;
}

void EmberProvider::sendMatrixLabelTypeNode(const QString &containerPath, const QString &labelType, ClientConnection *client)
{
    
    QString matrixPath = containerPath.left(containerPath.length() - 10);
    
    if (!m_matrices.contains(matrixPath)) {
        return;
    }
    
    const MatrixData &matrix = m_matrices[matrixPath];
    
    
    if (labelType == "1" && matrix.targetLabels.isEmpty()) {
        return;
    }
    if (labelType == "2" && matrix.sourceLabels.isEmpty()) {
        return;
    }
    
    
    QString typePath = containerPath + "." + labelType;
    
    
    auto qualNode = new libember::glow::GlowQualifiedNode(pathToOid(typePath));
    qualNode->setIdentifier(labelType == "1" ? "targets" : "sources");
    qualNode->setDescription(labelType == "1" ? "Target Labels" : "Source Labels");
    qualNode->setIsOnline(true);
    
    
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
    
    
    for (auto it = labels.begin(); it != labels.end(); ++it) {
        int number = it.key();
        QString label = it.value();
        
        
        QString paramPath = QString("%1.666999666.%2.%3").arg(matrixPath).arg(labelType).arg(number);
        
        
        auto qualParam = new libember::glow::GlowQualifiedParameter(pathToOid(paramPath));
        qualParam->setIdentifier(QString::number(number).toStdString());
        qualParam->setValue(label.toStdString());
        qualParam->setType(libember::glow::ParameterType::String);
        qualParam->setAccess(libember::glow::Access::ReadOnly);
        qualParam->setIsOnline(true);
        
        
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), qualParam);
        sendEncodedMessage(root, client);
        delete root;
    }
}

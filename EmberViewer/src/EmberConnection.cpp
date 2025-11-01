







#include "EmberConnection.h"
#include "TreeFetchService.h"
#include "CacheManager.h"
#include <QDebug>
#include <ember/Ember.hpp>
#include <ember/ber/ObjectIdentifier.hpp>
#include <ember/ber/Null.hpp>
#include <ember/glow/GlowNodeFactory.hpp>
#include <ember/glow/GlowRootElementCollection.hpp>
#include <ember/glow/GlowCommand.hpp>
#include <ember/glow/GlowNode.hpp>
#include <ember/glow/GlowQualifiedNode.hpp>
#include <ember/glow/GlowParameter.hpp>
#include <ember/glow/GlowQualifiedParameter.hpp>
#include <ember/glow/GlowMatrix.hpp>
#include <ember/glow/GlowQualifiedMatrix.hpp>
#include <ember/glow/GlowTarget.hpp>
#include <ember/glow/GlowSource.hpp>
#include <ember/glow/GlowConnection.hpp>
#include <ember/glow/GlowFunction.hpp>
#include <ember/glow/GlowQualifiedFunction.hpp>
#include <ember/glow/GlowInvocation.hpp>
#include <ember/glow/GlowInvocationResult.hpp>
#include <ember/glow/GlowTupleItemDescription.hpp>
#include <ember/glow/GlowStreamCollection.hpp>
#include <ember/glow/GlowStreamEntry.hpp>
#include <ember/glow/MatrixProperty.hpp>
#include <ember/glow/CommandType.hpp>
#include <ember/glow/DirFieldMask.hpp>
#include <ember/glow/NodeProperty.hpp>
#include <ember/glow/ParameterProperty.hpp>
#include <ember/glow/ParameterType.hpp>
#include <ember/glow/Access.hpp>
#include <ember/glow/ConnectionOperation.hpp>
#include <ember/glow/ConnectionDisposition.hpp>
#include <ember/util/OctetStream.hpp>




EmberConnection::EmberConnection(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_s101Protocol(new S101Protocol(this))
    , m_glowParser(new GlowParser(this))
    , m_treeFetchService(new TreeFetchService(this))
    , m_cacheManager(new CacheManager(this))
    , m_connected(false)
    , m_emberDataReceived(false)
    , m_connectionTimer(nullptr)
    , m_protocolTimer(nullptr)
    , m_nextInvocationId(1)
{
    
    connect(m_socket, &QTcpSocket::connected, this, &EmberConnection::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &EmberConnection::onSocketDisconnected);
    
    
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &EmberConnection::onSocketError);
    #else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &EmberConnection::onSocketError);
    #endif
    
    connect(m_socket, &QTcpSocket::readyRead, this, &EmberConnection::onDataReceived);
    
    
    connect(m_s101Protocol, &S101Protocol::messageReceived, this, &EmberConnection::onS101MessageReceived);
    connect(m_s101Protocol, &S101Protocol::keepAliveReceived, this, &EmberConnection::onKeepAliveReceived);
    connect(m_s101Protocol, &S101Protocol::protocolError, this, [this](const QString& error) {
        qCritical().noquote() << "S101 protocol error:" << error;
        disconnect();
    });
    
    
    connect(m_glowParser, &GlowParser::nodeReceived, this, &EmberConnection::onParserNodeReceived);
    connect(m_glowParser, &GlowParser::parameterReceived, this, &EmberConnection::onParserParameterReceived);
    connect(m_glowParser, &GlowParser::matrixReceived, this, &EmberConnection::onParserMatrixReceived);
    
    
    connect(m_glowParser, &GlowParser::matrixTargetReceived, this, 
            [this](const EmberData::MatrixTargetInfo& target) {
                emit matrixTargetReceived(target.matrixPath, target.targetNumber, target.label);
            });
    connect(m_glowParser, &GlowParser::matrixSourceReceived, this,
            [this](const EmberData::MatrixSourceInfo& source) {
                emit matrixSourceReceived(source.matrixPath, source.sourceNumber, source.label);
            });
    connect(m_glowParser, &GlowParser::matrixConnectionReceived, this,
            [this](const EmberData::MatrixConnectionInfo& conn) {
                emit matrixConnectionReceived(conn.matrixPath, conn.targetNumber, 
                                            conn.sourceNumber, conn.connected, conn.disposition);
            });
    connect(m_glowParser, &GlowParser::matrixTargetConnectionsCleared, this, 
            &EmberConnection::matrixTargetConnectionsCleared);
    connect(m_glowParser, &GlowParser::functionReceived, this,
            [this](const EmberData::FunctionInfo& func) {
                emit functionReceived(func.path, func.identifier, func.description,
                                    func.argNames, func.argTypes, func.resultNames, func.resultTypes);
            });
    connect(m_glowParser, &GlowParser::invocationResultReceived, this,
            [this](const EmberData::InvocationResult& result) {
                if (m_pendingInvocations.contains(result.invocationId)) {
                    m_pendingInvocations.remove(result.invocationId);
                }
                emit invocationResultReceived(result.invocationId, result.success, result.results);
            });
    connect(m_glowParser, &GlowParser::streamValueReceived, this,
            [this](const EmberData::StreamValue& stream) {
                emit streamValueReceived(stream.streamIdentifier, stream.value);
            });
    connect(m_glowParser, &GlowParser::parsingError, this, [this](const QString& error) {
        qCritical().noquote() << "Parsing error:" << error;
        disconnect();
    });
    
    
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setSingleShot(true);
    m_connectionTimer->setInterval(CONNECTION_TIMEOUT_MS);
    connect(m_connectionTimer, &QTimer::timeout, this, &EmberConnection::onConnectionTimeout);
    
    
    m_protocolTimer = new QTimer(this);
    m_protocolTimer->setSingleShot(true);
    m_protocolTimer->setInterval(PROTOCOL_TIMEOUT_MS);
    connect(m_protocolTimer, &QTimer::timeout, this, &EmberConnection::onProtocolTimeout);
}

EmberConnection::~EmberConnection()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    
    delete m_s101Protocol;
    delete m_glowParser;
}

void EmberConnection::connectToHost(const QString &host, int port)
{
    
    QAbstractSocket::SocketState state = m_socket->state();
    if (state != QAbstractSocket::UnconnectedState) {
        qWarning().noquote() << QString("Cannot connect: socket is already in state %1").arg(static_cast<int>(state));
        
        
        if (state == QAbstractSocket::ConnectingState || 
            state == QAbstractSocket::HostLookupState) {
            qInfo().noquote() << "Aborting previous connection attempt...";
            m_socket->abort();
            m_connectionTimer->stop();
        } else if (state == QAbstractSocket::ConnectedState) {
            qInfo().noquote() << "Already connected";
            return;
        } else {
            
            return;
        }
    }
    
    m_host = host;
    m_port = port;
    
    
    m_requestedPaths.clear();
    
    
    
    
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    
    
    
    
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 65536);
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 65536);
    
    qInfo().noquote() << QString("Connecting to %1:%2...").arg(host).arg(port);
    
    
    m_connectionTimer->start();
    
    m_socket->connectToHost(host, port);
}

void EmberConnection::disconnect()
{
    
    m_connectionTimer->stop();
    m_protocolTimer->stop();
    
    QAbstractSocket::SocketState state = m_socket->state();
    
    if (state == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    } else if (state == QAbstractSocket::ConnectingState || 
               state == QAbstractSocket::HostLookupState) {
        
        qInfo().noquote() << "Aborting pending connection...";
        m_socket->abort();
    }
    
    
    m_requestedPaths.clear();
    m_subscriptions.clear();  
}

bool EmberConnection::isConnected() const
{
    return m_connected;
}

void EmberConnection::onSocketConnected()
{
    
    m_connectionTimer->stop();
    
    m_connected = true;
    m_emberDataReceived = false;
    emit connected();
    qInfo().noquote() << "Connected to provider";
    
    
    m_protocolTimer->start();
    qInfo().noquote() << "Waiting for Ember+ response...";
    
    
    
    QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
    if (CacheManager::hasDeviceCache(cacheKey)) {
        CacheManager::DeviceCache cache = CacheManager::getDeviceCache(cacheKey);
        
        
        QDateTime now = QDateTime::currentDateTime();
        qint64 hoursSinceLastSeen = cache.lastSeen.secsTo(now) / 3600;
        
        if (cache.isValid) {
            qInfo().noquote() << QString("Using cached device name: '%1' (last seen %2 hours ago)")
                .arg(cache.deviceName)
                .arg(hoursSinceLastSeen);
            
            
            m_cacheManager->setRootNode(cache.rootPath, cache.deviceName, false, cache.identityPath);
            
            
            emit nodeReceived(cache.rootPath, cache.deviceName, cache.deviceName, true);
            
            qInfo().noquote() << "Cached device name displayed instantly, will verify with device...";
        } else {
            qInfo().noquote() << QString("Cache expired (last seen %1 hours ago), will rediscover device name")
                .arg(hoursSinceLastSeen);
        }
    }
    
    
    qInfo().noquote() << "About to send GetDirectory request...";
    sendGetDirectory();
    qInfo().noquote() << "GetDirectory call completed";
}

void EmberConnection::onSocketDisconnected()
{
    
    m_connectionTimer->stop();
    m_protocolTimer->stop();
    
    m_connected = false;
    m_emberDataReceived = false;
    m_cacheManager->clear();  
    emit disconnected();
    qInfo().noquote() << "Disconnected from provider";
}

void EmberConnection::onSocketError(QAbstractSocket::SocketError error)
{
    
    m_connectionTimer->stop();
    
    QString errorString = m_socket->errorString();
    QAbstractSocket::SocketState state = m_socket->state();
    
    qCritical().noquote() << QString("Connection error: %1 (error code: %2, state: %3)")
        .arg(errorString).arg(error).arg(state);
    
    
    
    
    if (error == QAbstractSocket::RemoteHostClosedError) {
        qInfo().noquote() << "Remote host closed connection gracefully";
        
        return;
    }
    
    
    if (error == QAbstractSocket::ConnectionRefusedError ||
        error == QAbstractSocket::NetworkError ||
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::SocketTimeoutError ||
        error == QAbstractSocket::SocketAccessError ||
        error == QAbstractSocket::SocketResourceError) {
        
        qInfo().noquote() << "Aborting connection due to error...";
        
        
        if (state != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        
        
    }
    else {
        
        qWarning() << "[EmberConnection] Unhandled socket error type:" << error 
                   << "- relying on Qt's default handling";
    }
}

void EmberConnection::onConnectionTimeout()
{
    qCritical().noquote() << QString("Connection timeout after 5 seconds");
    
    
    QAbstractSocket::SocketState state = m_socket->state();
    if (state == QAbstractSocket::ConnectingState || 
        state == QAbstractSocket::HostLookupState) {
        qInfo().noquote() << "Aborting connection attempt...";
        m_socket->abort();
        
        
        
        if (!m_connected) {
            emit disconnected();
        }
    }
}

void EmberConnection::onProtocolTimeout()
{
    qCritical().noquote() << QString("No Ember+ response received after 10 seconds. This port does not appear to be serving Ember+ protocol.");
    
    
    if (m_connected) {
        qInfo().noquote() << "Disconnecting due to protocol timeout...";
        disconnect();
    }
}

void EmberConnection::onDataReceived()
{
    QByteArray data = m_socket->readAll();
    
    if (data.isEmpty()) {
        return;
    }
    
    
    qDebug().noquote() << QString("Received %1 bytes from socket").arg(data.size());
    
    
    m_s101Protocol->feedData(data);
}

void EmberConnection::onS101MessageReceived(const QByteArray& emberData)
{
    
    bool isFirstData = false;
    if (!m_emberDataReceived) {
        m_emberDataReceived = true;
        isFirstData = true;
        m_protocolTimer->stop();
        qInfo().noquote() << "Ember+ protocol confirmed";
    }
    
    
    m_glowParser->parseEmberData(emberData);
    
    
    if (isFirstData) {
        qDebug().noquote() << "Initial tree populated, emitting treePopulated signal";
        emit treePopulated();
    }
}

void EmberConnection::onKeepAliveReceived()
{
    
    qDebug() << "[EmberConnection] Sending KeepAlive RESPONSE to device";
    QByteArray response = m_s101Protocol->encodeKeepAliveResponse();
    qint64 bytesWritten = m_socket->write(response);
    m_socket->flush();
    qDebug() << "[EmberConnection] KeepAlive response sent:" << bytesWritten << "bytes";
}

void EmberConnection::onParserNodeReceived(const EmberData::NodeInfo& node)
{
    
    QStringList pathParts = node.path.split('.');
    int pathDepth = pathParts.size();
    
    if (pathDepth == 1) {
        QString displayName = !node.description.isEmpty() ? node.description : node.identifier;
        bool isGeneric = isGenericNodeName(displayName);
        
        qInfo().noquote() << QString("ROOT Node [%1]: Identifier='%2', Description='%3', Generic=%4")
            .arg(node.path).arg(node.identifier)
            .arg(node.description)
            .arg(isGeneric ? "YES" : "no");
        
        
        if (m_cacheManager->hasRootNode(node.path) && !m_cacheManager->isRootNodeGeneric(node.path)) {
            
            CacheManager::RootNodeInfo rootInfo = m_cacheManager->getRootNode(node.path);
            qDebug().noquote() << QString("Preserving existing root node name: %1").arg(rootInfo.displayName);
        } else {
            
            QString existingIdentityPath;
            if (m_cacheManager->hasRootNode(node.path)) {
                
                existingIdentityPath = m_cacheManager->getRootNode(node.path).identityPath;
            }
            m_cacheManager->setRootNode(node.path, displayName, isGeneric, existingIdentityPath);
        }
    }
    
    else if (pathDepth == 2) {
        QString parentPath = pathParts[0];
        if (m_cacheManager->hasRootNode(parentPath)) {
            
            QString nodeName = node.identifier.toLower();
            if (nodeName == "identity" || nodeName == "_identity" || 
                nodeName == "deviceinfo" || nodeName == "device_info") {
                m_cacheManager->updateRootNodeIdentityPath(parentPath, node.path);
                qInfo().noquote() << QString("Detected identity node for root %1: %2")
                    .arg(parentPath).arg(node.path);
            }
        }
    }
    
    qDebug().noquote() << QString("Node: %1 - Online: %2")
        .arg(node.path).arg(node.isOnline ? "YES" : "NO");
    
    
    emit nodeReceived(node.path, node.identifier, node.description, node.isOnline);
    
    
    if (m_treeFetchService->isActive()) {
        m_treeFetchService->onNodeReceived(node.path);
    }
    
    
    bool shouldAutoRequest = false;
    
    
    if (pathDepth == 1 && m_cacheManager->hasRootNode(node.path) && m_cacheManager->isRootNodeGeneric(node.path)) {
        shouldAutoRequest = true;
        qDebug().noquote() << QString("Auto-requesting children of root node %1 for name discovery").arg(node.path);
    }
    
    else if (pathDepth == 2) {
        QString rootPath = pathParts[0];
        if (m_cacheManager->hasRootNode(rootPath)) {
            CacheManager::RootNodeInfo rootInfo = m_cacheManager->getRootNode(rootPath);
            if (rootInfo.identityPath == node.path) {
                shouldAutoRequest = true;
                qDebug().noquote() << QString("Auto-requesting children of identity node %1 for name discovery").arg(node.path);
            }
        }
    }
    
    if (shouldAutoRequest) {
        
        sendGetDirectoryForPath(node.path, true);
    }
}

void EmberConnection::onParserParameterReceived(const EmberData::ParameterInfo& param)
{
    qDebug().noquote() << QString("Param %1 complete: '%2' = '%3' (Type=%4, Access=%5)")
        .arg(param.path).arg(param.identifier).arg(param.value).arg(param.type).arg(param.access);
    
    
    QStringList pathParts = param.path.split('.');
    if (pathParts.size() >= 3) {
        QString rootPath = pathParts[0];
        if (m_cacheManager->hasRootNode(rootPath) && m_cacheManager->isRootNodeGeneric(rootPath)) {
            
            QString paramName = param.identifier.toLower();
            if (paramName == "name" || paramName == "device name" || 
                paramName == "devicename" || paramName == "product") {
                
                
                CacheManager::RootNodeInfo rootInfo = m_cacheManager->getRootNode(rootPath);
                if (!rootInfo.identityPath.isEmpty()) {
                    if (param.path.startsWith(rootInfo.identityPath + ".")) {
                        qInfo().noquote() << QString("Found device name '%1' for root node %2 (from %3)")
                            .arg(param.value).arg(rootPath).arg(param.path);
                        
                        
                        m_cacheManager->updateRootNodeDisplayName(rootPath, param.value, false);
                        
                        
                        QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
                        CacheManager::cacheDevice(cacheKey, param.value, rootPath, rootInfo.identityPath);
                        
                        qDebug().noquote() << QString("Cached device name '%1' for %2")
                            .arg(param.value).arg(cacheKey);
                        
                        
                        emit nodeReceived(rootPath, param.value, param.value, true);
                    }
                }
            }
        }
    }
    
    emit parameterReceived(param.path, param.number, param.identifier, param.value, param.access, param.type, 
                          param.minimum, param.maximum, param.enumOptions, param.enumValues, param.isOnline, param.streamIdentifier);
}

void EmberConnection::onParserMatrixReceived(const EmberData::MatrixInfo& matrix)
{
    qDebug().noquote() << QString("Matrix: %1 [%2] - Type:%3, %4×%5")
                    .arg(matrix.identifier).arg(matrix.path).arg(matrix.type).arg(matrix.sourceCount).arg(matrix.targetCount);
    
    emit matrixReceived(matrix.path, matrix.number, matrix.identifier, matrix.description, 
                       matrix.type, matrix.targetCount, matrix.sourceCount);
}

bool EmberConnection::isGenericNodeName(const QString &name)
{
    
    
    static QStringList genericNames = {
        "Device", "Root", "device", "root", 
        "Node 0", "Node 1", "Node 2", "Node 3", "Node 4", "Node 5"
    };
    
    return genericNames.contains(name);
}

void EmberConnection::sendGetDirectory()
{
    sendGetDirectoryForPath("");
}

void EmberConnection::sendGetDirectoryForPath(const QString& path, bool optimizedForNameDiscovery)
{
    qInfo().noquote() << QString("sendGetDirectoryForPath called with path='%1'").arg(path);
    qInfo().noquote() << QString("m_requestedPaths size: %1").arg(m_requestedPaths.size());
    
    
    if (m_requestedPaths.contains(path)) {
        qInfo().noquote() << QString("ERROR: Skipping duplicate request for %1").arg(path.isEmpty() ? "root" : path);
        return;
    }
    qInfo().noquote() << "Passed duplicate check, inserting path";
    m_requestedPaths.insert(path);
    qInfo().noquote() << "Path inserted, continuing...";
    
    try {
        if (path.isEmpty()) {
            qInfo().noquote() << "Requesting root directory...";
        } else {
            qInfo().noquote() << QString("Requesting children of %1%2...")
                .arg(path)
                .arg(optimizedForNameDiscovery ? " (optimized for name discovery)" : "");
        }
        
        
        
        
        libember::glow::DirFieldMask fieldMask = optimizedForNameDiscovery
            ? libember::glow::DirFieldMask(libember::glow::DirFieldMask::Identifier | 
                                           libember::glow::DirFieldMask::Description | 
                                           libember::glow::DirFieldMask::Value)
            : libember::glow::DirFieldMask::All;
        
        qInfo().noquote() << "Creating GlowRootElementCollection...";
        auto root = new libember::glow::GlowRootElementCollection();
        
        if (path.isEmpty()) {
            
            qInfo().noquote() << "Creating bare GlowCommand for root...";
            auto command = new libember::glow::GlowCommand(
                libember::glow::CommandType::GetDirectory
            );
            qInfo().noquote() << "Inserting command into root...";
            root->insert(root->end(), command);
            qInfo().noquote() << "Command inserted successfully";
        }
        else {
            
            qInfo().noquote() << QString("Creating QualifiedNode for path: %1").arg(path);
            QStringList segments = path.split('.', Qt::SkipEmptyParts);
            libember::ber::ObjectIdentifier oid;
            
            for (const QString& seg : segments) {
                oid.push_back(seg.toInt());
            }
            
            auto node = new libember::glow::GlowQualifiedNode(oid);
            new libember::glow::GlowCommand(
                node,
                libember::glow::CommandType::GetDirectory
            );
            root->insert(root->end(), node);
            qInfo().noquote() << "QualifiedNode with command inserted (no field mask)";
        }
        
        
        qInfo().noquote() << "Encoding to EmBER...";
        libember::util::OctetStream stream;
        root->encode(stream);
        qInfo().noquote() << QString("EmBER payload size: %1 bytes").arg(stream.size());
        
        
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        
        qInfo().noquote() << QString("About to write %1 bytes to socket...").arg(s101Frame.size());
        if (m_socket->write(s101Frame) > 0) {
            m_socket->flush();
            qInfo().noquote() << QString("Successfully sent GetDirectory request (%1 bytes)").arg(s101Frame.size());
        }
        else {
            qCritical().noquote() << "Failed to send GetDirectory - socket write returned 0 or error";
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        qCritical().noquote() << QString("Error sending GetDirectory for %1: %2").arg(path).arg(ex.what());
    }
}

void EmberConnection::sendBatchGetDirectory(const QStringList& paths, bool optimizedForNameDiscovery)
{
    if (paths.isEmpty()) {
        return;
    }
    
    
    QStringList pathsToRequest;
    for (const QString& path : paths) {
        if (!m_requestedPaths.contains(path)) {
            pathsToRequest.append(path);
            m_requestedPaths.insert(path);
        } else {
            qDebug().noquote() << QString("Skipping duplicate request for %1").arg(path.isEmpty() ? "root" : path);
        }
    }
    
    if (pathsToRequest.isEmpty()) {
        return;
    }
    
    try {
        qDebug().noquote() << QString("Batch requesting %1 paths%2...")
            .arg(pathsToRequest.size())
            .arg(optimizedForNameDiscovery ? " (optimized for name discovery)" : "");
        
        
        libember::glow::DirFieldMask fieldMask = optimizedForNameDiscovery
            ? libember::glow::DirFieldMask(libember::glow::DirFieldMask::Identifier | 
                                           libember::glow::DirFieldMask::Description | 
                                           libember::glow::DirFieldMask::Value)
            : libember::glow::DirFieldMask::All;
        
        
        
        auto root = new libember::glow::GlowRootElementCollection();
        
        for (const QString& path : pathsToRequest) {
            if (path.isEmpty()) {
                
                auto command = new libember::glow::GlowCommand(
                    libember::glow::CommandType::GetDirectory,
                    fieldMask
                );
                root->insert(root->end(), command);
            }
            else {
                
                QStringList segments = path.split('.', Qt::SkipEmptyParts);
                libember::ber::ObjectIdentifier oid;
                
                for (const QString& seg : segments) {
                    oid.push_back(seg.toInt());
                }
                
                auto node = new libember::glow::GlowQualifiedNode(oid);
                new libember::glow::GlowCommand(
                    node,
                    libember::glow::CommandType::GetDirectory,
                    fieldMask
                );
                root->insert(root->end(), node);
            }
        }
        
        
        libember::util::OctetStream stream;
        root->encode(stream);
        
        
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        
        if (m_socket->write(s101Frame) > 0) {
            m_socket->flush();
        }
        else {
            qWarning().noquote() << "Failed to send batch GetDirectory";
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        qCritical().noquote() << QString("Error sending batch GetDirectory: %1").arg(ex.what());
    }
}

void EmberConnection::sendParameterValue(const QString &path, const QString &value, int type)
{
    try {
        qDebug().noquote() << QString("Setting parameter %1 = %2").arg(path).arg(value);
        
        
        QStringList segments = path.split('.', Qt::SkipEmptyParts);
        libember::ber::ObjectIdentifier oid;
        
        for (const QString& seg : segments) {
            oid.push_back(seg.toInt());
        }
        
        
        auto param = new libember::glow::GlowQualifiedParameter(oid);
        
        
        switch (type) {
            case libember::glow::ParameterType::Integer:
                param->setValue(static_cast<long>(value.toLongLong()));
                break;
                
            case libember::glow::ParameterType::Real:
                param->setValue(value.toDouble());
                break;
                
            case libember::glow::ParameterType::String:
                param->setValue(value.toStdString());
                break;
                
            case libember::glow::ParameterType::Boolean:
                param->setValue(value.toLower() == "true" || value == "1");
                break;
                
            case libember::glow::ParameterType::Enum:
                
                param->setValue(static_cast<long>(value.toLongLong()));
                break;
                
            case libember::glow::ParameterType::Trigger:
                
                param->setValue(libember::ber::Null());
                break;
                
            default:
                qWarning().noquote() << QString("Unsupported parameter type: %1").arg(type);
                delete param;
                return;
        }
        
        
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), param);
        
        
        libember::util::OctetStream stream;
        root->encode(stream);
        
        
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        
        if (m_socket->write(s101Frame) > 0) {
            m_socket->flush();
            qDebug().noquote() << QString("Successfully sent value for %1").arg(path);
        }
        else {
            qWarning().noquote() << QString("Failed to send value for %1").arg(path);
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        qCritical().noquote() << QString("Error sending parameter value for %1: %2").arg(path).arg(ex.what());
    }
}

void EmberConnection::setMatrixConnection(const QString &matrixPath, int targetNumber, int sourceNumber, bool connect)
{
    try {
        QString operation = connect ? "CONNECT" : "DISCONNECT";
        
        qDebug().noquote() << QString(">>> Sending %1: Matrix=%2, Target=%3, Source=%4")
                       .arg(operation).arg(matrixPath).arg(targetNumber).arg(sourceNumber);
        
        
        QStringList segments = matrixPath.split('.', Qt::SkipEmptyParts);
        libember::ber::ObjectIdentifier matrixOid;
        
        for (const QString& seg : segments) {
            matrixOid.push_back(seg.toInt());
        }
        
        
        auto matrix = new libember::glow::GlowQualifiedMatrix(matrixOid);
        
        
        auto connectionsSeq = matrix->connections();
        
        
        auto connection = new libember::glow::GlowConnection(targetNumber);
        
        
        if (connect) {
            connection->setOperation(libember::glow::ConnectionOperation::Connect);
            qDebug().noquote() << QString("    Operation: Connect (1)");
        } else {
            connection->setOperation(libember::glow::ConnectionOperation::Disconnect);
            qDebug().noquote() << QString("    Operation: Disconnect (2)");
        }
        
        
        
        libember::ber::ObjectIdentifier sources;
        sources.push_back(sourceNumber);
        connection->setSources(sources);
        qDebug().noquote() << QString("    Sources: [%1]").arg(sourceNumber);
        
        
        connectionsSeq->insert(connectionsSeq->end(), connection);
        
        
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), matrix);
        
        
        libember::util::OctetStream stream;
        root->encode(stream);
        
        
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        
        if (m_socket->write(s101Frame) > 0) {
            m_socket->flush();
            qDebug().noquote() << QString("Successfully sent matrix connection command");
        }
        else {
            qWarning().noquote() << QString("Failed to send matrix connection command");
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        qCritical().noquote() << QString("Error sending matrix connection for %1: %2").arg(matrixPath).arg(ex.what());
    }
}

void EmberConnection::invokeFunction(const QString &path, const QList<QVariant> &arguments)
{
    if (!m_connected) {
        qCritical().noquote() << "Cannot invoke function - not connected";
        return;
    }
    
    int invocationId = m_nextInvocationId++;
    m_pendingInvocations[invocationId] = path;
    
    QStringList pathParts = path.split('.');
    libember::ber::ObjectIdentifier oid;
    for (const QString &part : pathParts) {
        oid.push_back(part.toInt());
    }
    
    auto root = new libember::glow::GlowRootElementCollection();
    auto invocation = new libember::glow::GlowInvocation();
    invocation->setInvocationId(invocationId);
    
    if (!arguments.isEmpty()) {
        std::vector<libember::glow::Value> glowArgs;
        for (const QVariant &arg : arguments) {
            switch (arg.typeId()) {
                case QMetaType::Int:
                case QMetaType::LongLong:
                    glowArgs.push_back(libember::glow::Value(static_cast<long>(arg.toLongLong())));
                    break;
                case QMetaType::Double:
                    glowArgs.push_back(libember::glow::Value(arg.toDouble()));
                    break;
                case QMetaType::QString:
                    glowArgs.push_back(libember::glow::Value(arg.toString().toStdString()));
                    break;
                case QMetaType::Bool:
                    glowArgs.push_back(libember::glow::Value(arg.toBool()));
                    break;
                default:
                    qWarning().noquote() << QString("Unsupported argument type: %1").arg(arg.typeName());
                    break;
            }
        }
        invocation->setTypedArguments(glowArgs.begin(), glowArgs.end());
    }
    
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Invoke);
    command->setInvocation(invocation);
    
    auto qualifiedFunction = new libember::glow::GlowQualifiedFunction(oid);
    qualifiedFunction->children()->insert(qualifiedFunction->children()->end(), command);
    
    
    auto subscribeCmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    qualifiedFunction->children()->insert(qualifiedFunction->children()->end(), subscribeCmd);
    
    root->insert(root->end(), qualifiedFunction);
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    m_socket->write(s101Frame);
    m_socket->flush();
    
    qDebug().noquote() << QString("Sent function invocation for %1").arg(path);
    delete root;
}


void EmberConnection::subscribeToParameter(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot subscribe - not connected";
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Already subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto param = new libember::glow::GlowQualifiedParameter(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    param->children()->insert(param->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), param);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    qDebug().noquote() << QString("Subscribed to parameter: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)");
    
    delete root;
}

void EmberConnection::subscribeToNode(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot subscribe - not connected";
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Already subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto node = new libember::glow::GlowQualifiedNode(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    node->children()->insert(node->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), node);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    qDebug().noquote() << QString("Subscribed to node: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)");
    
    delete root;
}

void EmberConnection::subscribeToMatrix(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot subscribe - not connected";
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Already subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    matrix->children()->insert(matrix->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), matrix);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    qDebug().noquote() << QString("Subscribed to matrix: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)");
    
    delete root;
}

void EmberConnection::unsubscribeFromParameter(const QString &path)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot unsubscribe - not connected";
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Not subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto param = new libember::glow::GlowQualifiedParameter(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    param->children()->insert(param->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), param);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    m_subscriptions.remove(path);
    
    qDebug().noquote() << QString("Unsubscribed from parameter: %1").arg(path);
    
    delete root;
}

void EmberConnection::unsubscribeFromNode(const QString &path)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot unsubscribe - not connected";
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Not subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto node = new libember::glow::GlowQualifiedNode(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    node->children()->insert(node->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), node);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    m_subscriptions.remove(path);
    
    qDebug().noquote() << QString("Unsubscribed from node: %1").arg(path);
    
    delete root;
}

void EmberConnection::unsubscribeFromMatrix(const QString &path)
{
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot unsubscribe - not connected";
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        qDebug().noquote() << QString("Not subscribed to %1").arg(path);
        return;
    }
    
    
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    
    auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    matrix->children()->insert(matrix->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), matrix);
    
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    
    m_socket->write(s101Frame);
    m_socket->flush();
    
    
    m_subscriptions.remove(path);
    
    qDebug().noquote() << QString("Unsubscribed from matrix: %1").arg(path);
    
    delete root;
}

void EmberConnection::sendBatchSubscribe(const QList<SubscriptionRequest>& requests)
{
    if (requests.isEmpty()) {
        return;
    }
    
    if (!m_socket || !m_connected) {
        qWarning().noquote() << "Cannot batch subscribe - not connected";
        return;
    }
    
    
    QList<SubscriptionRequest> toSubscribe;
    for (const auto& req : requests) {
        if (!m_subscriptions.contains(req.path)) {
            toSubscribe.append(req);
        } else {
            qDebug().noquote() << QString("Skipping duplicate subscription for %1").arg(req.path);
        }
    }
    
    if (toSubscribe.isEmpty()) {
        qDebug().noquote() << "All paths already subscribed, skipping batch";
        return;
    }
    
    try {
        qDebug().noquote() << QString("Batch subscribing to %1 paths...").arg(toSubscribe.size());
        
        
        
        auto root = new libember::glow::GlowRootElementCollection();
        
        int successCount = 0;
        for (const auto& req : toSubscribe) {
            
            QStringList segments = req.path.split('.', Qt::SkipEmptyParts);
            libember::ber::ObjectIdentifier oid;
            for (const QString& seg : segments) {
                oid.push_back(seg.toInt());
            }
            
            
            if (req.type == "Node") {
                auto node = new libember::glow::GlowQualifiedNode(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                node->children()->insert(node->children()->end(), cmd);
                root->insert(root->end(), node);
                successCount++;
            }
            else if (req.type == "Parameter") {
                auto param = new libember::glow::GlowQualifiedParameter(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                param->children()->insert(param->children()->end(), cmd);
                root->insert(root->end(), param);
                successCount++;
            }
            else if (req.type == "Matrix") {
                auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                matrix->children()->insert(matrix->children()->end(), cmd);
                root->insert(root->end(), matrix);
                successCount++;
            }
            else if (req.type == "Function") {
                auto function = new libember::glow::GlowQualifiedFunction(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                function->children()->insert(function->children()->end(), cmd);
                root->insert(root->end(), function);
                successCount++;
            }
            else {
                qWarning().noquote() << QString("Unknown subscription type '%1' for path %2")
                    .arg(req.type).arg(req.path);
                continue;
            }
            
            
            SubscriptionState state;
            state.subscribedAt = QDateTime::currentDateTime();
            state.autoSubscribed = true;
            m_subscriptions[req.path] = state;
        }
        
        if (successCount == 0) {
            qWarning().noquote() << "No valid subscriptions in batch";
            delete root;
            return;
        }
        
        
        libember::util::OctetStream stream;
        root->encode(stream);
        
        
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        
        m_socket->write(s101Frame);
        m_socket->flush();
        
        qDebug().noquote() << QString("Successfully batch subscribed to %1 paths").arg(successCount);
        
        delete root;
    }
    catch (const std::exception &ex) {
        qCritical().noquote() << QString("Error sending batch subscribe: %1").arg(ex.what());
    }
}

bool EmberConnection::isSubscribed(const QString &path) const
{
    return m_subscriptions.contains(path);
}


void EmberConnection::fetchCompleteTree(const QStringList &initialNodePaths)
{
    if (!m_connected) {
        emit treeFetchCompleted(false, "Not connected to device");
        return;
    }
    
    qInfo().noquote() << QString("Starting complete tree fetch with %1 initial nodes...").arg(initialNodePaths.size());
    
    
    m_treeFetchService->setSendGetDirectoryCallback([this](const QString& path, bool isRoot) {
        qDebug().noquote() << QString("Tree fetch requesting: %1").arg(path.isEmpty() ? "root" : path);
        
        try {
            auto root = new libember::glow::GlowRootElementCollection();
            
            if (path.isEmpty()) {
                
                auto command = new libember::glow::GlowCommand(
                    libember::glow::CommandType::GetDirectory,
                    libember::glow::DirFieldMask::All
                );
                root->insert(root->end(), command);
            }
            else {
                
                QStringList segments = path.split('.', Qt::SkipEmptyParts);
                libember::ber::ObjectIdentifier oid;
                
                for (const QString& seg : segments) {
                    oid.push_back(seg.toInt());
                }
                
                auto node = new libember::glow::GlowQualifiedNode(oid);
                new libember::glow::GlowCommand(
                    node,
                    libember::glow::CommandType::GetDirectory,
                    libember::glow::DirFieldMask::All
                );
                root->insert(root->end(), node);
            }
            
            
            libember::util::OctetStream stream;
            root->encode(stream);
            
            
            QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
            
            
            if (m_socket->write(s101Frame) > 0) {
                m_socket->flush();
            }
            
            delete root;
        }
        catch (const std::exception &ex) {
            qCritical().noquote() << QString("Error sending GetDirectory for tree fetch: %1").arg(ex.what());
        }
    });
    
    
    connect(m_treeFetchService, &TreeFetchService::progressUpdated, this, &EmberConnection::treeFetchProgress, Qt::UniqueConnection);
    connect(m_treeFetchService, &TreeFetchService::fetchCompleted, this, &EmberConnection::treeFetchCompleted, Qt::UniqueConnection);
    
    
    m_treeFetchService->startFetch(initialNodePaths);
}

void EmberConnection::cancelTreeFetch()
{
    m_treeFetchService->cancel();
}

bool EmberConnection::isTreeFetchActive() const
{
    return m_treeFetchService->isActive();
}

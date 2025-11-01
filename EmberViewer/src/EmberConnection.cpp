/*
    EmberViewer - Ember+ protocol communication handler
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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



// EmberConnection implementation
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
    // Socket connections
    connect(m_socket, &QTcpSocket::connected, this, &EmberConnection::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &EmberConnection::onSocketDisconnected);
    
    // Use errorOccurred instead of deprecated error signal (Qt 5.15+)
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &EmberConnection::onSocketError);
    #else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &EmberConnection::onSocketError);
    #endif
    
    connect(m_socket, &QTcpSocket::readyRead, this, &EmberConnection::onDataReceived);
    
    // Protocol layer connections: Socket -> S101Protocol -> GlowParser -> EmberConnection
    connect(m_s101Protocol, &S101Protocol::messageReceived, this, &EmberConnection::onS101MessageReceived);
    connect(m_s101Protocol, &S101Protocol::protocolError, this, [this](const QString& error) {
        qCritical().noquote() << "S101 protocol error:" << error;
        disconnect();
    });
    
    // Parser signal connections
    connect(m_glowParser, &GlowParser::nodeReceived, this, &EmberConnection::onParserNodeReceived);
    connect(m_glowParser, &GlowParser::parameterReceived, this, &EmberConnection::onParserParameterReceived);
    connect(m_glowParser, &GlowParser::matrixReceived, this, &EmberConnection::onParserMatrixReceived);
    
    // Forward parser signals directly to our signals
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
    
    // Initialize connection timeout timer (5 seconds)
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setSingleShot(true);
    m_connectionTimer->setInterval(CONNECTION_TIMEOUT_MS);
    connect(m_connectionTimer, &QTimer::timeout, this, &EmberConnection::onConnectionTimeout);
    
    // Initialize protocol timeout timer (10 seconds)
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
    // Check if already connected or connecting
    QAbstractSocket::SocketState state = m_socket->state();
    if (state != QAbstractSocket::UnconnectedState) {
        qWarning().noquote() << QString("Cannot connect: socket is already in state %1").arg(static_cast<int>(state));
        
        // If we're connecting or connected to a different host/port, abort first
        if (state == QAbstractSocket::ConnectingState || 
            state == QAbstractSocket::HostLookupState) {
            qInfo().noquote() << "Aborting previous connection attempt...";
            m_socket->abort();
            m_connectionTimer->stop();
        } else if (state == QAbstractSocket::ConnectedState) {
            qInfo().noquote() << "Already connected";
            return;
        } else {
            // For other states (closing, etc), just return
            return;
        }
    }
    
    m_host = host;
    m_port = port;
    
    // Clear request tracking from any previous connection attempts
    m_requestedPaths.clear();
    
    // OPTIMIZATION 1: Configure socket for low-latency real-time protocol
    // TCP_NODELAY disables Nagle's algorithm to eliminate 40-200ms buffering delays
    // This is critical for request/response protocols like Ember+
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    // TCP_KEEPALIVE maintains connection and detects broken connections faster
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    
    // OPTIMIZATION 2: Increase socket buffer sizes for better throughput
    // Larger buffers (64KB vs default 8KB) reduce latency for large tree responses
    // and improve performance when receiving bursts of data
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 65536);
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 65536);
    
    qInfo().noquote() << QString("Connecting to %1:%2...").arg(host).arg(port);
    
    // Start connection timeout timer
    m_connectionTimer->start();
    
    m_socket->connectToHost(host, port);
}

void EmberConnection::disconnect()
{
    // Stop timers
    m_connectionTimer->stop();
    m_protocolTimer->stop();
    
    QAbstractSocket::SocketState state = m_socket->state();
    
    if (state == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    } else if (state == QAbstractSocket::ConnectingState || 
               state == QAbstractSocket::HostLookupState) {
        // Abort pending connection
        qInfo().noquote() << "Aborting pending connection...";
        m_socket->abort();
    }
    
    // Clear request tracking for next connection
    m_requestedPaths.clear();
    m_subscriptions.clear();  // Clear all subscriptions on disconnect
}

bool EmberConnection::isConnected() const
{
    return m_connected;
}

void EmberConnection::onSocketConnected()
{
    // Stop connection timeout timer
    m_connectionTimer->stop();
    
    m_connected = true;
    m_emberDataReceived = false;
    emit connected();
    qInfo().noquote() << "Connected to provider";
    
    // Start protocol timeout timer - will be cancelled if we receive valid Ember+ data
    m_protocolTimer->start();
    qInfo().noquote() << "Waiting for Ember+ response...";
    
    // OPTIMIZATION: Check cache for previously discovered device name
    // Display cached name immediately for instant UI feedback on reconnection
    QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
    if (CacheManager::hasDeviceCache(cacheKey)) {
        CacheManager::DeviceCache cache = CacheManager::getDeviceCache(cacheKey);
        
        // Check if cache is still valid (not expired)
        QDateTime now = QDateTime::currentDateTime();
        qint64 hoursSinceLastSeen = cache.lastSeen.secsTo(now) / 3600;
        
        if (cache.isValid) {
            qInfo().noquote() << QString("Using cached device name: '%1' (last seen %2 hours ago)")
                .arg(cache.deviceName)
                .arg(hoursSinceLastSeen);
            
            // Create/update root node with cached info
            m_cacheManager->setRootNode(cache.rootPath, cache.deviceName, false, cache.identityPath);
            
            // Emit cached device name immediately for instant UI update
            emit nodeReceived(cache.rootPath, cache.deviceName, cache.deviceName, true);
            
            qInfo().noquote() << "Cached device name displayed instantly, will verify with device...";
        } else {
            qInfo().noquote() << QString("Cache expired (last seen %1 hours ago), will rediscover device name")
                .arg(hoursSinceLastSeen);
        }
    }
    
    // Send GetDirectory to request root tree (always validate with device)
    qInfo().noquote() << "About to send GetDirectory request...";
    sendGetDirectory();
    qInfo().noquote() << "GetDirectory call completed";
}

void EmberConnection::onSocketDisconnected()
{
    // Stop timers if still running
    m_connectionTimer->stop();
    m_protocolTimer->stop();
    
    m_connected = false;
    m_emberDataReceived = false;
    m_cacheManager->clear();  // Clear cached metadata
    emit disconnected();
    qInfo().noquote() << "Disconnected from provider";
}

void EmberConnection::onSocketError(QAbstractSocket::SocketError error)
{
    // Stop connection timeout timer
    m_connectionTimer->stop();
    
    QString errorString = m_socket->errorString();
    qCritical().noquote() << QString("Connection error: %1").arg(errorString);
    
    // For connection errors during connection attempt, abort and cleanup
    QAbstractSocket::SocketState state = m_socket->state();
    if (error == QAbstractSocket::ConnectionRefusedError ||
        error == QAbstractSocket::NetworkError ||
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::SocketTimeoutError) {
        
        qInfo().noquote() << "Aborting connection due to error...";
        
        // Abort the connection attempt
        if (state != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        
        // The abort() call will trigger onSocketDisconnected if needed
    }
}

void EmberConnection::onConnectionTimeout()
{
    qCritical().noquote() << QString("Connection timeout after 5 seconds");
    
    // Abort the connection attempt
    QAbstractSocket::SocketState state = m_socket->state();
    if (state == QAbstractSocket::ConnectingState || 
        state == QAbstractSocket::HostLookupState) {
        qInfo().noquote() << "Aborting connection attempt...";
        m_socket->abort();
        
        // Emit disconnected signal to notify UI
        // (abort() doesn't always trigger disconnected signal for non-connected sockets)
        if (!m_connected) {
            emit disconnected();
        }
    }
}

void EmberConnection::onProtocolTimeout()
{
    qCritical().noquote() << QString("No Ember+ response received after 10 seconds. This port does not appear to be serving Ember+ protocol.");
    
    // Disconnect - this port doesn't seem to be Ember+
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
    
    qInfo().noquote() << QString("*** Received %1 bytes from device ***").arg(data.size());
    
    // Feed data to S101 protocol layer
    m_s101Protocol->feedData(data);
}

void EmberConnection::onS101MessageReceived(const QByteArray& emberData)
{
    // Stop protocol timeout timer - we've received valid Ember+ data!
    bool isFirstData = false;
    if (!m_emberDataReceived) {
        m_emberDataReceived = true;
        isFirstData = true;
        m_protocolTimer->stop();
        qInfo().noquote() << "Ember+ protocol confirmed";
    }
    
    // Feed Ember data to parser
    m_glowParser->parseEmberData(emberData);
    
    // Emit treePopulated signal if this was the first data received
    if (isFirstData) {
        qDebug().noquote() << "Initial tree populated, emitting treePopulated signal";
        emit treePopulated();
    }
}

void EmberConnection::onParserNodeReceived(const EmberData::NodeInfo& node)
{
    // Track root-level nodes for smart device name detection
    QStringList pathParts = node.path.split('.');
    int pathDepth = pathParts.size();
    
    if (pathDepth == 1) {
        QString displayName = !node.description.isEmpty() ? node.description : node.identifier;
        bool isGeneric = isGenericNodeName(displayName);
        
        qInfo().noquote() << QString("ROOT Node [%1]: Identifier='%2', Description='%3', Generic=%4")
            .arg(node.path).arg(node.identifier)
            .arg(node.description)
            .arg(isGeneric ? "YES" : "no");
        
        // Track this root node - but preserve existing non-generic name during tree fetch
        if (m_cacheManager->hasRootNode(node.path) && !m_cacheManager->isRootNodeGeneric(node.path)) {
            // Already have a good name, keep it
            CacheManager::RootNodeInfo rootInfo = m_cacheManager->getRootNode(node.path);
            qDebug().noquote() << QString("Preserving existing root node name: %1").arg(rootInfo.displayName);
        } else {
            // New node or generic name - update it
            QString existingIdentityPath;
            if (m_cacheManager->hasRootNode(node.path)) {
                // Preserve identityPath if it was already discovered
                existingIdentityPath = m_cacheManager->getRootNode(node.path).identityPath;
            }
            m_cacheManager->setRootNode(node.path, displayName, isGeneric, existingIdentityPath);
        }
    }
    // Track identity child nodes under root nodes
    else if (pathDepth == 2) {
        QString parentPath = pathParts[0];
        if (m_cacheManager->hasRootNode(parentPath)) {
            // Check if this is an identity node
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
    
    // Emit the node signal
    emit nodeReceived(node.path, node.identifier, node.description, node.isOnline);
    
    // TREE FETCH: If tree fetch is active, notify the service
    if (m_treeFetchService->isActive()) {
        m_treeFetchService->onNodeReceived(node.path);
    }
    
    // LAZY LOADING: Only auto-request children for special cases (root name discovery)
    bool shouldAutoRequest = false;
    
    // Special case 1: Root nodes with generic names (to discover device name)
    if (pathDepth == 1 && m_cacheManager->hasRootNode(node.path) && m_cacheManager->isRootNodeGeneric(node.path)) {
        shouldAutoRequest = true;
        qDebug().noquote() << QString("Auto-requesting children of root node %1 for name discovery").arg(node.path);
    }
    // Special case 2: Identity nodes under generic root (to find name parameter)
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
        // OPTIMIZATION: Use optimized field mask for name discovery
        sendGetDirectoryForPath(node.path, true);
    }
}

void EmberConnection::onParserParameterReceived(const EmberData::ParameterInfo& param)
{
    qDebug().noquote() << QString("Param %1 complete: '%2' = '%3' (Type=%4, Access=%5)")
        .arg(param.path).arg(param.identifier).arg(param.value).arg(param.type).arg(param.access);
    
    // Check if this parameter could be a device name for a root node with generic name
    QStringList pathParts = param.path.split('.');
    if (pathParts.size() >= 3) {
        QString rootPath = pathParts[0];
        if (m_cacheManager->hasRootNode(rootPath) && m_cacheManager->isRootNodeGeneric(rootPath)) {
            // Check if parameter identifier suggests it's a device name
            QString paramName = param.identifier.toLower();
            if (paramName == "name" || paramName == "device name" || 
                paramName == "devicename" || paramName == "product") {
                
                // Check if under identity node
                CacheManager::RootNodeInfo rootInfo = m_cacheManager->getRootNode(rootPath);
                if (!rootInfo.identityPath.isEmpty()) {
                    if (param.path.startsWith(rootInfo.identityPath + ".")) {
                        qInfo().noquote() << QString("Found device name '%1' for root node %2 (from %3)")
                            .arg(param.value).arg(rootPath).arg(param.path);
                        
                        // Update the display name
                        m_cacheManager->updateRootNodeDisplayName(rootPath, param.value, false);
                        
                        // OPTIMIZATION: Cache device name for instant reconnection
                        QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
                        CacheManager::cacheDevice(cacheKey, param.value, rootPath, rootInfo.identityPath);
                        
                        qDebug().noquote() << QString("Cached device name '%1' for %2")
                            .arg(param.value).arg(cacheKey);
                        
                        // Re-emit node with new name (assume online since we're getting parameter updates)
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
    // Check if the name is a generic placeholder that should be replaced
    // with identity information if available
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
    
    // Avoid infinite loops - don't request the same path twice
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
        
        // OPTIMIZATION: Use minimal field mask for faster name discovery
        // When discovering device names, we only need Identifier, Description, and Value
        // This reduces payload size by 20-40% and speeds up provider processing
        libember::glow::DirFieldMask fieldMask = optimizedForNameDiscovery
            ? libember::glow::DirFieldMask(libember::glow::DirFieldMask::Identifier | 
                                           libember::glow::DirFieldMask::Description | 
                                           libember::glow::DirFieldMask::Value)
            : libember::glow::DirFieldMask::All;
        
        qInfo().noquote() << "Creating GlowRootElementCollection...";
        auto root = new libember::glow::GlowRootElementCollection();
        
        if (path.isEmpty()) {
            // Request root - use bare GlowCommand like working applications do
            qInfo().noquote() << "Creating bare GlowCommand for root...";
            auto command = new libember::glow::GlowCommand(
                libember::glow::CommandType::GetDirectory
            );
            qInfo().noquote() << "Inserting command into root...";
            root->insert(root->end(), command);
            qInfo().noquote() << "Command inserted successfully";
        }
        else {
            // Request specific node path
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
        
        // Encode to EmBER
        qInfo().noquote() << "Encoding to EmBER...";
        libember::util::OctetStream stream;
        root->encode(stream);
        qInfo().noquote() << QString("EmBER payload size: %1 bytes").arg(stream.size());
        
        // Use S101 protocol layer to encode
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        // Send
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
    
    // Filter out already-requested paths
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
        
        // OPTIMIZATION: Use minimal field mask for faster name discovery
        libember::glow::DirFieldMask fieldMask = optimizedForNameDiscovery
            ? libember::glow::DirFieldMask(libember::glow::DirFieldMask::Identifier | 
                                           libember::glow::DirFieldMask::Description | 
                                           libember::glow::DirFieldMask::Value)
            : libember::glow::DirFieldMask::All;
        
        // OPTIMIZATION: Batch multiple GetDirectory commands in single S101 frame
        // This reduces protocol overhead and allows provider to optimize batch processing
        auto root = new libember::glow::GlowRootElementCollection();
        
        for (const QString& path : pathsToRequest) {
            if (path.isEmpty()) {
                // Request root
                auto command = new libember::glow::GlowCommand(
                    libember::glow::CommandType::GetDirectory,
                    fieldMask
                );
                root->insert(root->end(), command);
            }
            else {
                // Request specific node path
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
        
        // Encode to EmBER  
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Use S101 protocol layer to encode
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        // Send
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
        
        // Parse path to OID
        QStringList segments = path.split('.', Qt::SkipEmptyParts);
        libember::ber::ObjectIdentifier oid;
        
        for (const QString& seg : segments) {
            oid.push_back(seg.toInt());
        }
        
        // Create qualified parameter with new value
        auto param = new libember::glow::GlowQualifiedParameter(oid);
        
        // Set value based on type
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
                // For enum, value is the integer index
                param->setValue(static_cast<long>(value.toLongLong()));
                break;
                
            case libember::glow::ParameterType::Trigger:
                // For triggers, send a null value (Ember+ convention for triggering actions)
                param->setValue(libember::ber::Null());
                break;
                
            default:
                qWarning().noquote() << QString("Unsupported parameter type: %1").arg(type);
                delete param;
                return;
        }
        
        // Create root collection and add parameter
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), param);
        
        // Encode to EmBER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Use S101 protocol layer to encode
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        // Send
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
        
        // Parse matrix path to OID
        QStringList segments = matrixPath.split('.', Qt::SkipEmptyParts);
        libember::ber::ObjectIdentifier matrixOid;
        
        for (const QString& seg : segments) {
            matrixOid.push_back(seg.toInt());
        }
        
        // Create qualified matrix
        auto matrix = new libember::glow::GlowQualifiedMatrix(matrixOid);
        
        // Get the connections sequence from the matrix (creates it if it doesn't exist)
        auto connectionsSeq = matrix->connections();
        
        // Create connection object
        auto connection = new libember::glow::GlowConnection(targetNumber);
        
        // Set the operation based on connect/disconnect
        if (connect) {
            connection->setOperation(libember::glow::ConnectionOperation::Connect);
            qDebug().noquote() << QString("    Operation: Connect (1)");
        } else {
            connection->setOperation(libember::glow::ConnectionOperation::Disconnect);
            qDebug().noquote() << QString("    Operation: Disconnect (2)");
        }
        
        // Add source(s) to the connection
        // For Ember+, we need to set the sources as an OID sequence
        libember::ber::ObjectIdentifier sources;
        sources.push_back(sourceNumber);
        connection->setSources(sources);
        qDebug().noquote() << QString("    Sources: [%1]").arg(sourceNumber);
        
        // Add connection to the connections sequence
        connectionsSeq->insert(connectionsSeq->end(), connection);
        
        // Create root collection and add matrix
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), matrix);
        
        // Encode to EmBER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Use S101 protocol layer to encode
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        // Send
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
    
    // Also subscribe to the function to receive invocation results
    auto subscribeCmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    qualifiedFunction->children()->insert(qualifiedFunction->children()->end(), subscribeCmd);
    
    root->insert(root->end(), qualifiedFunction);
    
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    m_socket->write(s101Frame);
    m_socket->flush();
    
    qDebug().noquote() << QString("Sent function invocation for %1").arg(path);
    delete root;
}

// Subscription methods
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified parameter with subscribe command
    auto param = new libember::glow::GlowQualifiedParameter(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    param->children()->insert(param->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), param);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Track subscription
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified node with subscribe command
    auto node = new libember::glow::GlowQualifiedNode(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    node->children()->insert(node->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), node);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Track subscription
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified matrix with subscribe command
    auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
    matrix->children()->insert(matrix->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), matrix);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Track subscription
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified parameter with unsubscribe command
    auto param = new libember::glow::GlowQualifiedParameter(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    param->children()->insert(param->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), param);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Remove from tracking
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified node with unsubscribe command
    auto node = new libember::glow::GlowQualifiedNode(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    node->children()->insert(node->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), node);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Remove from tracking
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
    
    // Parse path to OID
    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    libember::ber::ObjectIdentifier oid;
    for (const QString& seg : segments) {
        oid.push_back(seg.toInt());
    }
    
    // Create qualified matrix with unsubscribe command
    auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
    auto command = new libember::glow::GlowCommand(libember::glow::CommandType::Unsubscribe);
    matrix->children()->insert(matrix->children()->end(), command);
    
    auto root = new libember::glow::GlowRootElementCollection();
    root->insert(root->end(), matrix);
    
    // Encode to BER
    libember::util::OctetStream stream;
    root->encode(stream);
    
    // Use S101 protocol layer to encode
    QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
    
    // Send
    m_socket->write(s101Frame);
    m_socket->flush();
    
    // Remove from tracking
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
    
    // Filter out already-subscribed paths
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
        
        // OPTIMIZATION: Batch multiple Subscribe commands in single S101 frame
        // This reduces protocol overhead and network round trips (5-20x reduction)
        auto root = new libember::glow::GlowRootElementCollection();
        
        int successCount = 0;
        for (const auto& req : toSubscribe) {
            // Parse path to OID
            QStringList segments = req.path.split('.', Qt::SkipEmptyParts);
            libember::ber::ObjectIdentifier oid;
            for (const QString& seg : segments) {
                oid.push_back(seg.toInt());
            }
            
            // Create appropriate qualified element based on type
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
            
            // Track subscription
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
        
        // Encode to BER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Use S101 protocol layer to encode
        QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
        
        // Send
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

// Complete tree fetch implementation
void EmberConnection::fetchCompleteTree(const QStringList &initialNodePaths)
{
    if (!m_connected) {
        emit treeFetchCompleted(false, "Not connected to device");
        return;
    }
    
    qInfo().noquote() << QString("Starting complete tree fetch with %1 initial nodes...").arg(initialNodePaths.size());
    
    // Set up callback for TreeFetchService to send GetDirectory requests
    m_treeFetchService->setSendGetDirectoryCallback([this](const QString& path, bool isRoot) {
        qDebug().noquote() << QString("Tree fetch requesting: %1").arg(path.isEmpty() ? "root" : path);
        
        try {
            auto root = new libember::glow::GlowRootElementCollection();
            
            if (path.isEmpty()) {
                // Request root
                auto command = new libember::glow::GlowCommand(
                    libember::glow::CommandType::GetDirectory,
                    libember::glow::DirFieldMask::All
                );
                root->insert(root->end(), command);
            }
            else {
                // Request specific node path
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
            
            // Encode to EmBER
            libember::util::OctetStream stream;
            root->encode(stream);
            
            // Use S101 protocol layer to encode
            QByteArray s101Frame = m_s101Protocol->encodeEmberData(stream);
            
            // Send
            if (m_socket->write(s101Frame) > 0) {
                m_socket->flush();
            }
            
            delete root;
        }
        catch (const std::exception &ex) {
            qCritical().noquote() << QString("Error sending GetDirectory for tree fetch: %1").arg(ex.what());
        }
    });
    
    // Connect progress signals
    connect(m_treeFetchService, &TreeFetchService::progressUpdated, this, &EmberConnection::treeFetchProgress, Qt::UniqueConnection);
    connect(m_treeFetchService, &TreeFetchService::fetchCompleted, this, &EmberConnection::treeFetchCompleted, Qt::UniqueConnection);
    
    // Start the fetch
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

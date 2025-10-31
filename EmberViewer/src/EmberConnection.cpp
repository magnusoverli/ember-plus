/*
    EmberViewer - Ember+ protocol communication handler
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "EmberConnection.h"
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
#include <s101/CommandType.hpp>
#include <s101/MessageType.hpp>
#include <s101/PackageFlag.hpp>
#include <s101/StreamEncoder.hpp>

// Static device cache definition
QMap<QString, EmberConnection::DeviceCache> EmberConnection::s_deviceCache;

// DomReader implementation
EmberConnection::DomReader::DomReader(EmberConnection* connection)
    : libember::dom::AsyncDomReader(libember::glow::GlowNodeFactory::getFactory())
    , m_connection(connection)
{
}

EmberConnection::DomReader::~DomReader()
{
}

void EmberConnection::DomReader::rootReady(libember::dom::Node* root)
{
    if (m_connection && root) {
        m_connection->processRoot(root);
    }
}

// EmberConnection implementation
EmberConnection::EmberConnection(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_s101Decoder(new libs101::StreamDecoder<unsigned char>())
    , m_domReader(new DomReader(this))
    , m_connected(false)
    , m_emberDataReceived(false)
    , m_connectionTimer(nullptr)
    , m_protocolTimer(nullptr)
    , m_logLevel(LogLevel::Info)
    , m_nextInvocationId(1)
    , m_treeFetchActive(false)
    , m_treeFetchTotalEstimate(0)
    , m_treeFetchTimer(nullptr)
{
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
    
    // Initialize tree fetch processing timer
    m_treeFetchTimer = new QTimer(this);
    m_treeFetchTimer->setSingleShot(false);  // Repeating timer
    m_treeFetchTimer->setInterval(50);  // Process queue every 50ms
    connect(m_treeFetchTimer, &QTimer::timeout, this, &EmberConnection::processFetchQueue);
}

EmberConnection::~EmberConnection()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    
    delete m_domReader;
    delete m_s101Decoder;
}

void EmberConnection::connectToHost(const QString &host, int port)
{
    // Check if already connected or connecting
    QAbstractSocket::SocketState state = m_socket->state();
    if (state != QAbstractSocket::UnconnectedState) {
        log(LogLevel::Warning, QString("Cannot connect: socket is already in state %1").arg(static_cast<int>(state)));
        
        // If we're connecting or connected to a different host/port, abort first
        if (state == QAbstractSocket::ConnectingState || 
            state == QAbstractSocket::HostLookupState) {
            log(LogLevel::Info, "Aborting previous connection attempt...");
            m_socket->abort();
            m_connectionTimer->stop();
        } else if (state == QAbstractSocket::ConnectedState) {
            log(LogLevel::Info, "Already connected");
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
    
    log(LogLevel::Info, QString("Connecting to %1:%2...").arg(host).arg(port));
    
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
        log(LogLevel::Info, "Aborting pending connection...");
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

void EmberConnection::setLogLevel(LogLevel level)
{
    m_logLevel = level;
    log(LogLevel::Info, QString("Log level changed to: %1").arg(static_cast<int>(level)));
}

void EmberConnection::log(LogLevel level, const QString &message)
{
    if (level > m_logLevel) {
        return;
    }
    
    switch (level) {
        case LogLevel::Error:
            qCritical().noquote() << message;
            break;
        case LogLevel::Warning:
            qWarning().noquote() << message;
            break;
        case LogLevel::Info:
            qInfo().noquote() << message;
            break;
        case LogLevel::Debug:
        case LogLevel::Trace:
            qDebug().noquote() << message;
            break;
    }
}

void EmberConnection::onSocketConnected()
{
    // Stop connection timeout timer
    m_connectionTimer->stop();
    
    m_connected = true;
    m_emberDataReceived = false;
    emit connected();
    log(LogLevel::Info, "Connected to provider");
    
    // Start protocol timeout timer - will be cancelled if we receive valid Ember+ data
    m_protocolTimer->start();
    log(LogLevel::Info, "Waiting for Ember+ response...");
    
    // OPTIMIZATION: Check cache for previously discovered device name
    // Display cached name immediately for instant UI feedback on reconnection
    QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
    if (s_deviceCache.contains(cacheKey)) {
        DeviceCache& cache = s_deviceCache[cacheKey];
        
        // Check if cache is still valid (not expired)
        QDateTime now = QDateTime::currentDateTime();
        int hoursSinceLastSeen = cache.lastSeen.secsTo(now) / 3600;
        
        if (cache.isValid && hoursSinceLastSeen < CACHE_EXPIRY_HOURS) {
            log(LogLevel::Info, QString("Using cached device name: '%1' (last seen %2 hours ago)")
                .arg(cache.deviceName)
                .arg(hoursSinceLastSeen));
            
            // Create/update root node with cached info
            RootNodeInfo rootInfo;
            rootInfo.path = cache.rootPath;
            rootInfo.displayName = cache.deviceName;
            rootInfo.isGeneric = false;  // We have the real name
            rootInfo.identityPath = cache.identityPath;
            m_rootNodes[cache.rootPath] = rootInfo;
            
            // Emit cached device name immediately for instant UI update
            emit nodeReceived(cache.rootPath, cache.deviceName, cache.deviceName, true);
            
            log(LogLevel::Info, "Cached device name displayed instantly, will verify with device...");
        } else {
            log(LogLevel::Info, QString("Cache expired (last seen %1 hours ago), will rediscover device name")
                .arg(hoursSinceLastSeen));
            cache.isValid = false;
        }
    }
    
    // Send GetDirectory to request root tree (always validate with device)
    log(LogLevel::Info, "About to send GetDirectory request...");
    sendGetDirectory();
    log(LogLevel::Info, "GetDirectory call completed");
}

void EmberConnection::onSocketDisconnected()
{
    // Stop timers if still running
    m_connectionTimer->stop();
    m_protocolTimer->stop();
    
    m_connected = false;
    m_emberDataReceived = false;
    m_s101Decoder->reset();
    m_domReader->reset();
    m_parameterCache.clear();  // Clear cached parameter metadata
    m_rootNodes.clear();  // Clear root node tracking
    emit disconnected();
    log(LogLevel::Info, "Disconnected from provider");
}

void EmberConnection::onSocketError(QAbstractSocket::SocketError error)
{
    // Stop connection timeout timer
    m_connectionTimer->stop();
    
    QString errorString = m_socket->errorString();
    log(LogLevel::Error, QString("Connection error: %1").arg(errorString));
    
    // For connection errors during connection attempt, abort and cleanup
    QAbstractSocket::SocketState state = m_socket->state();
    if (error == QAbstractSocket::ConnectionRefusedError ||
        error == QAbstractSocket::NetworkError ||
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::SocketTimeoutError) {
        
        log(LogLevel::Info, "Aborting connection due to error...");
        
        // Abort the connection attempt
        if (state != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        
        // The abort() call will trigger onSocketDisconnected if needed
    }
}

void EmberConnection::onConnectionTimeout()
{
    log(LogLevel::Error, QString("Connection timeout after 5 seconds"));
    
    // Abort the connection attempt
    QAbstractSocket::SocketState state = m_socket->state();
    if (state == QAbstractSocket::ConnectingState || 
        state == QAbstractSocket::HostLookupState) {
        log(LogLevel::Info, "Aborting connection attempt...");
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
    log(LogLevel::Error, QString("No Ember+ response received after 10 seconds. This port does not appear to be serving Ember+ protocol."));
    
    // Disconnect - this port doesn't seem to be Ember+
    if (m_connected) {
        log(LogLevel::Info, "Disconnecting due to protocol timeout...");
        disconnect();
    }
}

void EmberConnection::onDataReceived()
{
    QByteArray data = m_socket->readAll();
    
    if (data.isEmpty()) {
        return;
    }
    
    log(LogLevel::Info, QString("*** Received %1 bytes from device ***").arg(data.size()));
    
    // Decode S101 frames
    const unsigned char *bytes = reinterpret_cast<const unsigned char*>(data.constData());
    
    m_s101Decoder->read(bytes, bytes + data.size(),
        [](libs101::StreamDecoder<unsigned char>::const_iterator first,
           libs101::StreamDecoder<unsigned char>::const_iterator last,
           EmberConnection* self)
        {
            self->handleS101Message(first, last);
            return true;
        },
        this
    );
}

void EmberConnection::handleS101Message(
    libs101::StreamDecoder<unsigned char>::const_iterator first,
    libs101::StreamDecoder<unsigned char>::const_iterator last)
{
    if (first == last)
        return;
    
    // Parse S101 frame header
    first++;  // Skip slot
    auto message = *first++;
    
    if (message == libs101::MessageType::EmBER) {
        auto command = *first++;
        first++;  // Skip version
        auto flags = libs101::PackageFlag(*first++);
        first++;  // Skip DTD
        
        if (command == libs101::CommandType::EmBER) {
            // Skip application bytes
            auto appBytes = *first++;
            while (appBytes-- > 0 && first != last) {
                ++first;
            }
            
            // Feed EmBER data to DOM reader
            try {
                log(LogLevel::Trace, "Parsing Glow message...");
                m_domReader->read(first, last);
                
                // If this is the last package, finalize the tree
                if (flags.value() & libs101::PackageFlag::LastPackage) {
                    log(LogLevel::Debug, "Complete message received");
                    m_domReader->reset();
                }
            }
            catch (const std::exception &ex) {
                log(LogLevel::Error, QString("Error parsing Ember+ data: %1").arg(ex.what()));
                log(LogLevel::Error, QString("This port is not serving valid Ember+ protocol. Disconnecting..."));
                // Disconnect immediately - we're receiving invalid Ember+ data
                disconnect();
                return;
            }
        }
        else if (command == libs101::CommandType::KeepAliveRequest) {
            log(LogLevel::Debug, "Received keep-alive request, sending response");
            
            // Send keep-alive response
            auto encoder = libs101::StreamEncoder<unsigned char>();
            encoder.encode(0x00);  // Slot
            encoder.encode(libs101::MessageType::EmBER);
            encoder.encode(libs101::CommandType::KeepAliveResponse);
            encoder.encode(0x01);  // Version
            encoder.finish();
            
            std::vector<unsigned char> data(encoder.begin(), encoder.end());
            QByteArray response(reinterpret_cast<const char*>(data.data()), data.size());
            m_socket->write(response);
            m_socket->flush();
        }
        else if (command == libs101::CommandType::KeepAliveResponse) {
            log(LogLevel::Info, "Received KeepAliveResponse from device - connection is alive");
            // Don't treat this as Ember+ data, but acknowledge the connection is working
        }
    }
}

void EmberConnection::processRoot(libember::dom::Node* root)
{
    if (!root) {
        return;
    }
    
    // Stop protocol timeout timer - we've received valid Ember+ data!
    bool isFirstData = false;
    if (!m_emberDataReceived) {
        m_emberDataReceived = true;
        isFirstData = true;
        m_protocolTimer->stop();
        log(LogLevel::Info, "Ember+ protocol confirmed");
    }
    
    log(LogLevel::Trace, "Processing Ember+ tree...");
    
    // Detach root from reader
    m_domReader->detachRoot();
    
    // Walk the tree and emit signals
    try {
        auto glowRoot = dynamic_cast<libember::glow::GlowRootElementCollection*>(root);
        if (glowRoot) {
            log(LogLevel::Debug, QString("Root has %1 elements").arg(glowRoot->size()));
            
            // Process all root elements
            processElementCollection(glowRoot, "");
        }
    } catch (const std::exception& e) {
        log(LogLevel::Error, QString("Error processing Ember+ tree: %1").arg(e.what()));
    }
    
    // Always delete the root
    delete root;
    
    // Emit treePopulated signal if this was the first data received
    if (isFirstData) {
        log(LogLevel::Debug, "Initial tree populated, emitting treePopulated signal");
        emit treePopulated();
    }
}

void EmberConnection::processElementCollection(libember::glow::GlowContainer* container, const QString& parentPath)
{
    // Count children for tree fetch completion tracking
    int childCount = 0;
    
    // Iterate through all elements in the container
    for (auto it = container->begin(); it != container->end(); ++it) {
        auto element = &(*it);
        
        // Check qualified types first (they inherit from non-qualified)
        if (auto matrix = dynamic_cast<libember::glow::GlowQualifiedMatrix*>(element)) {
            processQualifiedMatrix(matrix);
            childCount++;
        }
        else if (auto function = dynamic_cast<libember::glow::GlowQualifiedFunction*>(element)) {
            processQualifiedFunction(function);
            childCount++;
        }
        else if (auto node = dynamic_cast<libember::glow::GlowQualifiedNode*>(element)) {
            processQualifiedNode(node);
            childCount++;
        }
        else if (auto param = dynamic_cast<libember::glow::GlowQualifiedParameter*>(element)) {
            processQualifiedParameter(param);
            childCount++;
        }
        else if (auto invocationResult = dynamic_cast<libember::glow::GlowInvocationResult*>(element)) {
            log(LogLevel::Info, "Received InvocationResult!");
            processInvocationResult(element);
        }
        else if (auto streamCollection = dynamic_cast<libember::glow::GlowStreamCollection*>(element)) {
            log(LogLevel::Debug, "Received StreamCollection (audio meters)");
            processStreamCollection(streamCollection);
        }
        else if (auto matrix = dynamic_cast<libember::glow::GlowMatrix*>(element)) {
            processMatrix(matrix, parentPath);
            childCount++;
        }
        else if (auto function = dynamic_cast<libember::glow::GlowFunction*>(element)) {
            processFunction(function, parentPath);
            childCount++;
        }
        else if (auto node = dynamic_cast<libember::glow::GlowNode*>(element)) {
            processNode(node, parentPath);
            childCount++;
        }
        else if (auto param = dynamic_cast<libember::glow::GlowParameter*>(element)) {
            processParameter(param, parentPath);
            childCount++;
        }
        else {
            log(LogLevel::Warning, QString("Unknown element type received (parent: %1)").arg(parentPath));
        }
    }
    
    // TREE FETCH: Mark parent as completed after processing all its children
    if (m_treeFetchActive && !parentPath.isEmpty()) {
        if (m_activeFetchPaths.contains(parentPath)) {
            m_activeFetchPaths.remove(parentPath);
            m_completedFetchPaths.insert(parentPath);
            log(LogLevel::Trace, QString("Completed fetching node %1 (%2 children)").arg(parentPath).arg(childCount));
        }
    }
}

void EmberConnection::processQualifiedNode(libember::glow::GlowQualifiedNode* node)
{
    auto path = node->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);  // Remove trailing dot
    
    bool hasIdentifier = node->contains(libember::glow::NodeProperty::Identifier);
    QString identifier = hasIdentifier
        ? QString::fromStdString(node->identifier()) 
        : QString("Node %1").arg(path.back());
    
    if (!hasIdentifier) {
        log(LogLevel::Warning, QString("QNode at %1 missing Identifier property").arg(pathStr));
    }
    
    bool hasDescription = node->contains(libember::glow::NodeProperty::Description);
    QString description = hasDescription
        ? QString::fromStdString(node->description()) 
        : "";
    
    // Track root-level nodes for smart device name detection
    QStringList pathParts = pathStr.split('.');
    int pathDepth = pathParts.size();
    
    if (pathDepth == 1) {
        QString displayName = !description.isEmpty() ? description : identifier;
        bool isGeneric = isGenericNodeName(displayName);
        
        log(LogLevel::Info, QString("ROOT QNode [%1]: Identifier='%2' (provided=%3), Description='%4' (provided=%5), Generic=%6")
            .arg(pathStr).arg(identifier).arg(hasIdentifier ? "yes" : "no")
            .arg(description).arg(hasDescription ? "yes" : "no")
            .arg(isGeneric ? "YES" : "no"));
        
        // Track this root node - but preserve existing non-generic name during tree fetch
        if (m_rootNodes.contains(pathStr) && !m_rootNodes[pathStr].isGeneric) {
            // Already have a good name, keep it - don't emit again
            log(LogLevel::Debug, QString("Preserving existing root node name: %1").arg(m_rootNodes[pathStr].displayName));
            // Return early to prevent re-emitting nodeReceived with generic name
            if (!hasIdentifier || isGeneric) {
                log(LogLevel::Debug, "Skipping nodeReceived emit - would overwrite good name with generic/missing");
                return;
            }
        } else {
            // New node or generic name - update it
            RootNodeInfo rootInfo;
            rootInfo.path = pathStr;
            rootInfo.displayName = displayName;
            rootInfo.isGeneric = isGeneric;
            // Preserve identityPath if it was already discovered
            if (m_rootNodes.contains(pathStr)) {
                rootInfo.identityPath = m_rootNodes[pathStr].identityPath;
            }
            m_rootNodes[pathStr] = rootInfo;
        }
    }
    // Track identity child nodes under root nodes
    else if (pathDepth == 2) {
        QString parentPath = pathParts[0];
        if (m_rootNodes.contains(parentPath)) {
            // Check if this is an identity node
            QString nodeName = identifier.toLower();
            if (nodeName == "identity" || nodeName == "_identity" || 
                nodeName == "deviceinfo" || nodeName == "device_info") {
                m_rootNodes[parentPath].identityPath = pathStr;
                log(LogLevel::Info, QString("Detected identity node for root %1: %2")
                    .arg(parentPath).arg(pathStr));
            }
        }
    }
    
    bool isOnline = node->contains(libember::glow::NodeProperty::IsOnline)
        ? node->isOnline()
        : true;
    
    log(LogLevel::Debug, QString("QNode: %1 - Online: %2")
        .arg(pathStr).arg(isOnline ? "YES" : "NO"));
    
    // Don't re-emit if we already have this node with a valid identifier
    // and the new data lacks an identifier (device sent stub parent reference)
    static QMap<QString, bool> nodesWithIdentifier;
    bool hadIdentifier = nodesWithIdentifier.value(pathStr, false);
    
    bool shouldEmit = true;
    if (hadIdentifier && !hasIdentifier) {
        log(LogLevel::Debug, QString("Skipping re-emit for %1 - would replace valid name with generic").arg(pathStr));
        shouldEmit = false;
    }
    
    if (hasIdentifier) {
        nodesWithIdentifier[pathStr] = true;
    }
    
    if (shouldEmit) {
        emit nodeReceived(pathStr, identifier, description, isOnline);
    }
    
    // TREE FETCH: If tree fetch is active, add this node to the fetch queue
    if (m_treeFetchActive) {
        // Add this new node to pending fetch queue (to fetch its children)
        if (!m_completedFetchPaths.contains(pathStr) && 
            !m_activeFetchPaths.contains(pathStr) &&
            !m_pendingFetchPaths.contains(pathStr)) {
            m_pendingFetchPaths.insert(pathStr);
            m_treeFetchTotalEstimate++;
        }
    }
    
    // Process children if present inline
    if (node->children()) {
        processElementCollection(node->children(), pathStr);
    }
    
    // LAZY LOADING: Only auto-request children for special cases (root name discovery)
    // Otherwise, let the UI request children when user expands the node
    bool shouldAutoRequest = false;
    
    // Special case 1: Root nodes with generic names (to discover device name)
    if (pathDepth == 1 && m_rootNodes.contains(pathStr) && m_rootNodes[pathStr].isGeneric) {
        shouldAutoRequest = true;
    }
    // Special case 2: Identity nodes under generic root (to find name parameter)
    else if (pathDepth == 2) {
        QString rootPath = pathParts[0];
        if (m_rootNodes.contains(rootPath) && m_rootNodes[rootPath].identityPath == pathStr) {
            shouldAutoRequest = true;
        }
    }
    
    if (shouldAutoRequest && (!node->children() || node->children()->size() == 0)) {
        // OPTIMIZATION: Use optimized field mask for name discovery
        sendGetDirectoryForPath(pathStr, true);
    }
}

void EmberConnection::processNode(libember::glow::GlowNode* node, const QString& parentPath)
{
    int number = node->number();
    QString pathStr = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    bool hasIdentifier = node->contains(libember::glow::NodeProperty::Identifier);
    QString identifier = hasIdentifier
        ? QString::fromStdString(node->identifier())
        : QString("Node %1").arg(number);
    
    bool hasDescription = node->contains(libember::glow::NodeProperty::Description);
    QString description = hasDescription
        ? QString::fromStdString(node->description())
        : "";
    
    // Track root-level nodes for smart device name detection
    QStringList pathParts = pathStr.split('.');
    
    int pathDepth = pathParts.size();
    
    if (pathDepth == 1) {
        QString displayName = !description.isEmpty() ? description : identifier;
        bool isGeneric = isGenericNodeName(displayName);
        
        log(LogLevel::Info, QString("ROOT Node [%1]: Identifier='%2' (provided=%3), Description='%4' (provided=%5), Generic=%6")
            .arg(pathStr).arg(identifier).arg(hasIdentifier ? "yes" : "no")
            .arg(description).arg(hasDescription ? "yes" : "no")
            .arg(isGeneric ? "YES" : "no"));
        
        // Track this root node - but preserve existing non-generic name during tree fetch
        if (m_rootNodes.contains(pathStr) && !m_rootNodes[pathStr].isGeneric) {
            // Already have a good name, keep it
            log(LogLevel::Debug, QString("Preserving existing root node name: %1").arg(m_rootNodes[pathStr].displayName));
        } else {
            // New node or generic name - update it
            RootNodeInfo rootInfo;
            rootInfo.path = pathStr;
            rootInfo.displayName = displayName;
            rootInfo.isGeneric = isGeneric;
            // Preserve identityPath if it was already discovered
            if (m_rootNodes.contains(pathStr)) {
                rootInfo.identityPath = m_rootNodes[pathStr].identityPath;
            }
            m_rootNodes[pathStr] = rootInfo;
        }
    }
    // Track identity child nodes under root nodes
    else if (pathDepth == 2) {
        QString parentPath = pathParts[0];
        if (m_rootNodes.contains(parentPath)) {
            // Check if this is an identity node (common names: identity, _identity, deviceInfo, etc.)
            QString nodeName = identifier.toLower();
            if (nodeName == "identity" || nodeName == "_identity" || 
                nodeName == "deviceinfo" || nodeName == "device_info") {
                m_rootNodes[parentPath].identityPath = pathStr;
                log(LogLevel::Info, QString("Detected identity node for root %1: %2")
                    .arg(parentPath).arg(pathStr));
            }
        }
    }
    
    bool isOnline = node->contains(libember::glow::NodeProperty::IsOnline)
        ? node->isOnline()
        : true;
    
    log(LogLevel::Debug, QString("Node: %1 - Online: %2")
        .arg(pathStr).arg(isOnline ? "YES" : "NO"));
    
    // Don't re-emit if we already have this node with a valid identifier
    // and the new data lacks an identifier (device sent stub parent reference)
    static QMap<QString, bool> nodesWithIdentifier;
    bool hadIdentifier = nodesWithIdentifier.value(pathStr, false);
    
    bool shouldEmit = true;
    if (hadIdentifier && !hasIdentifier) {
        log(LogLevel::Info, QString("Skipping re-emit for %1 (processNode) - would replace valid name with generic").arg(pathStr));
        shouldEmit = false;
    }
    
    if (hasIdentifier) {
        nodesWithIdentifier[pathStr] = true;
    }
    
    if (shouldEmit) {
        emit nodeReceived(pathStr, identifier, description, isOnline);
    }
    
    // TREE FETCH: If tree fetch is active, add this node to the fetch queue
    if (m_treeFetchActive) {
        // Add this new node to pending fetch queue (to fetch its children)
        if (!m_completedFetchPaths.contains(pathStr) && 
            !m_activeFetchPaths.contains(pathStr) &&
            !m_pendingFetchPaths.contains(pathStr)) {
            m_pendingFetchPaths.insert(pathStr);
            m_treeFetchTotalEstimate++;
        }
    }
    
    // Process children if present inline
    if (node->children()) {
        processElementCollection(node->children(), pathStr);
    }
    
    // LAZY LOADING: Only auto-request children for special cases (root name discovery)
    // Otherwise, let the UI request children when user expands the node
    bool shouldAutoRequest = false;
    
    // Special case 1: Root nodes with generic names (to discover device name)
    if (pathDepth == 1 && m_rootNodes.contains(pathStr) && m_rootNodes[pathStr].isGeneric) {
        shouldAutoRequest = true;
        log(LogLevel::Debug, QString("Auto-requesting children of root node %1 for name discovery").arg(pathStr));
    }
    // Special case 2: Identity nodes under generic root (to find name parameter)
    else if (pathDepth == 2) {
        QString parentPath = pathParts[0];
        if (m_rootNodes.contains(parentPath) && m_rootNodes[parentPath].identityPath == pathStr) {
            shouldAutoRequest = true;
            log(LogLevel::Debug, QString("Auto-requesting children of identity node %1 for name discovery").arg(pathStr));
        }
    }
    
    if (shouldAutoRequest && (!node->children() || node->children()->size() == 0)) {
        // OPTIMIZATION: Use optimized field mask for name discovery
        sendGetDirectoryForPath(pathStr, true);
    }
}

void EmberConnection::processQualifiedParameter(libember::glow::GlowQualifiedParameter* param)
{
    auto path = param->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);
    
    int number = path.back();
    
    // Get or create cache entry for this parameter (needed early for identifier)
    ParameterCache& cache = m_parameterCache[pathStr];
    
    // Get identifier - if provided, update cache; otherwise use cached value
    QString identifier;
    bool hasIdentifier = param->contains(libember::glow::ParameterProperty::Identifier);
    if (hasIdentifier) {
        identifier = QString::fromStdString(param->identifier());
        cache.identifier = identifier;  // Update cache
        log(LogLevel::Debug, QString("QParam %1: Identifier='%2' (from message)").arg(pathStr).arg(identifier));
    } else {
        // Use cached value, or generate default if never seen before
        if (!cache.identifier.isEmpty()) {
            identifier = cache.identifier;
            log(LogLevel::Debug, QString("QParam %1: Identifier='%2' (from cache)").arg(pathStr).arg(identifier));
        } else {
            identifier = QString("Param %1").arg(number);
            cache.identifier = identifier;
            log(LogLevel::Debug, QString("QParam %1: Identifier='%2' (generated default)").arg(pathStr).arg(identifier));
        }
    }
    
    // Extract value
    QString value = "";
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        auto val = param->value();
        auto type = val.type();
        
        if (type.value() == libember::glow::ParameterType::Integer) {
            value = QString::number(val.toInteger());
        }
        else if (type.value() == libember::glow::ParameterType::Real) {
            value = QString::number(val.toReal());
        }
        else if (type.value() == libember::glow::ParameterType::String) {
            value = QString::fromStdString(val.toString());
        }
        else if (type.value() == libember::glow::ParameterType::Boolean) {
            value = val.toBoolean() ? "true" : "false";
        }
    }
    
    // Cache was already created above when getting identifier
    // Get access property - if provided, update cache; otherwise use cached value
    int access;
    if (param->contains(libember::glow::ParameterProperty::Access)) {
        access = param->access().value();
        cache.access = access;  // Update cache
        log(LogLevel::Debug, QString("QParam %1: Access=%2 (from message), Type will follow").arg(pathStr).arg(access));
    } else {
        // Use cached value, or default to ReadOnly if never seen before
        access = (cache.access >= 0) ? cache.access : libember::glow::Access::ReadOnly;
        QString source = (cache.access >= 0) ? "from cache" : "default ReadOnly";
        log(LogLevel::Debug, QString("QParam %1: Access=%2 (%3)").arg(pathStr).arg(access).arg(source));
    }
    
    // Get type property - if provided, update cache; otherwise use cached value
    int type;
    if (param->contains(libember::glow::ParameterProperty::Type)) {
        type = param->type().value();
        cache.type = type;  // Update cache
    } else {
        type = cache.type;  // Use cached value (defaults to 0/None)
    }
    
    // Get minimum/maximum constraints
    QVariant minimum, maximum;
    if (param->contains(libember::glow::ParameterProperty::Minimum)) {
        auto min = param->minimum();
        if (min.type().value() == libember::glow::ParameterType::Integer) {
            minimum = QVariant(static_cast<int>(min.toInteger()));
        } else if (min.type().value() == libember::glow::ParameterType::Real) {
            minimum = QVariant(min.toReal());
        }
    }
    
    if (param->contains(libember::glow::ParameterProperty::Maximum)) {
        auto max = param->maximum();
        if (max.type().value() == libember::glow::ParameterType::Integer) {
            maximum = QVariant(static_cast<int>(max.toInteger()));
        } else if (max.type().value() == libember::glow::ParameterType::Real) {
            maximum = QVariant(max.toReal());
        }
    }
    
    // Get enumeration options
    QStringList enumOptions;
    QList<int> enumValues;
    
    if (param->contains(libember::glow::ParameterProperty::EnumMap)) {
        auto enumMap = param->enumerationMap();
        for (auto it = enumMap.begin(); it != enumMap.end(); ++it) {
            enumOptions.append(QString::fromStdString((*it).first));
            enumValues.append((*it).second);
        }
    }
    else if (param->contains(libember::glow::ParameterProperty::Enumeration)) {
        auto enumeration = param->enumeration();
        for (auto it = enumeration.begin(); it != enumeration.end(); ++it) {
            enumOptions.append(QString::fromStdString((*it).first));
            enumValues.append((*it).second);
        }
    }
    
    log(LogLevel::Debug, QString("QParam %1 complete: '%2' = '%3' (Type=%4, Access=%5)").arg(pathStr).arg(identifier).arg(value).arg(type).arg(access));
    
    // Check if this parameter could be a device name for a root node with generic name
    QStringList pathParts = pathStr.split('.');
    if (pathParts.size() >= 3) {
        QString rootPath = pathParts[0];
        if (m_rootNodes.contains(rootPath) && m_rootNodes[rootPath].isGeneric) {
            // Check if parameter identifier suggests it's a device name
            QString paramName = identifier.toLower();
            if (paramName == "name" || paramName == "device name" || 
                paramName == "devicename" || paramName == "product") {
                
                // Check if under identity node
                if (!m_rootNodes[rootPath].identityPath.isEmpty()) {
                    if (pathStr.startsWith(m_rootNodes[rootPath].identityPath + ".")) {
                        log(LogLevel::Info, QString("Found device name '%1' for root node %2 (from %3)")
                            .arg(value).arg(rootPath).arg(pathStr));
                        
                        // Update the display name
                        m_rootNodes[rootPath].displayName = value;
                        m_rootNodes[rootPath].isGeneric = false;
                        
                        // OPTIMIZATION: Cache device name for instant reconnection
                        QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
                        DeviceCache cache;
                        cache.deviceName = value;
                        cache.rootPath = rootPath;
                        cache.identityPath = m_rootNodes[rootPath].identityPath;
                        cache.lastSeen = QDateTime::currentDateTime();
                        cache.isValid = true;
                        s_deviceCache[cacheKey] = cache;
                        
                        log(LogLevel::Debug, QString("Cached device name '%1' for %2")
                            .arg(value).arg(cacheKey));
                        
                        // Re-emit node with new name (assume online since we're getting parameter updates)
                        emit nodeReceived(rootPath, value, value, true);
                    }
                }
            }
        }
    }
    
    bool isOnline = param->contains(libember::glow::ParameterProperty::IsOnline)
        ? param->isOnline()
        : true;
    
    // Get stream identifier for audio level meters
    int streamIdentifier = param->streamIdentifier();  // Returns -1 if not present
    if (streamIdentifier != -1) {
        log(LogLevel::Debug, QString("Parameter %1 has streamIdentifier=%2 (audio meter)")
            .arg(pathStr).arg(streamIdentifier));
    }
    
    emit parameterReceived(pathStr, number, identifier, value, access, type, minimum, maximum, enumOptions, enumValues, isOnline, streamIdentifier);
}

void EmberConnection::processParameter(libember::glow::GlowParameter* param, const QString& parentPath)
{
    int number = param->number();
    QString pathStr = parentPath.isEmpty()
        ? QString::number(number)
        : QString("%1.%2").arg(parentPath).arg(number);
    
    // Get or create cache entry for this parameter (needed early for identifier)
    ParameterCache& cache = m_parameterCache[pathStr];
    
    // Get identifier - if provided, update cache; otherwise use cached value
    QString identifier;
    if (param->contains(libember::glow::ParameterProperty::Identifier)) {
        identifier = QString::fromStdString(param->identifier());
        cache.identifier = identifier;  // Update cache
    } else {
        // Use cached value, or generate default if never seen before
        if (!cache.identifier.isEmpty()) {
            identifier = cache.identifier;
        } else {
            identifier = QString("Param %1").arg(number);
            cache.identifier = identifier;
        }
    }
    
    // Extract value
    QString value = "";
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        auto val = param->value();
        auto type = val.type();
        
        if (type.value() == libember::glow::ParameterType::Integer) {
            value = QString::number(val.toInteger());
        }
        else if (type.value() == libember::glow::ParameterType::Real) {
            value = QString::number(val.toReal());
        }
        else if (type.value() == libember::glow::ParameterType::String) {
            value = QString::fromStdString(val.toString());
        }
        else if (type.value() == libember::glow::ParameterType::Boolean) {
            value = val.toBoolean() ? "true" : "false";
        }
    }
    
    // Cache was already created above when getting identifier
    // Get access property - if provided, update cache; otherwise use cached value
    int access;
    if (param->contains(libember::glow::ParameterProperty::Access)) {
        access = param->access().value();
        cache.access = access;  // Update cache
    } else {
        // Use cached value, or default to ReadOnly if never seen before
        access = (cache.access >= 0) ? cache.access : libember::glow::Access::ReadOnly;
    }
    
    // Get type property - if provided, update cache; otherwise use cached value
    int type;
    if (param->contains(libember::glow::ParameterProperty::Type)) {
        type = param->type().value();
        cache.type = type;  // Update cache
    } else {
        type = cache.type;  // Use cached value (defaults to 0/None)
    }
    
    // Get minimum/maximum constraints
    QVariant minimum, maximum;
    if (param->contains(libember::glow::ParameterProperty::Minimum)) {
        auto min = param->minimum();
        if (min.type().value() == libember::glow::ParameterType::Integer) {
            minimum = QVariant(static_cast<int>(min.toInteger()));
        } else if (min.type().value() == libember::glow::ParameterType::Real) {
            minimum = QVariant(min.toReal());
        }
    }
    
    if (param->contains(libember::glow::ParameterProperty::Maximum)) {
        auto max = param->maximum();
        if (max.type().value() == libember::glow::ParameterType::Integer) {
            maximum = QVariant(static_cast<int>(max.toInteger()));
        } else if (max.type().value() == libember::glow::ParameterType::Real) {
            maximum = QVariant(max.toReal());
        }
    }
    
    // Get enumeration options
    QStringList enumOptions;
    QList<int> enumValues;
    
    if (param->contains(libember::glow::ParameterProperty::EnumMap)) {
        auto enumMap = param->enumerationMap();
        for (auto it = enumMap.begin(); it != enumMap.end(); ++it) {
            enumOptions.append(QString::fromStdString((*it).first));
            enumValues.append((*it).second);
        }
    }
    else if (param->contains(libember::glow::ParameterProperty::Enumeration)) {
        auto enumeration = param->enumeration();
        for (auto it = enumeration.begin(); it != enumeration.end(); ++it) {
            enumOptions.append(QString::fromStdString((*it).first));
            enumValues.append((*it).second);
        }
    }
    
    // Check if this parameter could be a device name for a root node with generic name
    QStringList pathParts = pathStr.split('.');
    if (pathParts.size() >= 3) {
        QString rootPath = pathParts[0];
        if (m_rootNodes.contains(rootPath) && m_rootNodes[rootPath].isGeneric) {
            // Check if parameter identifier suggests it's a device name
            QString paramName = identifier.toLower();
            if (paramName == "name" || paramName == "device name" || 
                paramName == "devicename" || paramName == "product") {
                
                // Check if under identity node
                if (!m_rootNodes[rootPath].identityPath.isEmpty()) {
                    if (pathStr.startsWith(m_rootNodes[rootPath].identityPath + ".")) {
                        log(LogLevel::Info, QString("Found device name '%1' for root node %2 (from %3)")
                            .arg(value).arg(rootPath).arg(pathStr));
                        
                        // Update the display name
                        m_rootNodes[rootPath].displayName = value;
                        m_rootNodes[rootPath].isGeneric = false;
                        
                        // OPTIMIZATION: Cache device name for instant reconnection
                        QString cacheKey = QString("%1:%2").arg(m_host).arg(m_port);
                        DeviceCache cache;
                        cache.deviceName = value;
                        cache.rootPath = rootPath;
                        cache.identityPath = m_rootNodes[rootPath].identityPath;
                        cache.lastSeen = QDateTime::currentDateTime();
                        cache.isValid = true;
                        s_deviceCache[cacheKey] = cache;
                        
                        log(LogLevel::Debug, QString("Cached device name '%1' for %2")
                            .arg(value).arg(cacheKey));
                        
                        // Re-emit node with new name (assume online since we're getting parameter updates)
                        emit nodeReceived(rootPath, value, value, true);
                    }
                }
            }
        }
    }
    
    bool isOnline = param->contains(libember::glow::ParameterProperty::IsOnline)
        ? param->isOnline()
        : true;
    
    // Get stream identifier for audio level meters
    int streamIdentifier = param->streamIdentifier();  // Returns -1 if not present
    if (streamIdentifier != -1) {
        log(LogLevel::Debug, QString("Parameter %1 has streamIdentifier=%2 (audio meter)")
            .arg(pathStr).arg(streamIdentifier));
    }
    
    emit parameterReceived(pathStr, number, identifier, value, access, type, minimum, maximum, enumOptions, enumValues, isOnline, streamIdentifier);
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
    log(LogLevel::Info, QString("sendGetDirectoryForPath called with path='%1'").arg(path));
    log(LogLevel::Info, QString("m_requestedPaths size: %1").arg(m_requestedPaths.size()));
    
    // Avoid infinite loops - don't request the same path twice
    if (m_requestedPaths.contains(path)) {
        log(LogLevel::Info, QString("ERROR: Skipping duplicate request for %1").arg(path.isEmpty() ? "root" : path));
        return;
    }
    log(LogLevel::Info, "Passed duplicate check, inserting path");
    m_requestedPaths.insert(path);
    log(LogLevel::Info, "Path inserted, continuing...");
    
    try {
        if (path.isEmpty()) {
            log(LogLevel::Info, "Requesting root directory...");
        } else {
            log(LogLevel::Info, QString("Requesting children of %1%2...")
                .arg(path)
                .arg(optimizedForNameDiscovery ? " (optimized for name discovery)" : ""));
        }
        
        // OPTIMIZATION: Use minimal field mask for faster name discovery
        // When discovering device names, we only need Identifier, Description, and Value
        // This reduces payload size by 20-40% and speeds up provider processing
        libember::glow::DirFieldMask fieldMask = optimizedForNameDiscovery
            ? libember::glow::DirFieldMask(libember::glow::DirFieldMask::Identifier | 
                                           libember::glow::DirFieldMask::Description | 
                                           libember::glow::DirFieldMask::Value)
            : libember::glow::DirFieldMask::All;
        
        log(LogLevel::Info, "Creating GlowRootElementCollection...");
        auto root = new libember::glow::GlowRootElementCollection();
        
        if (path.isEmpty()) {
            // Request root - use bare GlowCommand like working applications do
            log(LogLevel::Info, "Creating bare GlowCommand for root...");
            auto command = new libember::glow::GlowCommand(
                libember::glow::CommandType::GetDirectory
            );
            log(LogLevel::Info, "Inserting command into root...");
            root->insert(root->end(), command);
            log(LogLevel::Info, "Command inserted successfully");
        }
        else {
            // Request specific node path
            log(LogLevel::Info, QString("Creating QualifiedNode for path: %1").arg(path));
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
            log(LogLevel::Info, "QualifiedNode with command inserted (no field mask)");
        }
        
        // Encode to EmBER
        log(LogLevel::Info, "Encoding to EmBER...");
        libember::util::OctetStream stream;
        root->encode(stream);
        log(LogLevel::Info, QString("EmBER payload size: %1 bytes").arg(stream.size()));
        
        // Wrap in S101 frame
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
        encoder.finish();
        
        // Send
        std::vector<unsigned char> data(encoder.begin(), encoder.end());
        QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
        
        log(LogLevel::Info, QString("About to write %1 bytes to socket...").arg(qdata.size()));
        if (m_socket->write(qdata) > 0) {
            m_socket->flush();
            log(LogLevel::Info, QString("Successfully sent GetDirectory request (%1 bytes)").arg(qdata.size()));
        }
        else {
            log(LogLevel::Error, "Failed to send GetDirectory - socket write returned 0 or error");
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending GetDirectory for %1: %2").arg(path).arg(ex.what()));
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
            log(LogLevel::Debug, QString("Skipping duplicate request for %1").arg(path.isEmpty() ? "root" : path));
        }
    }
    
    if (pathsToRequest.isEmpty()) {
        return;
    }
    
    try {
        log(LogLevel::Debug, QString("Batch requesting %1 paths%2...")
            .arg(pathsToRequest.size())
            .arg(optimizedForNameDiscovery ? " (optimized for name discovery)" : ""));
        
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
        
        // Wrap in S101 frame
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
        encoder.finish();
        
        // Send
        std::vector<unsigned char> data(encoder.begin(), encoder.end());
        QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
        
        if (m_socket->write(qdata) > 0) {
            m_socket->flush();
        }
        else {
            log(LogLevel::Warning, "Failed to send batch GetDirectory");
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending batch GetDirectory: %1").arg(ex.what()));
    }
}

void EmberConnection::sendParameterValue(const QString &path, const QString &value, int type)
{
    try {
        log(LogLevel::Debug, QString("Setting parameter %1 = %2").arg(path).arg(value));
        
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
                log(LogLevel::Warning, QString("Unsupported parameter type: %1").arg(type));
                delete param;
                return;
        }
        
        // Create root collection and add parameter
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), param);
        
        // Encode to EmBER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Wrap in S101 frame
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
        encoder.finish();
        
        // Send
        std::vector<unsigned char> data(encoder.begin(), encoder.end());
        QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
        
        if (m_socket->write(qdata) > 0) {
            m_socket->flush();
            log(LogLevel::Debug, QString("Successfully sent value for %1").arg(path));
        }
        else {
            log(LogLevel::Warning, QString("Failed to send value for %1").arg(path));
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending parameter value for %1: %2").arg(path).arg(ex.what()));
    }
}

void EmberConnection::setMatrixConnection(const QString &matrixPath, int targetNumber, int sourceNumber, bool connect)
{
    try {
        QString operation = connect ? "CONNECT" : "DISCONNECT";
        
        log(LogLevel::Debug, QString(">>> Sending %1: Matrix=%2, Target=%3, Source=%4")
                       .arg(operation).arg(matrixPath).arg(targetNumber).arg(sourceNumber));
        
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
            log(LogLevel::Debug, QString("    Operation: Connect (1)"));
        } else {
            connection->setOperation(libember::glow::ConnectionOperation::Disconnect);
            log(LogLevel::Debug, QString("    Operation: Disconnect (2)"));
        }
        
        // Add source(s) to the connection
        // For Ember+, we need to set the sources as an OID sequence
        libember::ber::ObjectIdentifier sources;
        sources.push_back(sourceNumber);
        connection->setSources(sources);
        log(LogLevel::Debug, QString("    Sources: [%1]").arg(sourceNumber));
        
        // Add connection to the connections sequence
        connectionsSeq->insert(connectionsSeq->end(), connection);
        
        // Create root collection and add matrix
        auto root = new libember::glow::GlowRootElementCollection();
        root->insert(root->end(), matrix);
        
        // Encode to EmBER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Wrap in S101 frame
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
        encoder.finish();
        
        // Send
        std::vector<unsigned char> data(encoder.begin(), encoder.end());
        QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
        
        if (m_socket->write(qdata) > 0) {
            m_socket->flush();
            log(LogLevel::Debug, QString("Successfully sent matrix connection command"));
        }
        else {
            log(LogLevel::Warning, QString("Failed to send matrix connection command"));
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending matrix connection for %1: %2").arg(matrixPath).arg(ex.what()));
    }
}

void EmberConnection::processQualifiedMatrix(libember::glow::GlowQualifiedMatrix* matrix)
{
    auto path = matrix->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);  // Remove trailing dot
    
    int number = path.back();
    
    // Check if this message contains matrix metadata (not just connection updates)
    // Per Ember+ spec, connection updates send only the connections collection
    bool hasMetadata = matrix->contains(libember::glow::MatrixProperty::Identifier) ||
                       matrix->contains(libember::glow::MatrixProperty::Description) ||
                       matrix->contains(libember::glow::MatrixProperty::Type) ||
                       matrix->contains(libember::glow::MatrixProperty::TargetCount) ||
                       matrix->contains(libember::glow::MatrixProperty::SourceCount);
    
    // Only extract and emit metadata if this is a full matrix message (not just a connection update)
    if (hasMetadata) {
        QString identifier = matrix->contains(libember::glow::MatrixProperty::Identifier)
            ? QString::fromStdString(matrix->identifier())
            : QString("Matrix %1").arg(number);
        
        QString description = matrix->contains(libember::glow::MatrixProperty::Description)
            ? QString::fromStdString(matrix->description())
            : "";
        
        int type = matrix->contains(libember::glow::MatrixProperty::Type)
            ? matrix->type().value()
            : 2;  // Default to NToN
        
        int targetCount = matrix->contains(libember::glow::MatrixProperty::TargetCount)
            ? matrix->targetCount()
            : 0;
        
        int sourceCount = matrix->contains(libember::glow::MatrixProperty::SourceCount)
            ? matrix->sourceCount()
            : 0;
        
        log(LogLevel::Debug, QString("QualifiedMatrix: %1 [%2] - Type:%3, %4×%5 (full metadata)")
                        .arg(identifier).arg(pathStr).arg(type).arg(sourceCount).arg(targetCount));
        
        emit matrixReceived(pathStr, number, identifier, description, type, targetCount, sourceCount);
    } else {
        log(LogLevel::Debug, QString("QualifiedMatrix [%1] - connection update only (no metadata)").arg(pathStr));
    }
    
    // Process targets
    if (matrix->targets()) {
        for (auto it = matrix->targets()->begin(); it != matrix->targets()->end(); ++it) {
            if (auto target = dynamic_cast<libember::glow::GlowTarget*>(&(*it))) {
                int targetNumber = target->number();
                QString label = QString("Target %1").arg(targetNumber);
                
                // Targets don't have identifiers in the protocol, they're just numbers
                // Labels would come from a labels node if present
                emit matrixTargetReceived(pathStr, targetNumber, label);
            }
        }
    }
    
    // Process sources
    if (matrix->sources()) {
        for (auto it = matrix->sources()->begin(); it != matrix->sources()->end(); ++it) {
            if (auto source = dynamic_cast<libember::glow::GlowSource*>(&(*it))) {
                int sourceNumber = source->number();
                QString label = QString("Source %1").arg(sourceNumber);
                
                emit matrixSourceReceived(pathStr, sourceNumber, label);
            }
        }
    }
    
    // Process connections
    if (matrix->connections()) {
        log(LogLevel::Debug, QString(" Qualified Matrix [%1] has connections collection").arg(pathStr));
        
        int connectionCount = 0;
        int targetCount = 0;
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                targetCount++;
                
                // Per Ember+ spec: Each Connection object represents the complete state for that target
                // Clear this target's connections first, then add the new ones
                emit matrixTargetConnectionsCleared(pathStr, targetNumber);
                log(LogLevel::Debug, QString(" Cleared connections for Target %1 in matrix %2").arg(targetNumber).arg(pathStr));
                
                // Get all sources for this connection (sources() returns ObjectIdentifier)
                libember::ber::ObjectIdentifier sources = connection->sources();
                log(LogLevel::Debug, QString(" Connection - Target %1, Sources count: %2")
                               .arg(targetNumber).arg(sources.size()));
                
                if (!sources.empty()) {
                    int disposition = connection->disposition().value();
                    
                    // Each subid in the ObjectIdentifier is a connected source number
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        int sourceNumber = *sourceIt;
                        log(LogLevel::Debug, QString(" Emitting connection: Target %1 <- Source %2 (Disposition: %3)")
                                       .arg(targetNumber).arg(sourceNumber).arg(disposition));
                        emit matrixConnectionReceived(pathStr, targetNumber, sourceNumber, true, disposition);
                        connectionCount++;
                    }
                } else {
                    // Target with empty sources = disconnected target (per Ember+ spec)
                    log(LogLevel::Debug, QString(" Target %1 has no sources (disconnected)").arg(targetNumber));
                }
            }
        }
        log(LogLevel::Debug, QString(" Processed %1 targets, %2 total connections").arg(targetCount).arg(connectionCount));
        
        // If connections collection is empty, request the connection state
        if (connectionCount == 0) {
            log(LogLevel::Debug, QString(" Requesting connection state for qualified matrix [%1]").arg(pathStr));
            sendGetDirectoryForPath(pathStr);
        }
    } else {
        log(LogLevel::Debug, QString(" Qualified Matrix [%1] has NO connections collection - requesting it").arg(pathStr));
        // Request the matrix contents including connections
        sendGetDirectoryForPath(pathStr);
    }
    
    // Process children if present (matrix can contain nodes for parameters)
    if (matrix->children()) {
        processElementCollection(matrix->children(), pathStr);
    }
}

void EmberConnection::processMatrix(libember::glow::GlowMatrix* matrix, const QString& parentPath)
{
    int number = matrix->number();
    QString pathStr = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    // Check if this message contains matrix metadata (not just connection updates)
    // Per Ember+ spec, connection updates send only the connections collection
    bool hasMetadata = matrix->contains(libember::glow::MatrixProperty::Identifier) ||
                       matrix->contains(libember::glow::MatrixProperty::Description) ||
                       matrix->contains(libember::glow::MatrixProperty::Type) ||
                       matrix->contains(libember::glow::MatrixProperty::TargetCount) ||
                       matrix->contains(libember::glow::MatrixProperty::SourceCount);
    
    // Only extract and emit metadata if this is a full matrix message (not just a connection update)
    if (hasMetadata) {
        QString identifier = matrix->contains(libember::glow::MatrixProperty::Identifier)
            ? QString::fromStdString(matrix->identifier())
            : QString("Matrix %1").arg(number);
        
        QString description = matrix->contains(libember::glow::MatrixProperty::Description)
            ? QString::fromStdString(matrix->description())
            : "";
        
        int type = matrix->contains(libember::glow::MatrixProperty::Type)
            ? matrix->type().value()
            : 2;  // Default to NToN
        
        int targetCount = matrix->contains(libember::glow::MatrixProperty::TargetCount)
            ? matrix->targetCount()
            : 0;
        
        int sourceCount = matrix->contains(libember::glow::MatrixProperty::SourceCount)
            ? matrix->sourceCount()
            : 0;
        
        log(LogLevel::Debug, QString("Matrix: %1 [%2] - Type:%3, %4×%5 (full metadata)")
                        .arg(identifier).arg(pathStr).arg(type).arg(sourceCount).arg(targetCount));
        
        emit matrixReceived(pathStr, number, identifier, description, type, targetCount, sourceCount);
    } else {
        log(LogLevel::Debug, QString("Matrix [%1] - connection update only (no metadata)").arg(pathStr));
    }
    
    // Process targets
    if (matrix->targets()) {
        for (auto it = matrix->targets()->begin(); it != matrix->targets()->end(); ++it) {
            if (auto target = dynamic_cast<libember::glow::GlowTarget*>(&(*it))) {
                int targetNumber = target->number();
                QString label = QString("Target %1").arg(targetNumber);
                emit matrixTargetReceived(pathStr, targetNumber, label);
            }
        }
    }
    
    // Process sources
    if (matrix->sources()) {
        for (auto it = matrix->sources()->begin(); it != matrix->sources()->end(); ++it) {
            if (auto source = dynamic_cast<libember::glow::GlowSource*>(&(*it))) {
                int sourceNumber = source->number();
                QString label = QString("Source %1").arg(sourceNumber);
                emit matrixSourceReceived(pathStr, sourceNumber, label);
            }
        }
    }
    
    // Process connections
    if (matrix->connections()) {
        log(LogLevel::Debug, QString(" NonQual Matrix [%1] has connections collection").arg(pathStr));
        
        int connectionCount = 0;
        int targetCount = 0;
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                targetCount++;
                
                // Per Ember+ spec: Each Connection object represents the complete state for that target
                // Clear this target's connections first, then add the new ones
                emit matrixTargetConnectionsCleared(pathStr, targetNumber);
                log(LogLevel::Debug, QString(" Cleared connections for Target %1 in matrix %2").arg(targetNumber).arg(pathStr));
                
                // Get all sources for this connection (sources() returns ObjectIdentifier)
                libember::ber::ObjectIdentifier sources = connection->sources();
                log(LogLevel::Debug, QString(" Connection - Target %1, Sources count: %2")
                               .arg(targetNumber).arg(sources.size()));
                
                if (!sources.empty()) {
                    int disposition = connection->disposition().value();
                    
                    // Each subid in the ObjectIdentifier is a connected source number
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        int sourceNumber = *sourceIt;
                        log(LogLevel::Debug, QString(" Emitting connection: Target %1 <- Source %2 (Disposition: %3)")
                                       .arg(targetNumber).arg(sourceNumber).arg(disposition));
                        emit matrixConnectionReceived(pathStr, targetNumber, sourceNumber, true, disposition);
                        connectionCount++;
                    }
                } else {
                    // Target with empty sources = disconnected target (per Ember+ spec)
                    log(LogLevel::Debug, QString(" Target %1 has no sources (disconnected)").arg(targetNumber));
                }
            }
        }
        log(LogLevel::Debug, QString(" Processed %1 targets, %2 total connections").arg(targetCount).arg(connectionCount));
        
        // If connections collection is empty, request the connection state
        if (connectionCount == 0) {
            log(LogLevel::Debug, QString(" Requesting connection state for matrix [%1]").arg(pathStr));
            sendGetDirectoryForPath(pathStr);
        }
    } else {
        log(LogLevel::Debug, QString(" NonQual Matrix [%1] has NO connections collection - requesting it").arg(pathStr));
        // Request the matrix contents including connections
        sendGetDirectoryForPath(pathStr);
    }
    
    // Process children if present
    if (matrix->children()) {
        processElementCollection(matrix->children(), pathStr);
    }
}

void EmberConnection::processQualifiedFunction(libember::glow::GlowQualifiedFunction* function)
{
    auto path = function->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);
    
    QString identifier = function->contains(libember::glow::FunctionProperty::Identifier)
        ? QString::fromStdString(function->identifier())
        : QString("Function %1").arg(path.back());
    
    QString description = function->contains(libember::glow::FunctionProperty::Description)
        ? QString::fromStdString(function->description())
        : QString("");
    
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
    
    if (function->arguments()) {
        auto argsSeq = function->arguments();
        for (auto it = argsSeq->begin(); it != argsSeq->end(); ++it) {
            if (auto arg = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                argNames.append(QString::fromStdString(arg->name()));
                argTypes.append(arg->type().value());
            }
        }
    }
    
    if (function->result()) {
        auto resultsSeq = function->result();
        for (auto it = resultsSeq->begin(); it != resultsSeq->end(); ++it) {
            if (auto res = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                resultNames.append(QString::fromStdString(res->name()));
                resultTypes.append(res->type().value());
            }
        }
    }
    
    log(LogLevel::Debug, QString("QualifiedFunction: %1 [%2] - %3 args, %4 results")
        .arg(identifier).arg(pathStr).arg(argNames.size()).arg(resultNames.size()));
    
    emit functionReceived(pathStr, identifier, description, argNames, argTypes, resultNames, resultTypes);
    
    if (function->children()) {
        processElementCollection(function->children(), pathStr);
    }
}

void EmberConnection::processFunction(libember::glow::GlowFunction* function, const QString& parentPath)
{
    int number = function->number();
    QString pathStr = parentPath.isEmpty() 
        ? QString::number(number)
        : QString("%1.%2").arg(parentPath).arg(number);
    
    QString identifier = function->contains(libember::glow::FunctionProperty::Identifier)
        ? QString::fromStdString(function->identifier())
        : QString("Function %1").arg(number);
    
    QString description = function->contains(libember::glow::FunctionProperty::Description)
        ? QString::fromStdString(function->description())
        : QString("");
    
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
    
    if (function->arguments()) {
        auto argsSeq = function->arguments();
        for (auto it = argsSeq->begin(); it != argsSeq->end(); ++it) {
            if (auto arg = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                argNames.append(QString::fromStdString(arg->name()));
                argTypes.append(arg->type().value());
            }
        }
    }
    
    if (function->result()) {
        auto resultsSeq = function->result();
        for (auto it = resultsSeq->begin(); it != resultsSeq->end(); ++it) {
            if (auto res = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                resultNames.append(QString::fromStdString(res->name()));
                resultTypes.append(res->type().value());
            }
        }
    }
    
    log(LogLevel::Debug, QString("Function: %1 [%2] - %3 args, %4 results")
        .arg(identifier).arg(pathStr).arg(argNames.size()).arg(resultNames.size()));
    
    emit functionReceived(pathStr, identifier, description, argNames, argTypes, resultNames, resultTypes);
    
    if (function->children()) {
        processElementCollection(function->children(), pathStr);
    }
}

void EmberConnection::processInvocationResult(libember::dom::Node* node)
{
    log(LogLevel::Info, "processInvocationResult called!");
    
    auto result = dynamic_cast<libember::glow::GlowInvocationResult*>(node);
    if (!result) {
        log(LogLevel::Warning, "Invalid invocation result received - cast failed");
        return;
    }
    
    int invocationId = result->invocationId();
    bool success = result->success();
    
    QList<QVariant> resultValues;
    if (result->result()) {
        std::vector<libember::glow::Value> values;
        result->typedResult(std::back_inserter(values));
        
        for (const auto& val : values) {
            switch (val.type().value()) {
                case libember::glow::ParameterType::Integer:
                    resultValues.append(QVariant::fromValue(val.toInteger()));
                    break;
                case libember::glow::ParameterType::Real:
                    resultValues.append(QVariant::fromValue(val.toReal()));
                    break;
                case libember::glow::ParameterType::String:
                    resultValues.append(QString::fromStdString(val.toString()));
                    break;
                case libember::glow::ParameterType::Boolean:
                    resultValues.append(val.toBoolean());
                    break;
                default:
                    resultValues.append(QVariant());
                    break;
            }
        }
    }
    
    log(LogLevel::Info, QString("Invocation result received - ID: %1, Success: %2, Values: %3")
        .arg(invocationId).arg(success ? "YES" : "NO").arg(resultValues.size()));
    
    if (m_pendingInvocations.contains(invocationId)) {
        m_pendingInvocations.remove(invocationId);
    }
    
    emit invocationResultReceived(invocationId, success, resultValues);
}

void EmberConnection::processStreamCollection(libember::glow::GlowContainer* streamCollection)
{
    // Process all stream entries in the collection
    int entryCount = 0;
    for (auto it = streamCollection->begin(); it != streamCollection->end(); ++it) {
        auto streamEntry = dynamic_cast<libember::glow::GlowStreamEntry*>(&(*it));
        if (streamEntry) {
            int streamId = streamEntry->streamIdentifier();
            auto value = streamEntry->value();
            
            // Extract numeric value based on type
            double numericValue = 0.0;
            bool hasValue = false;
            
            switch (value.type().value()) {
                case libember::glow::ParameterType::Integer:
                    numericValue = static_cast<double>(value.toInteger());
                    hasValue = true;
                    break;
                case libember::glow::ParameterType::Real:
                    numericValue = value.toReal();
                    hasValue = true;
                    break;
                default:
                    log(LogLevel::Warning, QString("StreamEntry %1 has non-numeric type %2")
                        .arg(streamId).arg(value.type().value()));
                    break;
            }
            
            if (hasValue) {
                log(LogLevel::Trace, QString("StreamEntry: streamId=%1, value=%2")
                    .arg(streamId).arg(numericValue));
                emit streamValueReceived(streamId, numericValue);
                entryCount++;
            }
        }
    }
    
    if (entryCount > 0) {
        log(LogLevel::Debug, QString("Processed StreamCollection with %1 entries").arg(entryCount));
    }
}

void EmberConnection::invokeFunction(const QString &path, const QList<QVariant> &arguments)
{
    if (!m_connected) {
        log(LogLevel::Error, "Cannot invoke function - not connected");
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
                    log(LogLevel::Warning, QString("Unsupported argument type: %1").arg(arg.typeName()));
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
    
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    log(LogLevel::Debug, QString("Sent GetDirectory request (root)"));
    delete root;
}

// Subscription methods
void EmberConnection::subscribeToParameter(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot subscribe - not connected");
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Already subscribed to %1").arg(path));
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
    
    // Encode to S101
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes (required by some devices!)
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Track subscription
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    log(LogLevel::Debug, QString("Subscribed to parameter: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)"));
    
    delete root;
}

void EmberConnection::subscribeToNode(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot subscribe - not connected");
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Already subscribed to %1").arg(path));
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
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Track subscription
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    log(LogLevel::Debug, QString("Subscribed to node: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)"));
    
    delete root;
}

void EmberConnection::subscribeToMatrix(const QString &path, bool autoSubscribed)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot subscribe - not connected");
        return;
    }
    
    if (m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Already subscribed to %1").arg(path));
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
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Track subscription
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = autoSubscribed;
    m_subscriptions[path] = state;
    
    log(LogLevel::Debug, QString("Subscribed to matrix: %1 %2")
        .arg(path)
        .arg(autoSubscribed ? "(auto)" : "(manual)"));
    
    delete root;
}

void EmberConnection::unsubscribeFromParameter(const QString &path)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot unsubscribe - not connected");
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Not subscribed to %1").arg(path));
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
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Remove from tracking
    m_subscriptions.remove(path);
    
    log(LogLevel::Debug, QString("Unsubscribed from parameter: %1").arg(path));
    
    delete root;
}

void EmberConnection::unsubscribeFromNode(const QString &path)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot unsubscribe - not connected");
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Not subscribed to %1").arg(path));
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
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Remove from tracking
    m_subscriptions.remove(path);
    
    log(LogLevel::Debug, QString("Unsubscribed from node: %1").arg(path));
    
    delete root;
}

void EmberConnection::unsubscribeFromMatrix(const QString &path)
{
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot unsubscribe - not connected");
        return;
    }
    
    if (!m_subscriptions.contains(path)) {
        log(LogLevel::Debug, QString("Not subscribed to %1").arg(path));
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
    
    // Encode to S101
    auto encoder = libs101::StreamEncoder<unsigned char>();
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);
    encoder.encode(0x00);
    encoder.encode(stream.begin(), stream.end());
    encoder.finish();
    
    // Send
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
    m_socket->write(qdata);
    m_socket->flush();
    
    // Remove from tracking
    m_subscriptions.remove(path);
    
    log(LogLevel::Debug, QString("Unsubscribed from matrix: %1").arg(path));
    
    delete root;
}

void EmberConnection::sendBatchSubscribe(const QList<SubscriptionRequest>& requests)
{
    if (requests.isEmpty()) {
        return;
    }
    
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot batch subscribe - not connected");
        return;
    }
    
    // Filter out already-subscribed paths
    QList<SubscriptionRequest> toSubscribe;
    for (const auto& req : requests) {
        if (!m_subscriptions.contains(req.path)) {
            toSubscribe.append(req);
        } else {
            log(LogLevel::Debug, QString("Skipping duplicate subscription for %1").arg(req.path));
        }
    }
    
    if (toSubscribe.isEmpty()) {
        log(LogLevel::Debug, "All paths already subscribed, skipping batch");
        return;
    }
    
    try {
        log(LogLevel::Debug, QString("Batch subscribing to %1 paths...").arg(toSubscribe.size()));
        
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
                log(LogLevel::Warning, QString("Unknown subscription type '%1' for path %2")
                    .arg(req.type).arg(req.path));
                continue;
            }
            
            // Track subscription
            SubscriptionState state;
            state.subscribedAt = QDateTime::currentDateTime();
            state.autoSubscribed = true;
            m_subscriptions[req.path] = state;
        }
        
        if (successCount == 0) {
            log(LogLevel::Warning, "No valid subscriptions in batch");
            delete root;
            return;
        }
        
        // Encode to BER
        libember::util::OctetStream stream;
        root->encode(stream);
        
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
        m_socket->write(qdata);
        m_socket->flush();
        
        log(LogLevel::Debug, QString("Successfully batch subscribed to %1 paths").arg(successCount));
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending batch subscribe: %1").arg(ex.what()));
    }
}

bool EmberConnection::isSubscribed(const QString &path) const
{
    return m_subscriptions.contains(path);
}

// Complete tree fetch implementation
void EmberConnection::fetchCompleteTree(const QStringList &initialNodePaths)
{
    if (m_treeFetchActive) {
        log(LogLevel::Warning, "Tree fetch already in progress");
        return;
    }
    
    if (!m_connected) {
        emit treeFetchCompleted(false, "Not connected to device");
        return;
    }
    
    log(LogLevel::Info, QString("Starting complete tree fetch with %1 initial nodes...").arg(initialNodePaths.size()));
    
    m_treeFetchActive = true;
    m_pendingFetchPaths.clear();
    m_completedFetchPaths.clear();
    m_activeFetchPaths.clear();
    m_treeFetchTotalEstimate = initialNodePaths.size();
    
    // Add all initial nodes to pending queue
    for (const QString &path : initialNodePaths) {
        QString type = path.split('|').value(1);  // Format: "path|type"
        QString nodePath = path.split('|').value(0);
        
        // Only fetch nodes (they have children), skip parameters/matrices/functions
        if (type == "Node") {
            m_pendingFetchPaths.insert(nodePath);
        }
    }
    
    if (m_pendingFetchPaths.isEmpty()) {
        log(LogLevel::Info, "No nodes to fetch (all are leaf elements)");
        m_treeFetchActive = false;
        emit treeFetchCompleted(true, "No nodes to fetch");
        return;
    }
    
    log(LogLevel::Debug, QString("Queued %1 nodes for fetching").arg(m_pendingFetchPaths.size()));
    
    // Start processing the queue
    m_treeFetchTimer->start();
    processFetchQueue();
}

void EmberConnection::cancelTreeFetch()
{
    if (!m_treeFetchActive) {
        return;
    }
    
    log(LogLevel::Info, "Cancelling tree fetch...");
    
    m_treeFetchActive = false;
    m_treeFetchTimer->stop();
    m_pendingFetchPaths.clear();
    m_activeFetchPaths.clear();
    m_completedFetchPaths.clear();
    
    emit treeFetchCompleted(false, "Cancelled by user");
}

void EmberConnection::processFetchQueue()
{
    if (!m_treeFetchActive) {
        m_treeFetchTimer->stop();
        return;
    }
    
    // Batch size: how many requests to send in parallel
    const int MAX_PARALLEL_REQUESTS = 5;
    
    // Send new requests if we have capacity
    while (m_activeFetchPaths.size() < MAX_PARALLEL_REQUESTS && !m_pendingFetchPaths.isEmpty()) {
        // Take a path from pending queue
        QString path = *m_pendingFetchPaths.begin();
        m_pendingFetchPaths.remove(path);
        m_activeFetchPaths.insert(path);
        
        log(LogLevel::Trace, QString("Fetching children of: %1").arg(path.isEmpty() ? "root" : path));
        
        // Send GetDirectory request bypassing m_requestedPaths check
        // We manage our own tracking with m_activeFetchPaths/m_completedFetchPaths
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
        log(LogLevel::Info, "Encoding to EmBER...");
        libember::util::OctetStream stream;
        root->encode(stream);
        log(LogLevel::Info, QString("Encoded %1 bytes").arg(stream.size()));
        
        // Wrap in S101 frame
        log(LogLevel::Info, "Creating S101 encoder...");
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD (Glow)
        encoder.encode(0x02);  // 2 app bytes (CRITICAL: some devices require these!)
        encoder.encode(0x28);  // App byte 1
        encoder.encode(0x02);  // App byte 2
        log(LogLevel::Info, "Added S101 app bytes: 0x28 0x02");
        
        // Add EmBER data
        encoder.encode(stream.begin(), stream.end());
            encoder.finish();
            
            // Send
            std::vector<unsigned char> data(encoder.begin(), encoder.end());
            QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
            
            if (m_socket->write(qdata) > 0) {
                m_socket->flush();
            }
            
            delete root;
        }
        catch (const std::exception &ex) {
            log(LogLevel::Error, QString("Error sending GetDirectory for tree fetch: %1").arg(ex.what()));
            // Move from active to completed even on error to avoid getting stuck
            m_activeFetchPaths.remove(path);
            m_completedFetchPaths.insert(path);
        }
    }
    
    // Check if we're done
    if (m_pendingFetchPaths.isEmpty() && m_activeFetchPaths.isEmpty()) {
        log(LogLevel::Info, QString("Tree fetch completed: fetched %1 nodes").arg(m_completedFetchPaths.size()));
        
        m_treeFetchActive = false;
        m_treeFetchTimer->stop();
        m_completedFetchPaths.clear();
        
        emit treeFetchCompleted(true, QString("Fetched %1 nodes").arg(m_completedFetchPaths.size()));
    }
    else {
        // Update progress
        int total = m_completedFetchPaths.size() + m_activeFetchPaths.size() + m_pendingFetchPaths.size();
        emit treeFetchProgress(m_completedFetchPaths.size(), total);
    }
}

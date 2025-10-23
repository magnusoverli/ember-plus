/*
 * EmberConnection.cpp - Handles Ember+ protocol communication
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
#include <ember/glow/MatrixProperty.hpp>
#include <ember/glow/CommandType.hpp>
#include <ember/glow/DirFieldMask.hpp>
#include <ember/glow/NodeProperty.hpp>
#include <ember/glow/ParameterProperty.hpp>
#include <ember/glow/ParameterType.hpp>
#include <ember/glow/Access.hpp>
#include <ember/glow/ConnectionOperation.hpp>
#include <ember/util/OctetStream.hpp>
#include <s101/CommandType.hpp>
#include <s101/MessageType.hpp>
#include <s101/PackageFlag.hpp>
#include <s101/StreamEncoder.hpp>

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
    , m_socket(nullptr)
    , m_s101Decoder(nullptr)
    , m_domReader(nullptr)
    , m_host()
    , m_port(0)
    , m_connected(false)
    , m_logLevel(LogLevel::Info)  // Default to Info level
{
    m_socket = new QTcpSocket(this);
    
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
    
    // Initialize S101 decoder and DOM reader
    m_s101Decoder = new libs101::StreamDecoder<unsigned char>();
    m_domReader = new DomReader(this);
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
    m_host = host;
    m_port = port;
    
    log(LogLevel::Debug, QString("Connecting to %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void EmberConnection::disconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    
    // Clear request tracking for next connection
    m_requestedPaths.clear();
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
    m_connected = true;
    emit connected();
    log(LogLevel::Info, "Connected to provider");
    
    // Send GetDirectory to request root tree
    sendGetDirectory();
}

void EmberConnection::onSocketDisconnected()
{
    m_connected = false;
    m_s101Decoder->reset();
    m_domReader->reset();
    m_parameterCache.clear();  // Clear cached parameter metadata
    emit disconnected();
    log(LogLevel::Debug, "Disconnected from provider");
}

void EmberConnection::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = m_socket->errorString();
    log(LogLevel::Error, QString("Connection error: %1").arg(errorString));
}

void EmberConnection::onDataReceived()
{
    QByteArray data = m_socket->readAll();
    
    if (data.isEmpty()) {
        return;
    }
    
    log(LogLevel::Trace, QString("Received %1 bytes").arg(data.size()));
    
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
                log(LogLevel::Error, QString("Error parsing Glow: %1").arg(ex.what()));
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
    }
}

void EmberConnection::processRoot(libember::dom::Node* root)
{
    if (!root) {
        return;
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
        
        // Clean up
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error processing tree: %1").arg(ex.what()));
        delete root;
    }
}

void EmberConnection::processElementCollection(libember::glow::GlowContainer* container, const QString& parentPath)
{
    // Iterate through all elements in the container
    for (auto it = container->begin(); it != container->end(); ++it) {
        auto element = &(*it);
        
        // Check qualified types first (they inherit from non-qualified)
        if (auto matrix = dynamic_cast<libember::glow::GlowQualifiedMatrix*>(element)) {
            processQualifiedMatrix(matrix);
        }
        else if (auto node = dynamic_cast<libember::glow::GlowQualifiedNode*>(element)) {
            processQualifiedNode(node);
        }
        else if (auto param = dynamic_cast<libember::glow::GlowQualifiedParameter*>(element)) {
            processQualifiedParameter(param);
        }
        else if (auto matrix = dynamic_cast<libember::glow::GlowMatrix*>(element)) {
            processMatrix(matrix, parentPath);
        }
        else if (auto node = dynamic_cast<libember::glow::GlowNode*>(element)) {
            processNode(node, parentPath);
        }
        else if (auto param = dynamic_cast<libember::glow::GlowParameter*>(element)) {
            processParameter(param, parentPath);
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
    
    QString description = node->contains(libember::glow::NodeProperty::Description)
        ? QString::fromStdString(node->description()) 
        : "";
    
    emit nodeReceived(pathStr, identifier, description);
    
    // Process children if present inline
    if (node->children()) {
        processElementCollection(node->children(), pathStr);
    }
    
    // Request children for this node if not already provided
    if (!node->children() || node->children()->size() == 0) {
        sendGetDirectoryForPath(pathStr);
    }
}

void EmberConnection::processNode(libember::glow::GlowNode* node, const QString& parentPath)
{
    int number = node->number();
    QString pathStr = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    QString identifier = node->contains(libember::glow::NodeProperty::Identifier)
        ? QString::fromStdString(node->identifier())
        : QString("Node %1").arg(number);
    
    QString description = node->contains(libember::glow::NodeProperty::Description)
        ? QString::fromStdString(node->description())
        : "";
    
    emit nodeReceived(pathStr, identifier, description);
    
    // Process children if present inline
    if (node->children()) {
        processElementCollection(node->children(), pathStr);
    }
    
    // Request children for this node if not already provided
    if (!node->children() || node->children()->size() == 0) {
        sendGetDirectoryForPath(pathStr);
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
        // Use cached value, or default to ReadWrite if never seen before
        access = (cache.access >= 0) ? cache.access : libember::glow::Access::ReadWrite;
        QString source = (cache.access >= 0) ? "from cache" : "default ReadWrite";
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
    emit parameterReceived(pathStr, number, identifier, value, access, type, minimum, maximum, enumOptions, enumValues);
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
        // Use cached value, or default to ReadWrite if never seen before
        access = (cache.access >= 0) ? cache.access : libember::glow::Access::ReadWrite;
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
    
    emit parameterReceived(pathStr, number, identifier, value, access, type, minimum, maximum, enumOptions, enumValues);
}

void EmberConnection::sendGetDirectory()
{
    sendGetDirectoryForPath("");
}

void EmberConnection::sendGetDirectoryForPath(const QString& path)
{
    // Avoid infinite loops - don't request the same path twice
    if (m_requestedPaths.contains(path)) {
        log(LogLevel::Debug, QString(" Skipping duplicate request for %1").arg(path.isEmpty() ? "root" : path));
        return;
    }
    m_requestedPaths.insert(path);
    
    try {
        if (path.isEmpty()) {
            log(LogLevel::Debug, "Requesting root directory...");
        } else {
            log(LogLevel::Debug, QString("Requesting children of %1...").arg(path));
        }
        
        // Create GetDirectory command with DirFieldMask requesting all fields
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
            auto command = new libember::glow::GlowCommand(
                node,
                libember::glow::CommandType::GetDirectory,
                libember::glow::DirFieldMask::All
            );
            root->insert(root->end(), node);
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
        encoder.encode(0x00);  // No app bytes
        
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
            log(LogLevel::Warning, "Failed to send GetDirectory");
        }
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending GetDirectory for %1: %2").arg(path).arg(ex.what()));
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
        encoder.encode(0x00);  // No app bytes
        
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
        encoder.encode(0x00);  // No app bytes
        
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

void EmberConnection::requestMatrixConnections(const QString &matrixPath)
{
    // Remove from requested paths to allow re-requesting after a change
    m_requestedPaths.remove(matrixPath);
    
    // Request the connection state by asking for the matrix directory
    sendGetDirectoryForPath(matrixPath);
    log(LogLevel::Debug, QString("Requesting updated connection state for matrix %1").arg(matrixPath));
}

void EmberConnection::processQualifiedMatrix(libember::glow::GlowQualifiedMatrix* matrix)
{
    auto path = matrix->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);  // Remove trailing dot
    
    // Check if this is a full matrix refresh (has connections)
    bool isFullRefresh = (matrix->connections() != nullptr);
    
    int number = path.back();
    
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
    
    log(LogLevel::Debug, QString("Matrix: %1 [%2] - Type:%3, %4×%5")
                    .arg(identifier).arg(pathStr).arg(type).arg(sourceCount).arg(targetCount));
    
    emit matrixReceived(pathStr, number, identifier, description, type, targetCount, sourceCount);
    
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
        
        // Clear existing connections in the UI before sending new ones
        emit matrixConnectionsCleared(pathStr);
        log(LogLevel::Debug, QString(" Cleared all existing connections for matrix %1").arg(pathStr));
        
        int connectionCount = 0;
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                
                // Get all sources for this connection (sources() returns ObjectIdentifier)
                libember::ber::ObjectIdentifier sources = connection->sources();
                log(LogLevel::Debug, QString(" Connection - Target %1, Sources count: %2")
                               .arg(targetNumber).arg(sources.size()));
                
                if (!sources.empty()) {
                    // Each subid in the ObjectIdentifier is a connected source number
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        int sourceNumber = *sourceIt;
                        log(LogLevel::Debug, QString(" Emitting connection: Target %1 <- Source %2")
                                       .arg(targetNumber).arg(sourceNumber));
                        emit matrixConnectionReceived(pathStr, targetNumber, sourceNumber, true);
                        connectionCount++;
                    }
                }
            }
        }
        log(LogLevel::Debug, QString(" Total connections emitted: %1").arg(connectionCount));
        
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
    
    log(LogLevel::Debug, QString("Matrix: %1 [%2] - Type:%3, %4×%5")
                    .arg(identifier).arg(pathStr).arg(type).arg(sourceCount).arg(targetCount));
    
    emit matrixReceived(pathStr, number, identifier, description, type, targetCount, sourceCount);
    
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
        
        // Clear existing connections in the UI before sending new ones
        emit matrixConnectionsCleared(pathStr);
        log(LogLevel::Debug, QString(" Cleared all existing connections for matrix %1").arg(pathStr));
        
        int connectionCount = 0;
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                
                // Get all sources for this connection (sources() returns ObjectIdentifier)
                libember::ber::ObjectIdentifier sources = connection->sources();
                log(LogLevel::Debug, QString(" Connection - Target %1, Sources count: %2")
                               .arg(targetNumber).arg(sources.size()));
                
                if (!sources.empty()) {
                    // Each subid in the ObjectIdentifier is a connected source number
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        int sourceNumber = *sourceIt;
                        log(LogLevel::Debug, QString(" Emitting connection: Target %1 <- Source %2")
                                       .arg(targetNumber).arg(sourceNumber));
                        emit matrixConnectionReceived(pathStr, targetNumber, sourceNumber, true);
                        connectionCount++;
                    }
                }
            }
        }
        log(LogLevel::Debug, QString(" Total connections emitted: %1").arg(connectionCount));
        
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

/*
 * EmberConnection.cpp - Handles Ember+ protocol communication
 */

#include "EmberConnection.h"
#include <ember/Ember.hpp>
#include <s101/StreamDecoder.hpp>
#include <s101/StreamEncoder.hpp>
#include <s101/CommandType.hpp>
#include <s101/MessageType.hpp>
#include <s101/PackageFlag.hpp>
#include <ember/glow/Glow.hpp>

EmberConnection::EmberConnection(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_s101Decoder(nullptr)
    , m_host()
    , m_port(0)
    , m_connected(false)
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
    
    // Initialize S101 decoder
    m_s101Decoder = new libs101::StreamDecoder<unsigned char>();
}

EmberConnection::~EmberConnection()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    
    delete m_s101Decoder;
}

void EmberConnection::connectToHost(const QString &host, int port)
{
    m_host = host;
    m_port = port;
    
    emit logMessage(QString("Connecting to %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void EmberConnection::disconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool EmberConnection::isConnected() const
{
    return m_connected;
}

void EmberConnection::onSocketConnected()
{
    m_connected = true;
    emit connected();
    emit logMessage("Socket connected");
    
    // Send initial GetDirectory command to request root
    sendGetDirectory();
}

void EmberConnection::onSocketDisconnected()
{
    m_connected = false;
    m_receiveBuffer.clear();
    m_s101Decoder->reset();
    emit disconnected();
    emit logMessage("Socket disconnected");
}

void EmberConnection::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = m_socket->errorString();
    emit logMessage(QString("Socket error: %1").arg(errorString));
}

void EmberConnection::onDataReceived()
{
    QByteArray data = m_socket->readAll();
    
    if (data.isEmpty()) {
        return;
    }
    
    emit logMessage(QString("Received %1 bytes").arg(data.size()));
    
    // Append to buffer
    m_receiveBuffer.append(data);
    
    // Process with S101 decoder
    processEmberData(m_receiveBuffer);
    
    // Clear buffer after processing
    m_receiveBuffer.clear();
}

void EmberConnection::processEmberData(const QByteArray &data)
{
    try {
        // Feed data into S101 decoder using callback
        const unsigned char *bytes = reinterpret_cast<const unsigned char*>(data.constData());
        
        m_s101Decoder->read(bytes, bytes + data.size(),
            // Callback lambda
            [](libs101::StreamDecoder<unsigned char>::const_iterator first,
               libs101::StreamDecoder<unsigned char>::const_iterator last,
               EmberConnection* self)
            {
                // Calculate message size
                auto messageSize = std::distance(first, last);
                emit self->logMessage(QString("Decoded S101 frame, message size: %1 bytes").arg(messageSize));
                
                // Here we would parse the Glow data
                // For now, just log that we received something
                if (messageSize > 0) {
                    // Skip the S101 header bytes and get to the EmBER data
                    // Format: Slot(1) | Message(1) | Command(1) | Version(1) | Flags(1) | DTD(1) | AppBytes(1) | ... | EmBER data
                    if (messageSize > 7) {
                        first++;  // Skip slot
                        auto message = *first++;
                        
                        if (message == libs101::MessageType::EmBER) {
                            auto command = *first++;
                            first++;  // Skip version
                            first++;  // Skip flags
                            first++;  // Skip DTD
                            
                            auto appBytes = *first++;
                            while (appBytes-- > 0 && first != last) {
                                ++first;
                            }
                            
                            // Now first points to EmBER data
                            if (first != last) {
                                emit self->logMessage("Received Ember+ message (detailed parsing not yet implemented)");
                                
                                // Simulate receiving a node for demonstration
                                emit self->treeItemReceived("1", "Root", "Root node of provider");
                            }
                        }
                    }
                }
                
                return true;  // Continue processing
            },
            this  // Pass this as state
        );
        
    } catch (const std::exception &ex) {
        emit logMessage(QString("Error processing Ember data: %1").arg(ex.what()));
    }
}

void EmberConnection::sendGetDirectory()
{
    try {
        emit logMessage("Sending GetDirectory command...");
        
        // For now, just send a simple S101 frame to test connectivity
        // A full implementation would encode proper Glow GetDirectory commands
        
        // Simplified: Just log that we would send it
        // Full implementation requires proper Glow message encoding
        emit logMessage("GetDirectory sending not yet fully implemented");
        
    } catch (const std::exception &ex) {
        emit logMessage(QString("Error sending GetDirectory: %1").arg(ex.what()));
    }
}

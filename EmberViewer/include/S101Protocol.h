/*
    EmberViewer - S101 Protocol Handler
    
    Handles S101 frame encoding and decoding for Ember+ communication.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef S101PROTOCOL_H
#define S101PROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <s101/StreamDecoder.hpp>
#include <s101/StreamEncoder.hpp>
#include <ember/util/OctetStream.hpp>

class S101Protocol : public QObject
{
    Q_OBJECT

public:
    explicit S101Protocol(QObject *parent = nullptr);
    ~S101Protocol();

    // Decode incoming S101 frames from raw data
    void feedData(const QByteArray& data);
    
    // Encode Ember+ data into S101 frame
    QByteArray encodeEmberData(const libember::util::OctetStream& emberData);
    
    // Encode keep-alive response frame
    QByteArray encodeKeepAliveResponse();

signals:
    // Emitted when a complete S101 message is decoded
    void messageReceived(const QByteArray& emberData);
    
    // Emitted on protocol errors
    void protocolError(const QString& error);

private:
    libs101::StreamDecoder<unsigned char> *m_decoder;
};

#endif // S101PROTOCOL_H

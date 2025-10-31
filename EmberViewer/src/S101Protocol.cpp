/*
    EmberViewer - S101 Protocol Handler Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "S101Protocol.h"
#include <s101/MessageType.hpp>
#include <s101/CommandType.hpp>
#include <s101/PackageFlag.hpp>

S101Protocol::S101Protocol(QObject *parent)
    : QObject(parent)
    , m_decoder(new libs101::StreamDecoder<unsigned char>())
{
}

S101Protocol::~S101Protocol()
{
    delete m_decoder;
}

void S101Protocol::feedData(const QByteArray& data)
{
    // Feed raw bytes to S101 decoder with callback
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    
    m_decoder->read(bytes, bytes + data.size(),
        [](libs101::StreamDecoder<unsigned char>::const_iterator first,
           libs101::StreamDecoder<unsigned char>::const_iterator last,
           S101Protocol* self)
        {
            if (first == last)
                return true;
            
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
                    
                    // Extract Ember+ data
                    QByteArray emberData(reinterpret_cast<const char*>(&(*first)), 
                                        std::distance(first, last));
                    
                    emit self->messageReceived(emberData);
                }
                else if (command == libs101::CommandType::KeepAliveRequest) {
                    // Send keep-alive response (handled by EmberConnection)
                    // For now, just acknowledge
                }
            }
            
            return true;
        },
        this
    );
}

QByteArray S101Protocol::encodeEmberData(const libember::util::OctetStream& emberData)
{
    auto encoder = libs101::StreamEncoder<unsigned char>();
    
    // S101 frame with Ember+ payload
    encoder.encode(0x00);  // Slot
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);  // Version
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);  // DTD (Glow)
    encoder.encode(0x02);  // 2 app bytes (required by some devices)
    encoder.encode(0x28);  // App byte 1
    encoder.encode(0x02);  // App byte 2
    
    // Add Ember+ data
    encoder.encode(emberData.begin(), emberData.end());
    encoder.finish();
    
    // Convert to QByteArray
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    return QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
}

QByteArray S101Protocol::encodeKeepAliveResponse()
{
    auto encoder = libs101::StreamEncoder<unsigned char>();
    
    // S101 keep-alive response frame
    encoder.encode(0x00);  // Slot
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::KeepAliveResponse);
    encoder.encode(0x01);  // Version
    encoder.finish();
    
    // Convert to QByteArray
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    return QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
}

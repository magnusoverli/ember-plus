







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
    
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    
    m_decoder->read(bytes, bytes + data.size(),
        [](libs101::StreamDecoder<unsigned char>::const_iterator first,
           libs101::StreamDecoder<unsigned char>::const_iterator last,
           S101Protocol* self)
        {
            if (first == last)
                return true;
            
            
            
            auto remaining = std::distance(first, last);
            if (remaining < 4) {
                qWarning() << "[S101] Frame too short:" << remaining << "bytes (need at least 4)";
                return true;  
            }
            
            first++;  
            auto message = *first++;
            
            if (message == libs101::MessageType::EmBER) {
                auto command = *first++;
                first++;  
                
                if (command == libs101::CommandType::EmBER) {
                    
                    if (std::distance(first, last) < 2) {
                        qWarning() << "[S101] EmBER command frame too short";
                        return true;
                    }
                    
                    auto flags = libs101::PackageFlag(*first++);
                    first++;  
                    
                    if (first == last) {
                        qWarning() << "[S101] Unexpected end of frame at appBytes field";
                        return true;
                    }
                    
                    
                    auto appBytes = *first++;
                    unsigned char appBytesCount = appBytes;
                    
                    while (appBytes > 0) {
                        if (first == last) {
                            qWarning() << "[S101] Frame truncated during app bytes skip (expected" 
                                       << appBytesCount << "bytes, got" << (appBytesCount - appBytes) << ")";
                            return true;
                        }
                        ++first;
                        --appBytes;
                    }
                    
                    
                    if (first != last) {
                        QByteArray emberData(reinterpret_cast<const char*>(&(*first)), 
                                            std::distance(first, last));
                        
                        emit self->messageReceived(emberData);
                    } else {
                        qWarning() << "[S101] No Ember+ payload in frame";
                    }
                }
                else if (command == libs101::CommandType::KeepAliveRequest) {
                    
                    qDebug() << "[S101] KeepAlive REQUEST received from device";
                    emit self->keepAliveReceived();
                }
                else if (command == libs101::CommandType::KeepAliveResponse) {
                    
                    qDebug() << "[S101] KeepAlive RESPONSE received (unexpected)";
                }
                else {
                    
                    qDebug() << "[S101] Unknown command type:" << static_cast<int>(command);
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
    
    
    encoder.encode(0x00);  
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::EmBER);
    encoder.encode(0x01);  
    encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
    encoder.encode(0x01);  
    encoder.encode(0x02);  
    encoder.encode(0x28);  
    encoder.encode(0x02);  
    
    
    encoder.encode(emberData.begin(), emberData.end());
    encoder.finish();
    
    
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    return QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
}

QByteArray S101Protocol::encodeKeepAliveResponse()
{
    auto encoder = libs101::StreamEncoder<unsigned char>();
    
    
    encoder.encode(0x00);  
    encoder.encode(libs101::MessageType::EmBER);
    encoder.encode(libs101::CommandType::KeepAliveResponse);
    encoder.encode(0x01);  
    encoder.finish();
    
    
    std::vector<unsigned char> data(encoder.begin(), encoder.end());
    return QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
}

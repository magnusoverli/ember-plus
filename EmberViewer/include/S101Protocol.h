









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

    
    void feedData(const QByteArray& data);
    
    
    QByteArray encodeEmberData(const libember::util::OctetStream& emberData);
    
    
    QByteArray encodeKeepAliveResponse();

signals:
    
    void messageReceived(const QByteArray& emberData);
    
    
    void keepAliveReceived();
    
    
    void protocolError(const QString& error);

private:
    libs101::StreamDecoder<unsigned char> *m_decoder;
};

#endif 

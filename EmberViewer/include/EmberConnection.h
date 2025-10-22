/*
 * EmberConnection.h - Handles Ember+ protocol communication
 */

#ifndef EMBERCONNECTION_H
#define EMBERCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>

// Forward declarations for libember types
namespace libember { namespace glow {
    class GlowReader;
}}

namespace libs101 {
    template<typename T> class StreamDecoder;
}

class EmberConnection : public QObject
{
    Q_OBJECT

public:
    explicit EmberConnection(QObject *parent = nullptr);
    ~EmberConnection();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void logMessage(const QString &message);
    void treeItemReceived(const QString &path, const QString &identifier, const QString &description);
    void parameterReceived(const QString &path, const QString &identifier, const QString &value);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onDataReceived();

private:
    void processEmberData(const QByteArray &data);
    void sendGetDirectory();

    QTcpSocket *m_socket;
    QByteArray m_receiveBuffer;
    
    // S101 decoder
    libs101::StreamDecoder<unsigned char> *m_s101Decoder;
    
    // Connection state
    QString m_host;
    int m_port;
    bool m_connected;
};

#endif // EMBERCONNECTION_H


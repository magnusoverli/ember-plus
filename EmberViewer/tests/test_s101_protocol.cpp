#include <QtTest/QtTest>
#include "../include/S101Protocol.h"
#include <ember/util/OctetStream.hpp>

class TestS101Protocol : public QObject
{
    Q_OBJECT

private slots:
    void testEncodeKeepAliveResponse()
    {
        S101Protocol protocol;
        QByteArray frame = protocol.encodeKeepAliveResponse();
        
        
        QVERIFY(!frame.isEmpty());
        
        
        
        QVERIFY(frame.size() >= 4);
        
        
        
        QVERIFY(frame.size() < 100);
    }

    void testEncodeEmberData()
    {
        S101Protocol protocol;
        
        
        libember::util::OctetStream emberData;
        emberData.append(0x01);
        emberData.append(0x02);
        emberData.append(0x03);
        emberData.append(0x04);
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        
        QVERIFY(!frame.isEmpty());
        
        
        
        
        
        QVERIFY(frame.size() >= 13);
    }

    void testEncodeEmberDataEmpty()
    {
        S101Protocol protocol;
        
        
        libember::util::OctetStream emberData;
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        
        QVERIFY(!frame.isEmpty());
        
        
        QVERIFY(frame.size() >= 9);
    }

    void testEncodeEmberDataLarge()
    {
        S101Protocol protocol;
        
        
        libember::util::OctetStream emberData;
        for (int i = 0; i < 1000; ++i) {
            emberData.append(static_cast<unsigned char>(i & 0xFF));
        }
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        
        QVERIFY(!frame.isEmpty());
        QVERIFY(frame.size() >= 1000); 
        QVERIFY(frame.size() < 2000);  
    }

    void testMultipleEncodings()
    {
        S101Protocol protocol;
        
        
        libember::util::OctetStream data1;
        data1.append(0xAA);
        
        libember::util::OctetStream data2;
        data2.append(0xBB);
        
        QByteArray frame1a = protocol.encodeEmberData(data1);
        QByteArray frame2 = protocol.encodeEmberData(data2);
        QByteArray frame1b = protocol.encodeEmberData(data1);
        
        
        QCOMPARE(frame1a, frame1b);
        
        
        QVERIFY(frame1a != frame2);
        
        
        QVERIFY(qAbs(frame1a.size() - frame2.size()) <= 5); 
    }

    void testKeepAliveConsistency()
    {
        S101Protocol protocol;
        
        
        QByteArray frame1 = protocol.encodeKeepAliveResponse();
        QByteArray frame2 = protocol.encodeKeepAliveResponse();
        QByteArray frame3 = protocol.encodeKeepAliveResponse();
        
        QCOMPARE(frame1, frame2);
        QCOMPARE(frame2, frame3);
    }

    void testEncodingDifferentiation()
    {
        S101Protocol protocol;
        
        
        QByteArray keepAlive = protocol.encodeKeepAliveResponse();
        
        libember::util::OctetStream emberData;
        QByteArray emberFrame = protocol.encodeEmberData(emberData);
        
        
        QVERIFY(keepAlive != emberFrame);
    }

    void testAppBytesPresence()
    {
        S101Protocol protocol;
        
        
        libember::util::OctetStream emberData;
        emberData.append(0xFF);
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        
        
        
        
        QVERIFY(frame.size() >= 10);
        
        
        
    }
};

QTEST_MAIN(TestS101Protocol)
#include "test_s101_protocol.moc"

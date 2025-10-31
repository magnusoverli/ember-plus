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
        
        // Verify frame is not empty
        QVERIFY(!frame.isEmpty());
        
        // Verify frame has expected minimum size
        // S101 keep-alive: Slot(1) + Message(1) + Command(1) + Version(1) + framing overhead
        QVERIFY(frame.size() >= 4);
        
        // The actual frame will be larger due to S101 framing (escaping, CRC, etc.)
        // but should be reasonable size
        QVERIFY(frame.size() < 100);
    }

    void testEncodeEmberData()
    {
        S101Protocol protocol;
        
        // Create some test Ember data
        libember::util::OctetStream emberData;
        emberData.append(0x01);
        emberData.append(0x02);
        emberData.append(0x03);
        emberData.append(0x04);
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        // Verify frame is not empty
        QVERIFY(!frame.isEmpty());
        
        // Frame should contain header + payload + framing
        // Header: Slot(1) + Message(1) + Command(1) + Version(1) + Flags(1) + DTD(1) + AppBytes(3) = 9 bytes
        // Payload: 4 bytes
        // Plus S101 framing overhead (BOF, EOF, CRC, escaping)
        QVERIFY(frame.size() >= 13);
    }

    void testEncodeEmberDataEmpty()
    {
        S101Protocol protocol;
        
        // Create empty Ember data
        libember::util::OctetStream emberData;
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        // Should still encode a valid frame (just without payload)
        QVERIFY(!frame.isEmpty());
        
        // Verify minimum frame size (header + framing)
        QVERIFY(frame.size() >= 9);
    }

    void testEncodeEmberDataLarge()
    {
        S101Protocol protocol;
        
        // Create larger Ember data
        libember::util::OctetStream emberData;
        for (int i = 0; i < 1000; ++i) {
            emberData.append(static_cast<unsigned char>(i & 0xFF));
        }
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        // Verify frame is not empty and reasonable size
        QVERIFY(!frame.isEmpty());
        QVERIFY(frame.size() >= 1000); // At least as large as payload
        QVERIFY(frame.size() < 2000);  // But not excessively large due to framing
    }

    void testMultipleEncodings()
    {
        S101Protocol protocol;
        
        // Encode multiple frames and verify they're consistent
        libember::util::OctetStream data1;
        data1.append(0xAA);
        
        libember::util::OctetStream data2;
        data2.append(0xBB);
        
        QByteArray frame1a = protocol.encodeEmberData(data1);
        QByteArray frame2 = protocol.encodeEmberData(data2);
        QByteArray frame1b = protocol.encodeEmberData(data1);
        
        // Same data should produce same frame
        QCOMPARE(frame1a, frame1b);
        
        // Different data should produce different frames
        QVERIFY(frame1a != frame2);
        
        // All frames should be similar sizes (differ by 1 byte payload)
        QVERIFY(qAbs(frame1a.size() - frame2.size()) <= 5); // Allow for escaping differences
    }

    void testKeepAliveConsistency()
    {
        S101Protocol protocol;
        
        // Multiple keep-alive responses should be identical
        QByteArray frame1 = protocol.encodeKeepAliveResponse();
        QByteArray frame2 = protocol.encodeKeepAliveResponse();
        QByteArray frame3 = protocol.encodeKeepAliveResponse();
        
        QCOMPARE(frame1, frame2);
        QCOMPARE(frame2, frame3);
    }

    void testEncodingDifferentiation()
    {
        S101Protocol protocol;
        
        // Keep-alive and Ember data frames should be different
        QByteArray keepAlive = protocol.encodeKeepAliveResponse();
        
        libember::util::OctetStream emberData;
        QByteArray emberFrame = protocol.encodeEmberData(emberData);
        
        // Should produce different frames
        QVERIFY(keepAlive != emberFrame);
    }

    void testAppBytesPresence()
    {
        S101Protocol protocol;
        
        // Create test data
        libember::util::OctetStream emberData;
        emberData.append(0xFF);
        
        QByteArray frame = protocol.encodeEmberData(emberData);
        
        // The frame should be larger than a frame without app bytes
        // With app bytes: 9 byte header + 1 byte payload + framing
        // Without app bytes: 6 byte header + 1 byte payload + framing
        // So we expect at least 10 bytes total
        QVERIFY(frame.size() >= 10);
        
        // This is an indirect test - we're verifying that the frame size
        // is consistent with having app bytes included
    }
};

QTEST_MAIN(TestS101Protocol)
#include "test_s101_protocol.moc"

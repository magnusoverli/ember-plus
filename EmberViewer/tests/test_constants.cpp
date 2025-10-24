#include <QtTest/QtTest>
#include "../include/MainWindow.h"
#include "../include/EmberConnection.h"

class TestConstants : public QObject
{
    Q_OBJECT

private slots:
    void testMainWindowConstants()
    {
        QCOMPARE(MainWindow::MATRIX_LABEL_PATH_MARKER, 666999666);
        QCOMPARE(MainWindow::ACTIVITY_TIMEOUT_MS, 60000);
        QCOMPARE(MainWindow::TICK_INTERVAL_MS, 1000);
        QCOMPARE(MainWindow::DEFAULT_EMBER_PORT, 9092);
        QCOMPARE(MainWindow::DEFAULT_PORT_FALLBACK, 9000);
    }
    
    void testEmberConnectionConstants()
    {
        QCOMPARE(EmberConnection::CONNECTION_TIMEOUT_MS, 5000);
    }
    
    void testConstantsArePositive()
    {
        QVERIFY(MainWindow::ACTIVITY_TIMEOUT_MS > 0);
        QVERIFY(MainWindow::TICK_INTERVAL_MS > 0);
        QVERIFY(MainWindow::DEFAULT_EMBER_PORT > 0);
        QVERIFY(MainWindow::DEFAULT_EMBER_PORT < 65536); // Valid port range
        QVERIFY(EmberConnection::CONNECTION_TIMEOUT_MS > 0);
    }
    
    void testTimeoutRelationships()
    {
        // Activity timeout should be multiple of tick interval
        QVERIFY(MainWindow::ACTIVITY_TIMEOUT_MS % MainWindow::TICK_INTERVAL_MS == 0);
        
        // Activity timeout should be reasonable (between 10s and 5min)
        QVERIFY(MainWindow::ACTIVITY_TIMEOUT_MS >= 10000);
        QVERIFY(MainWindow::ACTIVITY_TIMEOUT_MS <= 300000);
    }
};

QTEST_MAIN(TestConstants)
#include "test_constants.moc"

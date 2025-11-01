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
        QCOMPARE(MainWindow::DEFAULT_EMBER_PORT, 9092);
        QCOMPARE(MainWindow::DEFAULT_PORT_FALLBACK, 9000);
    }
    
    void testEmberConnectionConstants()
    {
        QCOMPARE(EmberConnection::CONNECTION_TIMEOUT_MS, 5000);
    }
    
    void testConstantsArePositive()
    {
        QVERIFY(MainWindow::DEFAULT_EMBER_PORT > 0);
        QVERIFY(MainWindow::DEFAULT_EMBER_PORT < 65536); 
        QVERIFY(EmberConnection::CONNECTION_TIMEOUT_MS > 0);
    }
};

QTEST_MAIN(TestConstants)
#include "test_constants.moc"

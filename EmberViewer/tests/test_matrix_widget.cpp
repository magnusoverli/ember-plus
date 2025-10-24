#include <QtTest/QtTest>
#include "../include/MatrixWidget.h"

class TestMatrixWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Qt Test needs a QApplication for widgets
    }
    
    void testMatrixWidgetCreation()
    {
        MatrixWidget widget;
        QVERIFY(widget.getMatrixType() == 2); // Default to N:N
    }
    
    void testSetMatrixInfo()
    {
        MatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 1, 8, 4);
        
        QCOMPARE(widget.getMatrixType(), 1);
    }
    
    void testTargetLabels()
    {
        MatrixWidget widget;
        
        widget.setTargetLabel(0, "Target 0");
        widget.setTargetLabel(1, "Target 1");
        
        QCOMPARE(widget.getTargetLabel(0), QString("Target 0"));
        QCOMPARE(widget.getTargetLabel(1), QString("Target 1"));
    }
    
    void testSourceLabels()
    {
        MatrixWidget widget;
        
        widget.setSourceLabel(0, "Source 0");
        widget.setSourceLabel(1, "Source 1");
        
        QCOMPARE(widget.getSourceLabel(0), QString("Source 0"));
        QCOMPARE(widget.getSourceLabel(1), QString("Source 1"));
    }
    
    void testDefaultLabels()
    {
        MatrixWidget widget;
        
        // Labels for non-existent entries should return defaults
        QString label = widget.getTargetLabel(99);
        QVERIFY(label.contains("99"));
        QVERIFY(label.contains("Target"));
        
        label = widget.getSourceLabel(99);
        QVERIFY(label.contains("99"));
        QVERIFY(label.contains("Source"));
    }
    
    void testConnectionState()
    {
        MatrixWidget widget;
        
        // Initially not connected
        QVERIFY(!widget.isConnected(0, 0));
        
        // Set connection
        widget.setConnection(0, 0, true, 0); // Tally disposition
        QVERIFY(widget.isConnected(0, 0));
        
        // Clear connection
        widget.setConnection(0, 0, false, 0);
        QVERIFY(!widget.isConnected(0, 0));
    }
    
    void testMultipleConnections()
    {
        MatrixWidget widget;
        
        // Set multiple connections
        widget.setConnection(0, 0, true, 0);
        widget.setConnection(0, 1, true, 0);
        widget.setConnection(1, 0, true, 0);
        
        QVERIFY(widget.isConnected(0, 0));
        QVERIFY(widget.isConnected(0, 1));
        QVERIFY(widget.isConnected(1, 0));
        QVERIFY(!widget.isConnected(1, 1));
    }
    
    void testClearConnections()
    {
        MatrixWidget widget;
        
        // Set some connections
        widget.setConnection(0, 0, true, 0);
        widget.setConnection(1, 1, true, 0);
        
        QVERIFY(widget.isConnected(0, 0));
        QVERIFY(widget.isConnected(1, 1));
        
        // Clear all
        widget.clearConnections();
        
        QVERIFY(!widget.isConnected(0, 0));
        QVERIFY(!widget.isConnected(1, 1));
    }
    
    void testConnectionDispositions()
    {
        MatrixWidget widget;
        
        // Test different dispositions don't affect connection state
        widget.setConnection(0, 0, true, 0); // Tally
        QVERIFY(widget.isConnected(0, 0));
        
        widget.setConnection(0, 1, true, 1); // Modified
        QVERIFY(widget.isConnected(0, 1));
        
        widget.setConnection(0, 2, true, 2); // Pending
        QVERIFY(widget.isConnected(0, 2));
        
        widget.setConnection(0, 3, true, 3); // Locked
        QVERIFY(widget.isConnected(0, 3));
    }
    
    void testCrosspointsEnabled()
    {
        MatrixWidget widget;
        
        // Default should be disabled
        widget.setCrosspointsEnabled(true);
        widget.setCrosspointsEnabled(false);
        
        // Just verify it doesn't crash
        QVERIFY(true);
    }
};

QTEST_MAIN(TestMatrixWidget)
#include "test_matrix_widget.moc"

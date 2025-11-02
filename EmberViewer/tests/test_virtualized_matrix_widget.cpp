#include <QtTest/QtTest>
#include "../include/VirtualizedMatrixWidget.h"
#include "../include/MatrixModel.h"

class TestVirtualizedMatrixWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Initialization code
    }
    
    void testVirtualizedMatrixWidgetCreation()
    {
        VirtualizedMatrixWidget widget;
        QVERIFY(widget.getMatrixType() == 2); // Default N:N
    }
    
    void testSetMatrixInfo()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 1, 8, 4);
        
        QCOMPARE(widget.getMatrixType(), 1);
        // Verify dimensions were auto-generated
        QCOMPARE(widget.getTargetNumbers().size(), 8);
        QCOMPARE(widget.getSourceNumbers().size(), 4);
    }
    
    void testTargetLabels()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        widget.setTargetLabel(0, "Target 0");
        widget.setTargetLabel(1, "Target 1");
        
        QCOMPARE(widget.getTargetLabel(0), QString("Target 0"));
        QCOMPARE(widget.getTargetLabel(1), QString("Target 1"));
    }
    
    void testSourceLabels()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        widget.setSourceLabel(0, "Source 0");
        widget.setSourceLabel(1, "Source 1");
        
        QCOMPARE(widget.getSourceLabel(0), QString("Source 0"));
        QCOMPARE(widget.getSourceLabel(1), QString("Source 1"));
    }
    
    void testDefaultLabels()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 100, 100);
        
        // Test default label generation
        QString label = widget.getTargetLabel(99);
        QVERIFY(label.contains("99"));
        QVERIFY(label.contains("Target"));
        
        label = widget.getSourceLabel(99);
        QVERIFY(label.contains("99"));
        QVERIFY(label.contains("Source"));
    }
    
    void testConnectionState()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        // Initially no connection
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
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
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
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        // Set connections
        widget.setConnection(0, 0, true, 0);
        widget.setConnection(1, 1, true, 0);
        
        QVERIFY(widget.isConnected(0, 0));
        QVERIFY(widget.isConnected(1, 1));
        
        // Clear all connections
        widget.clearConnections();
        
        QVERIFY(!widget.isConnected(0, 0));
        QVERIFY(!widget.isConnected(1, 1));
    }
    
    void testConnectionDispositions()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 1, 4);
        
        // Test different dispositions
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
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        // Test enabling/disabling crosspoints
        widget.setCrosspointsEnabled(true);
        widget.setCrosspointsEnabled(false);
        
        // No crash = success
        QVERIFY(true);
    }
    
    void testClearTargetConnections()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        // Set connections for multiple targets
        widget.setConnection(0, 0, true, 0);
        widget.setConnection(0, 1, true, 0);
        widget.setConnection(1, 0, true, 0);
        widget.setConnection(1, 1, true, 0);
        
        QVERIFY(widget.isConnected(0, 0));
        QVERIFY(widget.isConnected(0, 1));
        QVERIFY(widget.isConnected(1, 0));
        QVERIFY(widget.isConnected(1, 1));
        
        // Clear connections for target 0 only
        widget.clearTargetConnections(0);
        
        QVERIFY(!widget.isConnected(0, 0));
        QVERIFY(!widget.isConnected(0, 1));
        QVERIFY(widget.isConnected(1, 0)); // Target 1 should remain
        QVERIFY(widget.isConnected(1, 1));
    }
    
    void testLargeMatrix()
    {
        VirtualizedMatrixWidget widget;
        // Test that large matrices don't crash (virtualization benefit)
        widget.setMatrixInfo("LargeMatrix", "Test Large Matrix", 2, 5000, 5000);
        
        QCOMPARE(widget.getTargetNumbers().size(), 5000);
        QCOMPARE(widget.getSourceNumbers().size(), 5000);
        
        // Set a connection in the large matrix
        widget.setConnection(2500, 2500, true, 0);
        QVERIFY(widget.isConnected(2500, 2500));
        
        // No crash or freeze = virtualization working
        QVERIFY(true);
    }
    
    void testRebuild()
    {
        VirtualizedMatrixWidget widget;
        widget.setMatrixInfo("TestMatrix", "Test Description", 2, 2, 2);
        
        widget.setConnection(0, 0, true, 0);
        
        // Rebuild should not lose data
        widget.rebuild();
        
        QVERIFY(widget.isConnected(0, 0));
        QCOMPARE(widget.getMatrixType(), 2);
    }
};

QTEST_MAIN(TestVirtualizedMatrixWidget)
#include "test_virtualized_matrix_widget.moc"


#include <QtTest/QtTest>
#include <QTreeWidget>
#include <QTreeWidgetItem>

// We'll test path parsing logic independently
class TestPathParsing : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_treeWidget = new QTreeWidget();
    }
    
    void cleanupTestCase()
    {
        delete m_treeWidget;
    }
    
    void testBasicPathSplit()
    {
        QString path = "1.2.3";
        QStringList parts = path.split('.', Qt::SkipEmptyParts);
        
        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts[0], QString("1"));
        QCOMPARE(parts[1], QString("2"));
        QCOMPARE(parts[2], QString("3"));
    }
    
    void testEmptyPath()
    {
        QString path = "";
        QStringList parts = path.split('.', Qt::SkipEmptyParts);
        
        QCOMPARE(parts.size(), 0);
    }
    
    void testSingleLevelPath()
    {
        QString path = "0";
        QStringList parts = path.split('.', Qt::SkipEmptyParts);
        
        QCOMPARE(parts.size(), 1);
        QCOMPARE(parts[0], QString("0"));
    }
    
    void testPathWithTrailingDot()
    {
        QString path = "1.2.3.";
        QStringList parts = path.split('.', Qt::SkipEmptyParts);
        
        QCOMPARE(parts.size(), 3); // Trailing dot should be ignored
    }
    
    void testPathConstruction()
    {
        QStringList parts = {"1", "2", "3"};
        QString path = parts.join(".");
        
        QCOMPARE(path, QString("1.2.3"));
    }
    
    void testPathPrefix()
    {
        QString fullPath = "1.2.3.4";
        QString prefix = "1.2";
        
        QVERIFY(fullPath.startsWith(prefix));
        QVERIFY(fullPath.startsWith(prefix + "."));
        QVERIFY(!fullPath.startsWith("1.3"));
    }
    
    void testMatrixLabelPathPattern()
    {
        // Test matrix label path pattern: matrixPath.666999666.1.N (targets)
        QString targetPath = "1.2.666999666.1.5";
        QStringList parts = targetPath.split('.');
        
        QCOMPARE(parts.size(), 5);
        QCOMPARE(parts[2], QString("666999666"));
        QCOMPARE(parts[3], QString("1")); // 1 = targets
        
        // Extract matrix path (everything except last 3 segments)
        QStringList matrixParts = parts.mid(0, parts.size() - 3);
        QString matrixPath = matrixParts.join('.');
        QCOMPARE(matrixPath, QString("1.2"));
    }
    
    void testPathDepth()
    {
        auto getDepth = [](const QString& path) {
            return path.split('.', Qt::SkipEmptyParts).size();
        };
        
        QCOMPARE(getDepth("1"), 1);
        QCOMPARE(getDepth("1.2"), 2);
        QCOMPARE(getDepth("1.2.3"), 3);
        QCOMPARE(getDepth(""), 0);
    }

private:
    QTreeWidget *m_treeWidget;
};

QTEST_MAIN(TestPathParsing)
#include "test_path_parsing.moc"

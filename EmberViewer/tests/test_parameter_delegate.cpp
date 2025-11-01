#include <QtTest/QtTest>
#include <QTreeWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include "../include/ParameterDelegate.h"

class TestParameterDelegate : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_treeWidget = new QTreeWidget();
        m_delegate = new ParameterDelegate(m_treeWidget);
        
        
        m_treeWidget->setColumnCount(3);
        m_treeWidget->setHeaderLabels(QStringList() << "Path" << "Type" << "Value");
    }
    
    void cleanupTestCase()
    {
        delete m_delegate;
        delete m_treeWidget;
    }
    
    void testDelegateCreation()
    {
        QVERIFY(m_delegate != nullptr);
    }
    
    void testIntegerParameterRoles()
    {
        
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "42");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.3")); 
        item->setData(0, Qt::UserRole + 1, 1); 
        item->setData(0, Qt::UserRole + 2, 3); 
        item->setData(0, Qt::UserRole + 3, QVariant(0)); 
        item->setData(0, Qt::UserRole + 4, QVariant(100)); 
        
        
        QCOMPARE(item->data(0, Qt::UserRole).toString(), QString("1.2.3"));
        QCOMPARE(item->data(0, Qt::UserRole + 1).toInt(), 1);
        QCOMPARE(item->data(0, Qt::UserRole + 2).toInt(), 3);
    }
    
    void testRealParameterRoles()
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "3.14");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.4"));
        item->setData(0, Qt::UserRole + 1, 2); 
        item->setData(0, Qt::UserRole + 2, 3); 
        item->setData(0, Qt::UserRole + 3, QVariant(-10.0));
        item->setData(0, Qt::UserRole + 4, QVariant(10.0));
        
        QCOMPARE(item->data(0, Qt::UserRole + 1).toInt(), 2);
    }
    
    void testStringParameterRoles()
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "Hello");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.5"));
        item->setData(0, Qt::UserRole + 1, 3); 
        item->setData(0, Qt::UserRole + 2, 3); 
        
        QCOMPARE(item->data(0, Qt::UserRole + 1).toInt(), 3);
    }
    
    void testBooleanParameterRoles()
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "true");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.6"));
        item->setData(0, Qt::UserRole + 1, 4); 
        item->setData(0, Qt::UserRole + 2, 3); 
        
        QCOMPARE(item->data(0, Qt::UserRole + 1).toInt(), 4);
    }
    
    void testEnumParameterRoles()
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "Option1");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.7"));
        item->setData(0, Qt::UserRole + 1, 6); 
        item->setData(0, Qt::UserRole + 2, 3); 
        
        QStringList enumOptions;
        enumOptions << "Option1" << "Option2" << "Option3";
        item->setData(0, Qt::UserRole + 5, enumOptions);
        
        QList<QVariant> enumValues;
        enumValues << 0 << 1 << 2;
        item->setData(0, Qt::UserRole + 6, QVariant::fromValue(enumValues));
        
        QCOMPARE(item->data(0, Qt::UserRole + 1).toInt(), 6);
        QCOMPARE(item->data(0, Qt::UserRole + 5).toStringList().size(), 3);
    }
    
    void testReadOnlyParameter()
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, "TestParam");
        item->setText(1, "Parameter");
        item->setText(2, "100");
        
        
        item->setData(0, Qt::UserRole, QString("1.2.8"));
        item->setData(0, Qt::UserRole + 1, 1); 
        item->setData(0, Qt::UserRole + 2, 1); 
        
        QCOMPARE(item->data(0, Qt::UserRole + 2).toInt(), 1);
    }
    
    void testParameterTypeConstants()
    {
        
        enum ParameterType {
            None = 0,
            Integer = 1,
            Real = 2,
            String = 3,
            Boolean = 4,
            Trigger = 5,
            Enum = 6,
            Octets = 7
        };
        
        QCOMPARE(int(ParameterType::Integer), 1);
        QCOMPARE(int(ParameterType::Real), 2);
        QCOMPARE(int(ParameterType::String), 3);
        QCOMPARE(int(ParameterType::Boolean), 4);
        QCOMPARE(int(ParameterType::Trigger), 5);
        QCOMPARE(int(ParameterType::Enum), 6);
    }

private:
    QTreeWidget *m_treeWidget;
    ParameterDelegate *m_delegate;
};

QTEST_MAIN(TestParameterDelegate)
#include "test_parameter_delegate.moc"

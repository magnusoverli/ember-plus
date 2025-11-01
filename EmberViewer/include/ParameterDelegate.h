







#ifndef PARAMETERDELEGATE_H
#define PARAMETERDELEGATE_H

#include <QStyledItemDelegate>
#include <QTreeWidget>

class ParameterDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ParameterDelegate(QObject *parent = nullptr);

    
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void valueChanged(const QString &path, const QString &newValue) const;

private:
    
    enum DataRole {
        TypeRole = Qt::UserRole + 1,      
        AccessRole = Qt::UserRole + 2,    
        MinimumRole = Qt::UserRole + 3,   
        MaximumRole = Qt::UserRole + 4,   
        EnumOptionsRole = Qt::UserRole + 5, 
        EnumValuesRole = Qt::UserRole + 6,  
        PathRole = Qt::UserRole + 7       
    };
};

#endif 


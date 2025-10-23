#ifndef PARAMETERDELEGATE_H
#define PARAMETERDELEGATE_H

#include <QStyledItemDelegate>
#include <QTreeWidget>

class ParameterDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ParameterDelegate(QObject *parent = nullptr);

    // Create appropriate editor widget based on parameter type
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Populate editor with current value
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // Extract new value from editor
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    // Editor geometry
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void valueChanged(const QString &path, const QString &newValue) const;

private:
    // Parameter metadata stored in QTreeWidgetItem custom roles
    enum DataRole {
        TypeRole = Qt::UserRole + 1,      // int: ParameterType value
        AccessRole = Qt::UserRole + 2,    // int: Access value
        MinimumRole = Qt::UserRole + 3,   // QVariant: minimum value (int/double)
        MaximumRole = Qt::UserRole + 4,   // QVariant: maximum value (int/double)
        EnumOptionsRole = Qt::UserRole + 5, // QStringList: enum option names
        EnumValuesRole = Qt::UserRole + 6,  // QList<int>: enum option values
        PathRole = Qt::UserRole + 7       // QString: ember path (already stored at Qt::UserRole)
    };
};

#endif // PARAMETERDELEGATE_H


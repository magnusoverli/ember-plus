







#ifndef PATHCOLUMNDELEGATE_H
#define PATHCOLUMNDELEGATE_H

#include <QStyledItemDelegate>

class PathColumnDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PathColumnDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    static constexpr int EXTRA_PADDING = 30;
};

#endif 

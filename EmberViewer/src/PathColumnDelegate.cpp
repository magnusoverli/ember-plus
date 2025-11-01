







#include "PathColumnDelegate.h"

PathColumnDelegate::PathColumnDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize PathColumnDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.rwidth() += EXTRA_PADDING;
    return size;
}

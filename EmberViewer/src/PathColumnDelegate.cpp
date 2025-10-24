/*
    EmberViewer - Custom delegate for path column padding
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

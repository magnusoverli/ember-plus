/*
    EmberViewer - Custom delegate for path column padding
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

#endif // PATHCOLUMNDELEGATE_H

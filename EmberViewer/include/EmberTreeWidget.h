/*
    EmberViewer - Custom Tree Widget for Ember+ Device Tree
    
    Custom QTreeWidget that distinguishes between clicks on expand arrows
    and clicks on item text, allowing instant expand/collapse on arrow clicks
    while preserving double-click detection for item text.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef EMBERTREEWIDGET_H
#define EMBERTREEWIDGET_H

#include <QTreeWidget>
#include <QMouseEvent>
#include <QApplication>

class EmberTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit EmberTreeWidget(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    
private:
    int m_savedDoubleClickInterval;
    bool isClickOnExpandArrow(const QPoint &pos, const QModelIndex &index);
};

#endif // EMBERTREEWIDGET_H


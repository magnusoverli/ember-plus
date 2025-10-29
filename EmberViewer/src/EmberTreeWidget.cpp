/*
    EmberViewer - Custom Tree Widget for Ember+ Device Tree Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "EmberTreeWidget.h"
#include <QDebug>

EmberTreeWidget::EmberTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
    , m_savedDoubleClickInterval(QApplication::doubleClickInterval())
{
}

bool EmberTreeWidget::isClickOnExpandArrow(const QPoint &pos, const QModelIndex &index)
{
    if (!index.isValid()) {
        return false;
    }
    
    // Get the visual rectangle for this item
    QRect rect = visualRect(index);
    
    // Calculate indentation level (how deep in the tree)
    int level = 0;
    QModelIndex parent = index.parent();
    while (parent.isValid()) {
        level++;
        parent = parent.parent();
    }
    
    // Calculate arrow area
    // The arrow is typically in the first ~20 pixels of the indented area
    int indent = indentation();
    int arrowX = rect.left() + (level * indent);
    int arrowWidth = 20;  // Approximate width of expand arrow area
    
    QRect arrowRect(arrowX, rect.top(), arrowWidth, rect.height());
    
    return arrowRect.contains(pos);
}

void EmberTreeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QTreeWidget::mousePressEvent(event);
        return;
    }
    
    QModelIndex index = indexAt(event->pos());
    
    // Check if click is on the expand/collapse arrow
    if (index.isValid() && isClickOnExpandArrow(event->pos(), index)) {
        // Arrow click: Temporarily disable double-click detection for instant response
        int originalInterval = QApplication::doubleClickInterval();
        QApplication::setDoubleClickInterval(0);  // Instant - no wait for double-click
        
        // Let Qt handle the expand/collapse
        QTreeWidget::mousePressEvent(event);
        
        // Restore double-click interval immediately after
        QApplication::setDoubleClickInterval(originalInterval);
    } else {
        // Normal item click: Use standard double-click detection (250ms)
        QTreeWidget::mousePressEvent(event);
    }
}










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
    
    
    QRect rect = visualRect(index);
    
    
    int level = 0;
    QModelIndex parent = index.parent();
    while (parent.isValid()) {
        level++;
        parent = parent.parent();
    }
    
    
    
    int indent = indentation();
    int arrowX = rect.left() + (level * indent);
    int arrowWidth = 20;  
    
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
    
    
    if (index.isValid() && isClickOnExpandArrow(event->pos(), index)) {
        
        int originalInterval = QApplication::doubleClickInterval();
        QApplication::setDoubleClickInterval(0);  
        
        
        QTreeWidget::mousePressEvent(event);
        
        
        QApplication::setDoubleClickInterval(originalInterval);
    } else {
        
        QTreeWidget::mousePressEvent(event);
    }
}


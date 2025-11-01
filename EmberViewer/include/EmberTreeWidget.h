











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

#endif 


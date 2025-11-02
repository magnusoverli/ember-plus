/*
    VirtualizedSidebarView.h - Virtualized vertical sidebar for source labels
*/

#ifndef VIRTUALIZEDSIDEBARVIEW_H
#define VIRTUALIZEDSIDEBARVIEW_H

#include <QWidget>
#include <QSize>

class MatrixModel;

class VirtualizedSidebarView : public QWidget
{
    Q_OBJECT

public:
    explicit VirtualizedSidebarView(int cellHeight, QWidget *parent = nullptr);
    ~VirtualizedSidebarView();

    void setModel(MatrixModel *model);
    void setCellHeight(int height);
    void setScrollOffset(int offset);
    void setHighlightedRow(int row);  // For crosshair effect
    
    int cellHeight() const { return m_cellHeight; }
    int scrollOffset() const { return m_scrollOffset; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MatrixModel *m_model;
    int m_cellHeight;
    int m_scrollOffset;
    int m_highlightedRow;  // -1 means no highlight
};

#endif // VIRTUALIZEDSIDEBARVIEW_H


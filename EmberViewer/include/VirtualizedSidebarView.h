/*
    VirtualizedSidebarView.h - Virtualized vertical sidebar for source labels (inputs/rows)
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
    void setCrosspointsEnabled(bool enabled);
    
    int cellHeight() const { return m_cellHeight; }
    int scrollOffset() const { return m_scrollOffset; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void sidebarWidthChanged(int newWidth);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool isInResizeZone(int x) const;
    void updateCursor(int x);

    MatrixModel *m_model;
    int m_cellHeight;
    int m_scrollOffset;
    int m_highlightedRow;  // -1 means no highlight
    bool m_crosspointsEnabled;
    
    // Resize handling
    bool m_isResizing;
    int m_resizeStartX;
    int m_resizeStartWidth;
    static constexpr int RESIZE_HANDLE_WIDTH = 10;  // Width of draggable zone at right edge
    static constexpr int MIN_SIDEBAR_WIDTH = 80;
    static constexpr int MAX_SIDEBAR_WIDTH = 300;
};

#endif // VIRTUALIZEDSIDEBARVIEW_H


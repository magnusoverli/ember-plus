/*
    VirtualizedHeaderView.h - Virtualized horizontal header for target labels (outputs/columns)
*/

#ifndef VIRTUALIZEDHEADERVIEW_H
#define VIRTUALIZEDHEADERVIEW_H

#include <QWidget>
#include <QSize>

class MatrixModel;

class VirtualizedHeaderView : public QWidget
{
    Q_OBJECT

public:
    explicit VirtualizedHeaderView(int cellWidth, QWidget *parent = nullptr);
    ~VirtualizedHeaderView();

    void setModel(MatrixModel *model);
    void setCellWidth(int width);
    void setScrollOffset(int offset);
    void setHighlightedColumn(int col);  // For crosshair effect
    void setCrosspointsEnabled(bool enabled);
    
    int cellWidth() const { return m_cellWidth; }
    int scrollOffset() const { return m_scrollOffset; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void headerHeightChanged(int newHeight);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool isInResizeZone(int y) const;
    void updateCursor(int y);

    MatrixModel *m_model;
    int m_cellWidth;
    int m_scrollOffset;
    int m_highlightedCol;  // -1 means no highlight
    bool m_crosspointsEnabled;
    
    // Resize handling
    bool m_isResizing;
    int m_resizeStartY;
    int m_resizeStartHeight;
    static constexpr int RESIZE_HANDLE_HEIGHT = 10;  // Increased from 5 to 10 pixels for easier activation
    static constexpr int MIN_HEADER_HEIGHT = 30;
    static constexpr int MAX_HEADER_HEIGHT = 200;
};

#endif // VIRTUALIZEDHEADERVIEW_H


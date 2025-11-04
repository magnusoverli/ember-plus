

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
    void setHighlightedColumn(int col);  
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
    int m_highlightedCol;  
    bool m_crosspointsEnabled;
    
    
    bool m_isResizing;
    int m_resizeStartY;
    int m_resizeStartHeight;
    static constexpr int RESIZE_HANDLE_HEIGHT = 10;  
    static constexpr int MIN_HEADER_HEIGHT = 10;
    static constexpr int MAX_HEADER_HEIGHT = 200;
};

#endif 


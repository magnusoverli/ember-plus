/*
    VirtualizedHeaderView.h - Virtualized horizontal header for target labels
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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MatrixModel *m_model;
    int m_cellWidth;
    int m_scrollOffset;
    int m_highlightedCol;  // -1 means no highlight
    bool m_crosspointsEnabled;
};

#endif // VIRTUALIZEDHEADERVIEW_H


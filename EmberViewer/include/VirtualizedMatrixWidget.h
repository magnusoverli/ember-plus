

#ifndef VIRTUALIZEDMATRIXWIDGET_H
#define VIRTUALIZEDMATRIXWIDGET_H

#include <QAbstractScrollArea>
#include <QPoint>
#include <QSize>
#include <QPushButton>
#include "MatrixModel.h"

class QLabel;
class VirtualizedHeaderView;
class VirtualizedSidebarView;

class VirtualizedMatrixWidget : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit VirtualizedMatrixWidget(QWidget *parent = nullptr);
    ~VirtualizedMatrixWidget();

    
    void setModel(MatrixModel *model);
    MatrixModel* model() const { return m_model; }

    
    void setMatrixInfo(const QString &identifier, const QString &description,
                       int type, int targetCount, int sourceCount);
    void setMatrixPath(const QString &path);
    void setTargetLabel(int targetNumber, const QString &label);
    void setSourceLabel(int sourceNumber, const QString &label);
    void setConnection(int targetNumber, int sourceNumber, bool connected, int disposition = 0);
    void clearConnections();
    void clearTargetConnections(int targetNumber);
    
    // Batch update optimization
    void beginBatchUpdate();
    void endBatchUpdate();
    void rebuild();
    bool isConnected(int targetNumber, int sourceNumber) const;
    int getMatrixType() const;
    QString getTargetLabel(int targetNumber) const;
    QString getSourceLabel(int sourceNumber) const;
    QList<int> getTargetNumbers() const;
    QList<int> getSourceNumbers() const;
    void setCrosspointsEnabled(bool enabled);
    void updateCornerButton(bool enabled, int timeRemaining);

    
    void setCellSize(const QSize &size);
    QSize cellSize() const { return m_cellSize; }

    
    void setHeaderHeight(int height);
    void setSidebarWidth(int width);
    int headerHeight() const { return m_headerHeight; }
    int sidebarWidth() const { return m_sidebarWidth; }
    
    
    static void setPreferredHeaderHeight(int height);
    static void setPreferredSidebarWidth(int width);
    static int preferredHeaderHeight();
    static int preferredSidebarWidth();

    
    QPoint cellAt(const QPoint &pos) const;
    QRect cellRect(int row, int col) const;

    
    void setSelectedCell(int targetIndex, int sourceIndex);
    void clearSelection();
    QPoint selectedCell() const { return m_selectedCell; }

    
    QPoint hoveredCell() const { return m_hoveredCell; }

    
    void refresh();

signals:
    void crosspointClicked(int targetNumber, int sourceNumber);
    void crosspointHovered(int targetNumber, int sourceNumber);
    void selectionChanged(int targetNumber, int sourceNumber);
    
    void crosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    void enableCrosspointsRequested(bool enable);
    void crosspointToggleRequested();

protected:
    
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onModelDataChanged();
    void onModelConnectionChanged(int targetNumber, int sourceNumber, bool connected);

private:
    void updateScrollBars();
    void updateViewportSize();
    QRect visibleCellsRect() const;
    void drawGrid(QPainter &painter, const QRect &visibleCells);
    void drawConnections(QPainter &painter, const QRect &visibleCells);
    void drawSelection(QPainter &painter);
    void drawHover(QPainter &painter);
    void invalidateCellRegion(int row, int col);
    void invalidateCell(int targetNumber, int sourceNumber);

    MatrixModel *m_model;

    
    QSize m_cellSize;           
    int m_headerHeight;         
    int m_sidebarWidth;         
    
    
    QPoint m_selectedCell;      
    QPoint m_hoveredCell;       
    bool m_isDragging;
    bool m_crosspointsEnabled;

    
    VirtualizedHeaderView *m_headerView;
    VirtualizedSidebarView *m_sidebarView;
    QPushButton *m_cornerWidget;
    
    
    QString m_matrixPath;
};

#endif 


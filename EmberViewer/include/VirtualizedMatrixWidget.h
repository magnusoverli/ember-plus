/*
    VirtualizedMatrixWidget.h - High-performance virtualized matrix grid
    Renders only visible crosspoints for efficient handling of huge matrices
*/

#ifndef VIRTUALIZEDMATRIXWIDGET_H
#define VIRTUALIZEDMATRIXWIDGET_H

#include <QAbstractScrollArea>
#include <QString>
#include <QPoint>
#include <QRect>
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

    // Model management
    void setModel(MatrixModel *model);
    MatrixModel* model() const { return m_model; }

    // MatrixWidget API compatibility methods
    void setMatrixInfo(const QString &identifier, const QString &description,
                       int type, int targetCount, int sourceCount);
    void setMatrixPath(const QString &path);
    void setTargetLabel(int targetNumber, const QString &label);
    void setSourceLabel(int sourceNumber, const QString &label);
    void setConnection(int targetNumber, int sourceNumber, bool connected, int disposition = 0);
    void clearConnections();
    void clearTargetConnections(int targetNumber);
    void rebuild();
    bool isConnected(int targetNumber, int sourceNumber) const;
    int getMatrixType() const;
    QString getTargetLabel(int targetNumber) const;
    QString getSourceLabel(int sourceNumber) const;
    QList<int> getTargetNumbers() const;
    QList<int> getSourceNumbers() const;
    void setCrosspointsEnabled(bool enabled);

    // Cell sizing
    void setCellSize(const QSize &size);
    QSize cellSize() const { return m_cellSize; }

    // Header/Sidebar dimensions
    void setHeaderHeight(int height);
    void setSidebarWidth(int width);
    int headerHeight() const { return m_headerHeight; }
    int sidebarWidth() const { return m_sidebarWidth; }

    // Coordinate conversion
    QPoint cellAt(const QPoint &pos) const;
    QRect cellRect(int row, int col) const;

    // Selection state
    void setSelectedCell(int targetIndex, int sourceIndex);
    void clearSelection();
    QPoint selectedCell() const { return m_selectedCell; }

    // Hover state
    QPoint hoveredCell() const { return m_hoveredCell; }

    // Force full repaint (e.g., after connection changes)
    void refresh();

signals:
    // New API signals
    void crosspointClicked(int targetNumber, int sourceNumber);
    void crosspointHovered(int targetNumber, int sourceNumber);
    
    // MatrixWidget compatibility signals
    void crosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    void crosspointToggleRequested();

protected:
    // QAbstractScrollArea overrides
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

    // Layout parameters
    QSize m_cellSize;           // Size of each crosspoint cell
    int m_headerHeight;         // Height of target label header
    int m_sidebarWidth;         // Width of source label sidebar
    
    // Interaction state
    QPoint m_selectedCell;      // (row, col) in visible grid coordinates
    QPoint m_hoveredCell;       // (row, col) in visible grid coordinates
    bool m_isDragging;
    bool m_crosspointsEnabled;

    // Child views
    VirtualizedHeaderView *m_headerView;
    VirtualizedSidebarView *m_sidebarView;
    QWidget *m_cornerWidget;
    
    // Path for signal compatibility
    QString m_matrixPath;
};

#endif // VIRTUALIZEDMATRIXWIDGET_H


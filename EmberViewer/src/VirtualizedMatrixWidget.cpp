

#include "VirtualizedMatrixWidget.h"
#include "VirtualizedHeaderView.h"
#include "VirtualizedSidebarView.h"
#include "MatrixModel.h"
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>


static int s_preferredHeaderHeight = 80;
static int s_preferredSidebarWidth = 80;

VirtualizedMatrixWidget::VirtualizedMatrixWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_model(nullptr)
    , m_cellSize(17, 17)
    , m_headerHeight(s_preferredHeaderHeight)
    , m_sidebarWidth(s_preferredSidebarWidth)
    , m_selectedCell(-1, -1)
    , m_hoveredCell(-1, -1)
    , m_isDragging(false)
    , m_crosspointsEnabled(true)
{
    
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    
    setFocusPolicy(Qt::StrongFocus);
    
    
    viewport()->setToolTip("");

    
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    viewport()->setBackgroundRole(QPalette::Base);
    
    
    setViewportMargins(m_sidebarWidth, m_headerHeight, 0, 0);

    
    m_headerView = new VirtualizedHeaderView(m_cellSize.width(), this);

    
    m_sidebarView = new VirtualizedSidebarView(m_cellSize.height(), this);

    
    m_cornerWidget = new QPushButton(this);
    m_cornerWidget->setCheckable(true);
    m_cornerWidget->setChecked(false);
    
    
    QIcon lockIcon;
    lockIcon.addFile(":/resources/lock-closed.png", QSize(), QIcon::Normal, QIcon::Off);
    lockIcon.addFile(":/resources/lock-open.png", QSize(), QIcon::Normal, QIcon::On);
    m_cornerWidget->setIcon(lockIcon);
    m_cornerWidget->setIconSize(QSize(m_sidebarWidth - 5, m_headerHeight - 5));
    
    m_cornerWidget->setToolTip("Click to enable crosspoint editing (Ctrl+E)");
    m_cornerWidget->setStyleSheet(
        "QPushButton { "
        "  background-color: palette(button); "
        "  border: 2px solid palette(mid); "
        "}"
    );
    
    
    connect(m_cornerWidget, &QPushButton::toggled, this, &VirtualizedMatrixWidget::enableCrosspointsRequested);
    
    
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, 
            this, [this](int value) {
        m_headerView->setScrollOffset(value);
    });
    
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        m_sidebarView->setScrollOffset(value);
    });
    
    
    connect(m_headerView, &VirtualizedHeaderView::headerHeightChanged,
            this, [this](int newHeight) {
        setHeaderHeight(newHeight);
    });
    
    
    connect(m_sidebarView, &VirtualizedSidebarView::sidebarWidthChanged,
            this, [this](int newWidth) {
        setSidebarWidth(newWidth);
    });

    
    updateViewportSize();
}

VirtualizedMatrixWidget::~VirtualizedMatrixWidget()
{
}

void VirtualizedMatrixWidget::setModel(MatrixModel *model)
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if (m_model) {
        connect(m_model, &MatrixModel::dataChanged, 
                this, &VirtualizedMatrixWidget::onModelDataChanged);
        connect(m_model, &MatrixModel::connectionChanged,
                this, &VirtualizedMatrixWidget::onModelConnectionChanged);
        
        
        QString matrixTypeStr;
        switch (m_model->matrixType()) {
            case 0: matrixTypeStr = "1:N"; break;
            case 1: matrixTypeStr = "N:1"; break;
            case 2: matrixTypeStr = "N:N"; break;
            default: matrixTypeStr = "Unknown"; break;
        }
        
        
        
    }

    
    m_headerView->setModel(model);
    m_sidebarView->setModel(model);
    
    
    connect(m_headerView, &VirtualizedHeaderView::headerHeightChanged,
            this, [this](int newHeight) {
        setHeaderHeight(newHeight);
    });

    updateScrollBars();
    viewport()->update();
}

void VirtualizedMatrixWidget::setCellSize(const QSize &size)
{
    m_cellSize = size;
    m_headerView->setCellWidth(size.width());
    m_sidebarView->setCellHeight(size.height());
    updateScrollBars();
    viewport()->update();
}

void VirtualizedMatrixWidget::setHeaderHeight(int height)
{
    m_headerHeight = height;
    s_preferredHeaderHeight = height;  
    setViewportMargins(m_sidebarWidth, m_headerHeight, 0, 0);
    updateViewportSize();
}

void VirtualizedMatrixWidget::setSidebarWidth(int width)
{
    m_sidebarWidth = width;
    s_preferredSidebarWidth = width;  
    setViewportMargins(m_sidebarWidth, m_headerHeight, 0, 0);
    updateViewportSize();
}

void VirtualizedMatrixWidget::setSelectedCell(int targetIndex, int sourceIndex)
{
    if (!m_model) return;

    const QList<int> &targets = m_model->targetNumbers();
    const QList<int> &sources = m_model->sourceNumbers();

    
    int col = targets.indexOf(targetIndex);
    int row = sources.indexOf(sourceIndex);

    if (row >= 0 && col >= 0) {
        m_selectedCell = QPoint(col, row);
        viewport()->update();
    }
}

void VirtualizedMatrixWidget::clearSelection()
{
    m_selectedCell = QPoint(-1, -1);
    viewport()->update();
}

void VirtualizedMatrixWidget::refresh()
{
    viewport()->update();
}





void VirtualizedMatrixWidget::setMatrixInfo(const QString &identifier, const QString &description,
                                             int type, int targetCount, int sourceCount)
{
    if (!m_model) {
        m_model = new MatrixModel(this);
        m_headerView->setModel(m_model);
        m_sidebarView->setModel(m_model);
        
        connect(m_model, &MatrixModel::dataChanged, 
                this, &VirtualizedMatrixWidget::onModelDataChanged);
        connect(m_model, &MatrixModel::connectionChanged,
                this, &VirtualizedMatrixWidget::onModelConnectionChanged);
    }
    
    m_model->setMatrixInfo(identifier, description, type, targetCount, sourceCount);
    
    
    QString matrixTypeStr;
    switch (type) {
        case 0: matrixTypeStr = "1:N"; break;
        case 1: matrixTypeStr = "N:1"; break;
        case 2: matrixTypeStr = "N:N"; break;
        default: matrixTypeStr = "Unknown"; break;
    }
    
    
    
    
    updateScrollBars();
    viewport()->update();
}

void VirtualizedMatrixWidget::setMatrixPath(const QString &path)
{
    m_matrixPath = path;
    if (m_model) {
        m_model->setMatrixPath(path);
    }
}

void VirtualizedMatrixWidget::setTargetLabel(int targetNumber, const QString &label)
{
    if (m_model) {
        m_model->setTargetLabel(targetNumber, label);
    }
}

void VirtualizedMatrixWidget::setSourceLabel(int sourceNumber, const QString &label)
{
    if (m_model) {
        m_model->setSourceLabel(sourceNumber, label);
    }
}

void VirtualizedMatrixWidget::setConnection(int targetNumber, int sourceNumber, bool connected, int disposition)
{
    if (m_model) {
        m_model->setConnection(targetNumber, sourceNumber, connected, disposition);
    }
}

void VirtualizedMatrixWidget::clearConnections()
{
    if (m_model) {
        m_model->clearConnections();
    }
}

void VirtualizedMatrixWidget::clearTargetConnections(int targetNumber)
{
    if (m_model) {
        m_model->clearTargetConnections(targetNumber);
    }
}

void VirtualizedMatrixWidget::beginBatchUpdate()
{
    if (m_model) {
        m_model->beginBatchUpdate();
    }
}

void VirtualizedMatrixWidget::endBatchUpdate()
{
    if (m_model) {
        m_model->endBatchUpdate();
    }
}

void VirtualizedMatrixWidget::rebuild()
{
    
    updateScrollBars();
    viewport()->update();
    m_headerView->update();
    m_sidebarView->update();
}

bool VirtualizedMatrixWidget::isConnected(int targetNumber, int sourceNumber) const
{
    return m_model ? m_model->isConnected(targetNumber, sourceNumber) : false;
}

int VirtualizedMatrixWidget::getMatrixType() const
{
    return m_model ? m_model->matrixType() : 2;
}

QString VirtualizedMatrixWidget::getTargetLabel(int targetNumber) const
{
    return m_model ? m_model->targetLabel(targetNumber) : QString();
}

QString VirtualizedMatrixWidget::getSourceLabel(int sourceNumber) const
{
    return m_model ? m_model->sourceLabel(sourceNumber) : QString();
}

QList<int> VirtualizedMatrixWidget::getTargetNumbers() const
{
    return m_model ? m_model->targetNumbers() : QList<int>();
}

QList<int> VirtualizedMatrixWidget::getSourceNumbers() const
{
    return m_model ? m_model->sourceNumbers() : QList<int>();
}

void VirtualizedMatrixWidget::setCrosspointsEnabled(bool enabled)
{
    m_crosspointsEnabled = enabled;
    m_cornerWidget->setChecked(enabled);
    
    
    m_headerView->setCrosspointsEnabled(enabled);
    m_sidebarView->setCrosspointsEnabled(enabled);
}

void VirtualizedMatrixWidget::updateCornerButton(bool enabled, int timeRemaining)
{
    Q_UNUSED(timeRemaining);
    
    if (enabled) {
        m_cornerWidget->setToolTip("Crosspoint editing enabled\nClick to disable");
    } else {
        m_cornerWidget->setToolTip("Click to enable crosspoint editing (Ctrl+E)");
    }
    m_cornerWidget->setChecked(enabled);
}

QPoint VirtualizedMatrixWidget::cellAt(const QPoint &pos) const
{
    if (!m_model) return QPoint(-1, -1);

    QPoint vpPos = pos - QPoint(0, 0);

    int scrollX = horizontalScrollBar()->value();
    int scrollY = verticalScrollBar()->value();

    int col = (vpPos.x() + scrollX) / m_cellSize.width();
    int row = (vpPos.y() + scrollY) / m_cellSize.height();

    
    
    if (col < 0 || col >= m_model->targetNumbers().size() ||
        row < 0 || row >= m_model->sourceNumbers().size()) {
        return QPoint(-1, -1);
    }

    return QPoint(col, row);
}

QRect VirtualizedMatrixWidget::cellRect(int row, int col) const
{
    if (!m_model) return QRect();

    
    int scrollX = horizontalScrollBar()->value();
    int scrollY = verticalScrollBar()->value();

    int x = col * m_cellSize.width() - scrollX;
    int y = row * m_cellSize.height() - scrollY;

    return QRect(x, y, m_cellSize.width(), m_cellSize.height());
}

void VirtualizedMatrixWidget::paintEvent(QPaintEvent *event)
{
    if (!m_model) {
        QPainter painter(viewport());
        painter.fillRect(viewport()->rect(), palette().base());
        painter.setPen(palette().text().color());
        painter.drawText(viewport()->rect(), Qt::AlignCenter, 
                        "No matrix data loaded");
        return;
    }
    
    
    if (m_model->targetCount() == 0 || m_model->sourceCount() == 0) {
        QPainter painter(viewport());
        painter.fillRect(viewport()->rect(), palette().base());
        painter.setPen(palette().text().color());
        painter.drawText(viewport()->rect(), Qt::AlignCenter, 
                        QString("Empty matrix: %1×%2")
                        .arg(m_model->sourceCount())
                        .arg(m_model->targetCount()));
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false); 
    
    
    QRect updateRect = event->rect();
    painter.setClipRect(updateRect);

    
    painter.fillRect(updateRect, palette().base());

    
    QRect visibleCells = visibleCellsRect();

    
    drawConnections(painter, visibleCells);
    drawGrid(painter, visibleCells);
    drawHover(painter);
    drawSelection(painter);
}

void VirtualizedMatrixWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateViewportSize();
}

void VirtualizedMatrixWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_model || !m_crosspointsEnabled) return;

    if (event->button() == Qt::LeftButton) {
        QPoint cell = cellAt(event->pos());
        if (cell.x() >= 0 && cell.y() >= 0) {
            m_selectedCell = cell;
            
            
            
            const QList<int> &targets = m_model->targetNumbers();
            const QList<int> &sources = m_model->sourceNumbers();
            
            int targetNumber = targets[cell.x()];
            int sourceNumber = sources[cell.y()];
            
            
            emit crosspointClicked(targetNumber, sourceNumber);
            emit crosspointClicked(m_matrixPath, targetNumber, sourceNumber);
            viewport()->update();
        }
    }

    QAbstractScrollArea::mousePressEvent(event);
}

void VirtualizedMatrixWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_model) return;

    QPoint cell = cellAt(event->pos());
    if (cell != m_hoveredCell) {
        m_hoveredCell = cell;
        
        
        m_headerView->setHighlightedColumn(cell.x());
        m_sidebarView->setHighlightedRow(cell.y());
        
        if (cell.x() >= 0 && cell.y() >= 0) {
            
            const QList<int> &targets = m_model->targetNumbers();
            const QList<int> &sources = m_model->sourceNumbers();
            
            int targetNumber = targets[cell.x()];
            int sourceNumber = sources[cell.y()];
            
            
            QString targetLabel = m_model->targetLabel(targetNumber);
            QString sourceLabel = m_model->sourceLabel(sourceNumber);
            bool connected = m_model->isConnected(targetNumber, sourceNumber);
            
            QString tooltip = QString("<b>Target:</b> %1<br><b>Source:</b> %2<br><b>Status:</b> %3")
                .arg(targetLabel)
                .arg(sourceLabel)
                .arg(connected ? "Connected" : "Disconnected");
            
            viewport()->setToolTip(tooltip);
            
            emit crosspointHovered(targetNumber, sourceNumber);
        } else {
            viewport()->setToolTip("");
        }
        
        viewport()->update();
    }

    QAbstractScrollArea::mouseMoveEvent(event);
}

void VirtualizedMatrixWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void VirtualizedMatrixWidget::leaveEvent(QEvent *event)
{
    m_hoveredCell = QPoint(-1, -1);
    m_headerView->setHighlightedColumn(-1);
    m_sidebarView->setHighlightedRow(-1);
    viewport()->update();
    QAbstractScrollArea::leaveEvent(event);
}

void VirtualizedMatrixWidget::wheelEvent(QWheelEvent *event)
{
    
    QAbstractScrollArea::wheelEvent(event);
}

void VirtualizedMatrixWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_model || m_selectedCell.x() < 0 || m_selectedCell.y() < 0) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    int maxCol = m_model->sourceNumbers().size() - 1;
    int maxRow = m_model->targetNumbers().size() - 1;
    QPoint newCell = m_selectedCell;

    switch (event->key()) {
        case Qt::Key_Left:
            newCell.setX(qMax(0, newCell.x() - 1));
            break;
        case Qt::Key_Right:
            newCell.setX(qMin(maxCol, newCell.x() + 1));
            break;
        case Qt::Key_Up:
            newCell.setY(qMax(0, newCell.y() - 1));
            break;
        case Qt::Key_Down:
            newCell.setY(qMin(maxRow, newCell.y() + 1));
            break;
        case Qt::Key_Home:
            if (event->modifiers() & Qt::ControlModifier) {
                newCell = QPoint(0, 0);
            } else {
                newCell.setX(0);
            }
            break;
        case Qt::Key_End:
            if (event->modifiers() & Qt::ControlModifier) {
                newCell = QPoint(maxCol, maxRow);
            } else {
                newCell.setX(maxCol);
            }
            break;
        default:
            QAbstractScrollArea::keyPressEvent(event);
            return;
    }

    if (newCell != m_selectedCell) {
        m_selectedCell = newCell;
        
        
        QRect cellRect = this->cellRect(newCell.y(), newCell.x());
        
        
        int scrollX = horizontalScrollBar()->value();
        int scrollY = verticalScrollBar()->value();
        
        if (cellRect.right() > viewport()->width()) {
            scrollX = (newCell.x() + 1) * m_cellSize.width() - viewport()->width();
        } else if (cellRect.left() < 0) {
            scrollX = newCell.x() * m_cellSize.width();
        }
        
        if (cellRect.bottom() > viewport()->height()) {
            scrollY = (newCell.y() + 1) * m_cellSize.height() - viewport()->height();
        } else if (cellRect.top() < 0) {
            scrollY = newCell.y() * m_cellSize.height();
        }
        
        horizontalScrollBar()->setValue(scrollX);
        verticalScrollBar()->setValue(scrollY);
        
        viewport()->update();
        
        
        const QList<int> &targets = m_model->targetNumbers();
        const QList<int> &sources = m_model->sourceNumbers();
        emit crosspointClicked(targets[newCell.y()], sources[newCell.x()]);
    }
}

void VirtualizedMatrixWidget::onModelDataChanged()
{
    updateScrollBars();
    viewport()->update();
}

void VirtualizedMatrixWidget::onModelConnectionChanged(int targetNumber, int sourceNumber, bool connected)
{
    Q_UNUSED(connected);
    
    
    invalidateCell(targetNumber, sourceNumber);
}

void VirtualizedMatrixWidget::updateScrollBars()
{
    if (!m_model) {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
        return;
    }

    int totalWidth = m_model->sourceNumbers().size() * m_cellSize.width();
    int totalHeight = m_model->targetNumbers().size() * m_cellSize.height();

    int maxScrollX = qMax(0, totalWidth - viewport()->width());
    int maxScrollY = qMax(0, totalHeight - viewport()->height());

    horizontalScrollBar()->setRange(0, maxScrollX);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setSingleStep(m_cellSize.width());

    verticalScrollBar()->setRange(0, maxScrollY);
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(m_cellSize.height());
}

void VirtualizedMatrixWidget::updateViewportSize()
{
    
    setViewportMargins(m_sidebarWidth, m_headerHeight, 0, 0);
    
    QRect vp = viewport()->geometry();
    
    
    m_headerView->setGeometry(m_sidebarWidth, 0, 
                              vp.width(), m_headerHeight);
    
    
    m_sidebarView->setGeometry(0, m_headerHeight,
                               m_sidebarWidth, vp.height());
    
    
    m_cornerWidget->setGeometry(0, 0, m_sidebarWidth, m_headerHeight);
    
    updateScrollBars();
}

QRect VirtualizedMatrixWidget::visibleCellsRect() const
{
    if (!m_model) return QRect();

    int scrollX = horizontalScrollBar()->value();
    int scrollY = verticalScrollBar()->value();

    int firstCol = scrollX / m_cellSize.width();
    int firstRow = scrollY / m_cellSize.height();

    int lastCol = (scrollX + viewport()->width() - 1) / m_cellSize.width();
    int lastRow = (scrollY + viewport()->height() - 1) / m_cellSize.height();

    
    lastCol = qMin(lastCol, m_model->sourceNumbers().size() - 1);
    lastRow = qMin(lastRow, m_model->targetNumbers().size() - 1);

    return QRect(firstCol, firstRow, lastCol - firstCol + 1, lastRow - firstRow + 1);
}

void VirtualizedMatrixWidget::drawGrid(QPainter &painter, const QRect &visibleCells)
{
    painter.setPen(QPen(palette().mid().color(), 1));

    
    for (int col = visibleCells.left(); col <= visibleCells.right() + 1; ++col) {
        int x = col * m_cellSize.width() - horizontalScrollBar()->value();
        painter.drawLine(x, 0, x, viewport()->height());
    }

    
    for (int row = visibleCells.top(); row <= visibleCells.bottom() + 1; ++row) {
        int y = row * m_cellSize.height() - verticalScrollBar()->value();
        painter.drawLine(0, y, viewport()->width(), y);
    }
}

void VirtualizedMatrixWidget::drawConnections(QPainter &painter, const QRect &visibleCells)
{
    if (!m_model) return;

    
    const QList<int> &targets = m_model->targetNumbers();
    const QList<int> &sources = m_model->sourceNumbers();

    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(120, 255, 120, 130)); 

    for (int row = visibleCells.top(); row <= visibleCells.bottom(); ++row) {
        for (int col = visibleCells.left(); col <= visibleCells.right(); ++col) {
            int targetNumber = targets[col];
            int sourceNumber = sources[row];

            if (m_model->isConnected(targetNumber, sourceNumber)) {
                QRect rect = cellRect(row, col);
                painter.fillRect(rect, painter.brush());
            }
        }
    }
}

void VirtualizedMatrixWidget::drawSelection(QPainter &painter)
{
    if (m_selectedCell.x() < 0 || m_selectedCell.y() < 0) return;

    QRect rect = cellRect(m_selectedCell.y(), m_selectedCell.x());
    
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(1, 1, -1, -1));
}

void VirtualizedMatrixWidget::drawHover(QPainter &painter)
{
    if (m_hoveredCell.x() < 0 || m_hoveredCell.y() < 0) return;
    if (m_hoveredCell == m_selectedCell) return;

    QRect rect = cellRect(m_hoveredCell.y(), m_hoveredCell.x());
    
    painter.setPen(QPen(palette().highlight().color(), 1, Qt::DashLine));
    QColor hoverColor = palette().highlight().color();
    hoverColor.setAlpha(30);
    painter.setBrush(hoverColor);
    painter.drawRect(rect.adjusted(1, 1, -1, -1));
}

void VirtualizedMatrixWidget::invalidateCellRegion(int row, int col)
{
    if (row < 0 || col < 0) return;
    
    QRect cellGeometry = cellRect(row, col);
    
    
    if (cellGeometry.intersects(viewport()->rect())) {
        viewport()->update(cellGeometry);
    }
}

void VirtualizedMatrixWidget::invalidateCell(int targetNumber, int sourceNumber)
{
    if (!m_model) return;

    
    const QList<int> &targets = m_model->targetNumbers();
    const QList<int> &sources = m_model->sourceNumbers();

    int col = targets.indexOf(targetNumber);
    int row = sources.indexOf(sourceNumber);
    
    if (row >= 0 && col >= 0) {
        invalidateCellRegion(row, col);
    }
}


void VirtualizedMatrixWidget::setPreferredHeaderHeight(int height)
{
    s_preferredHeaderHeight = height;
}

void VirtualizedMatrixWidget::setPreferredSidebarWidth(int width)
{
    s_preferredSidebarWidth = width;
}

int VirtualizedMatrixWidget::preferredHeaderHeight()
{
    return s_preferredHeaderHeight;
}

int VirtualizedMatrixWidget::preferredSidebarWidth()
{
    return s_preferredSidebarWidth;
}




#include "VirtualizedSidebarView.h"
#include "MatrixModel.h"
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>

VirtualizedSidebarView::VirtualizedSidebarView(int cellHeight, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_cellHeight(cellHeight)
    , m_scrollOffset(0)
    , m_highlightedRow(-1)
    , m_crosspointsEnabled(false)
    , m_isResizing(false)
    , m_resizeStartX(0)
    , m_resizeStartWidth(0)
{
    setMinimumWidth(MIN_SIDEBAR_WIDTH);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMouseTracking(true);  
}

VirtualizedSidebarView::~VirtualizedSidebarView()
{
}

void VirtualizedSidebarView::setModel(MatrixModel *model)
{
    m_model = model;
    update();
}

void VirtualizedSidebarView::setCellHeight(int height)
{
    m_cellHeight = height;
    update();
}

void VirtualizedSidebarView::setScrollOffset(int offset)
{
    m_scrollOffset = offset;
    update();
}

void VirtualizedSidebarView::setHighlightedRow(int row)
{
    if (m_highlightedRow != row) {
        m_highlightedRow = row;
        update();
    }
}

void VirtualizedSidebarView::setCrosspointsEnabled(bool enabled)
{
    if (m_crosspointsEnabled != enabled) {
        m_crosspointsEnabled = enabled;
        update();
    }
}

QSize VirtualizedSidebarView::sizeHint() const
{
    return QSize(80, height());
}

QSize VirtualizedSidebarView::minimumSizeHint() const
{
    return QSize(80, 0);
}

void VirtualizedSidebarView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    
    
    QColor bgColor = m_crosspointsEnabled ? QColor("#890000") : palette().button().color();
    painter.fillRect(rect(), bgColor);

    if (!m_model) return;

    const QList<int> &sources = m_model->sourceNumbers();
    if (sources.isEmpty()) return;

    
    int firstRow = m_scrollOffset / m_cellHeight;
    int lastRow = (m_scrollOffset + height() - 1) / m_cellHeight;
    lastRow = qMin(lastRow, sources.size() - 1);

    
    painter.setPen(palette().buttonText().color());
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int row = firstRow; row <= lastRow; ++row) {
        int sourceNumber = sources[row];
        QString label = m_model->sourceLabel(sourceNumber);

        int y = row * m_cellHeight - m_scrollOffset;
        QRect cellRect(0, y, width(), m_cellHeight);

        
        if (row == m_highlightedRow) {
            QColor highlightColor = palette().highlight().color();
            highlightColor.setAlpha(50);
            painter.fillRect(cellRect, highlightColor);
        }

        
        painter.setPen(palette().mid().color());
        painter.drawLine(cellRect.bottomLeft(), cellRect.bottomRight());

        
        painter.setPen(palette().buttonText().color());
        
        QFontMetrics fm(painter.font());
        QString elidedText = fm.elidedText(label, Qt::ElideRight, width() - 8);
        
        painter.drawText(cellRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, elidedText);
    }

    
    painter.setPen(palette().dark().color());
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

bool VirtualizedSidebarView::isInResizeZone(int x) const
{
    return x >= width() - RESIZE_HANDLE_WIDTH && x <= width();
}

void VirtualizedSidebarView::updateCursor(int x)
{
    if (isInResizeZone(x)) {
        setCursor(Qt::SizeHorCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void VirtualizedSidebarView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInResizeZone(event->pos().x())) {
        m_isResizing = true;
        m_resizeStartX = event->globalPosition().toPoint().x();
        m_resizeStartWidth = width();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void VirtualizedSidebarView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing) {
        int deltaX = event->globalPosition().toPoint().x() - m_resizeStartX;
        int newWidth = m_resizeStartWidth + deltaX;
        
        
        newWidth = qMax(MIN_SIDEBAR_WIDTH, qMin(MAX_SIDEBAR_WIDTH, newWidth));
        
        
        emit sidebarWidthChanged(newWidth);
        
        event->accept();
    } else {
        
        updateCursor(event->pos().x());
        QWidget::mouseMoveEvent(event);
    }
}

void VirtualizedSidebarView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isResizing) {
        m_isResizing = false;
        updateCursor(event->pos().x());
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}


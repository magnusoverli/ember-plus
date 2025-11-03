

#include "VirtualizedHeaderView.h"
#include "MatrixModel.h"
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>

VirtualizedHeaderView::VirtualizedHeaderView(int cellWidth, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_cellWidth(cellWidth)
    , m_scrollOffset(0)
    , m_highlightedCol(-1)
    , m_crosspointsEnabled(false)
    , m_isResizing(false)
    , m_resizeStartY(0)
    , m_resizeStartHeight(0)
{
    setMinimumHeight(MIN_HEADER_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);  
}

VirtualizedHeaderView::~VirtualizedHeaderView()
{
}

void VirtualizedHeaderView::setModel(MatrixModel *model)
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    
    m_model = model;
    
    if (m_model) {
        connect(m_model, &MatrixModel::dataChanged, this, [this]() {
            update();
        });
    }
    
    update();
}

void VirtualizedHeaderView::setCellWidth(int width)
{
    m_cellWidth = width;
    update();
}

void VirtualizedHeaderView::setScrollOffset(int offset)
{
    m_scrollOffset = offset;
    update();
}

void VirtualizedHeaderView::setHighlightedColumn(int col)
{
    if (m_highlightedCol != col) {
        m_highlightedCol = col;
        update();
    }
}

void VirtualizedHeaderView::setCrosspointsEnabled(bool enabled)
{
    if (m_crosspointsEnabled != enabled) {
        m_crosspointsEnabled = enabled;
        update();
    }
}

QSize VirtualizedHeaderView::sizeHint() const
{
    return QSize(width(), 30);
}

QSize VirtualizedHeaderView::minimumSizeHint() const
{
    return QSize(0, 30);
}

void VirtualizedHeaderView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    
    
    QColor bgColor = m_crosspointsEnabled ? QColor("#890000") : palette().button().color();
    painter.fillRect(rect(), bgColor);

    if (!m_model) return;

    const QList<int> &targets = m_model->targetNumbers();
    if (targets.isEmpty()) return;

    
    int firstCol = m_scrollOffset / m_cellWidth;
    int lastCol = (m_scrollOffset + width() - 1) / m_cellWidth;
    lastCol = qMin(lastCol, targets.size() - 1);

    
    painter.setPen(palette().buttonText().color());
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int col = firstCol; col <= lastCol; ++col) {
        int targetNumber = targets[col];
        QString label = m_model->targetLabel(targetNumber);

        int x = col * m_cellWidth - m_scrollOffset;
        QRect cellRect(x, 0, m_cellWidth, height());

        
        if (col == m_highlightedCol) {
            QColor highlightColor = palette().highlight().color();
            highlightColor.setAlpha(50);
            painter.fillRect(cellRect, highlightColor);
        }

        
        painter.setPen(palette().mid().color());
        painter.drawLine(cellRect.topRight(), cellRect.bottomRight());

        
        painter.save();
        painter.setPen(palette().buttonText().color());
        
        
        painter.translate(cellRect.center());
        painter.rotate(-90);
        
        QFontMetrics fm(painter.font());
        QString elidedText = fm.elidedText(label, Qt::ElideRight, height() - 4);
        
        QRect textRect(-height() / 2, -m_cellWidth / 2, height() - 4, m_cellWidth);
        painter.drawText(textRect, Qt::AlignCenter, elidedText);
        
        painter.restore();
    }

    
    painter.setPen(palette().dark().color());
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

bool VirtualizedHeaderView::isInResizeZone(int y) const
{
    return y >= height() - RESIZE_HANDLE_HEIGHT && y <= height();
}

void VirtualizedHeaderView::updateCursor(int y)
{
    if (isInResizeZone(y)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void VirtualizedHeaderView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInResizeZone(event->pos().y())) {
        m_isResizing = true;
        m_resizeStartY = event->globalPosition().toPoint().y();
        m_resizeStartHeight = height();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void VirtualizedHeaderView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing) {
        int deltaY = event->globalPosition().toPoint().y() - m_resizeStartY;
        int newHeight = m_resizeStartHeight + deltaY;
        
        
        newHeight = qMax(MIN_HEADER_HEIGHT, qMin(MAX_HEADER_HEIGHT, newHeight));
        
        
        emit headerHeightChanged(newHeight);
        
        event->accept();
    } else {
        
        updateCursor(event->pos().y());
        QWidget::mouseMoveEvent(event);
    }
}

void VirtualizedHeaderView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isResizing) {
        m_isResizing = false;
        updateCursor(event->pos().y());
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}


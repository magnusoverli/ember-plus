/*
    VirtualizedSidebarView.cpp - Implementation of virtualized vertical sidebar
*/

#include "VirtualizedSidebarView.h"
#include "MatrixModel.h"
#include <QPainter>
#include <QFontMetrics>

VirtualizedSidebarView::VirtualizedSidebarView(int cellHeight, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_cellHeight(cellHeight)
    , m_scrollOffset(0)
    , m_highlightedRow(-1)
    , m_crosspointsEnabled(false)
{
    setMinimumWidth(80);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
    
    // Use red background when crosspoints enabled, otherwise default button color
    QColor bgColor = m_crosspointsEnabled ? QColor("#890000") : palette().button().color();
    painter.fillRect(rect(), bgColor);

    if (!m_model) return;

    const QList<int> &targets = m_model->targetNumbers();
    if (targets.isEmpty()) return;

    // Calculate visible range
    int firstRow = m_scrollOffset / m_cellHeight;
    int lastRow = (m_scrollOffset + height() - 1) / m_cellHeight;
    lastRow = qMin(lastRow, targets.size() - 1);

    // Draw labels
    painter.setPen(palette().buttonText().color());
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int row = firstRow; row <= lastRow; ++row) {
        int targetNumber = targets[row];
        QString label = m_model->targetLabel(targetNumber);

        int y = row * m_cellHeight - m_scrollOffset;
        QRect cellRect(0, y, width(), m_cellHeight);

        // Highlight if this is the hovered row
        if (row == m_highlightedRow) {
            QColor highlightColor = palette().highlight().color();
            highlightColor.setAlpha(50);
            painter.fillRect(cellRect, highlightColor);
        }

        // Draw separator
        painter.setPen(palette().mid().color());
        painter.drawLine(cellRect.bottomLeft(), cellRect.bottomRight());

        // Draw text (elided if too long)
        painter.setPen(palette().buttonText().color());
        
        QFontMetrics fm(painter.font());
        QString elidedText = fm.elidedText(label, Qt::ElideRight, width() - 8);
        
        painter.drawText(cellRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, elidedText);
    }

    // Draw right border
    painter.setPen(palette().dark().color());
    painter.drawLine(width() - 1, 0, width() - 1, height());
}


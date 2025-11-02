/*
    VirtualizedHeaderView.cpp - Implementation of virtualized horizontal header
*/

#include "VirtualizedHeaderView.h"
#include "MatrixModel.h"
#include <QPainter>
#include <QFontMetrics>

VirtualizedHeaderView::VirtualizedHeaderView(int cellWidth, QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_cellWidth(cellWidth)
    , m_scrollOffset(0)
    , m_highlightedCol(-1)
{
    setMinimumHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

VirtualizedHeaderView::~VirtualizedHeaderView()
{
}

void VirtualizedHeaderView::setModel(MatrixModel *model)
{
    m_model = model;
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
    painter.fillRect(rect(), palette().button());

    if (!m_model) return;

    const QList<int> &sources = m_model->sourceNumbers();
    if (sources.isEmpty()) return;

    // Calculate visible range
    int firstCol = m_scrollOffset / m_cellWidth;
    int lastCol = (m_scrollOffset + width() - 1) / m_cellWidth;
    lastCol = qMin(lastCol, sources.size() - 1);

    // Draw labels
    painter.setPen(palette().buttonText().color());
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int col = firstCol; col <= lastCol; ++col) {
        int sourceNumber = sources[col];
        QString label = m_model->sourceLabel(sourceNumber);

        int x = col * m_cellWidth - m_scrollOffset;
        QRect cellRect(x, 0, m_cellWidth, height());

        // Highlight if this is the hovered column
        if (col == m_highlightedCol) {
            QColor highlightColor = palette().highlight().color();
            highlightColor.setAlpha(50);
            painter.fillRect(cellRect, highlightColor);
        }

        // Draw separator
        painter.setPen(palette().mid().color());
        painter.drawLine(cellRect.topRight(), cellRect.bottomRight());

        // Draw text (rotated 90 degrees for space efficiency)
        painter.save();
        painter.setPen(palette().buttonText().color());
        
        // Rotate text
        painter.translate(cellRect.center());
        painter.rotate(-90);
        
        QFontMetrics fm(painter.font());
        QString elidedText = fm.elidedText(label, Qt::ElideRight, height() - 4);
        
        QRect textRect(-height() / 2, -m_cellWidth / 2, height() - 4, m_cellWidth);
        painter.drawText(textRect, Qt::AlignCenter, elidedText);
        
        painter.restore();
    }

    // Draw bottom border
    painter.setPen(palette().dark().color());
    painter.drawLine(0, height() - 1, width(), height() - 1);
}


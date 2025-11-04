#include "BusySpinner.h"
#include <QPainter>
#include <QPaintEvent>

BusySpinner::BusySpinner(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_angle(0)
    , m_spinning(false)
    , m_color(QColor(29, 118, 210))  // Material blue color (#1976d2)
{
    m_timer->setInterval(ROTATION_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &BusySpinner::rotate);
    
    setFixedSize(WIDGET_SIZE, WIDGET_SIZE);
}

BusySpinner::~BusySpinner()
{
}

QSize BusySpinner::sizeHint() const
{
    return QSize(WIDGET_SIZE, WIDGET_SIZE);
}

QSize BusySpinner::minimumSizeHint() const
{
    return QSize(WIDGET_SIZE, WIDGET_SIZE);
}

void BusySpinner::start()
{
    if (!m_spinning) {
        m_spinning = true;
        m_angle = 0;
        setVisible(true);
        // Use QTimer::singleShot to defer the timer start until after the widget is shown
        QTimer::singleShot(0, this, [this]() {
            if (m_spinning && m_timer) {
                m_timer->start();
            }
        });
    }
}

void BusySpinner::stop()
{
    if (m_spinning) {
        m_spinning = false;
        m_timer->stop();
        setVisible(false);
    }
}

void BusySpinner::rotate()
{
    m_angle = (m_angle + ANGLE_STEP) % 360;
    update();
}

void BusySpinner::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (!m_spinning) {
        return;
    }
    
    // Safety check: don't paint if widget has zero size
    if (width() <= 0 || height() <= 0) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int side = qMin(width(), height());
    painter.translate(width() / 2, height() / 2);
    painter.scale(side / 20.0, side / 20.0);
    
    // Draw 8 dots in a circle with varying opacity based on rotation
    for (int i = 0; i < 8; ++i) {
        QColor color = m_color;
        
        // Calculate opacity based on dot position relative to current angle
        int dotAngle = i * 45;
        int angleDiff = (m_angle - dotAngle + 360) % 360;
        
        // Dots ahead of rotation are more transparent
        qreal opacity = 1.0 - (angleDiff / 360.0);
        opacity = qMax(0.15, opacity);  // Minimum opacity
        
        color.setAlphaF(opacity);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        
        painter.save();
        painter.rotate(dotAngle);
        painter.translate(0, -7);
        painter.drawEllipse(QRectF(-1.5, -1.5, 3, 3));
        painter.restore();
    }
}

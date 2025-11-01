







#include "MeterWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cmath>

MeterWidget::MeterWidget(QWidget *parent)
    : QWidget(parent)
    , m_streamIdentifier(-1)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_currentValue(0.0)
    , m_displayValue(0.0)
    , m_peakValue(0.0)
    , m_needsRedraw(false)
    , m_lastUpdateTime(0)
{
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_valueLabel = new QLabel("-- dB", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet("font-size: 14pt; font-weight: bold;");
    headerLayout->addStretch();
    headerLayout->addWidget(m_valueLabel);
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);
    mainLayout->addStretch();
    
    
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &MeterWidget::onUpdateTimer);
    m_updateTimer->start();
    
    setMinimumSize(METER_WIDTH + METER_MARGIN * 2, 200);
}

MeterWidget::~MeterWidget()
{
}

void MeterWidget::setParameterInfo(const QString &identifier, const QString &path, 
                                   double minValue, double maxValue)
{
    m_identifier = identifier;
    m_parameterPath = path;
    m_minValue = minValue;
    m_maxValue = maxValue;
    
    
    m_currentValue = minValue;
    m_displayValue = minValue;
    m_peakValue = minValue;
    
    update();
}

void MeterWidget::updateValue(double value)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    
    if (now - m_lastUpdateTime < UPDATE_INTERVAL_MS / 2) {
        
        m_currentValue = value;
        m_needsRedraw = true;
        return;
    }
    
    m_currentValue = value;
    m_lastUpdateTime = now;
    m_needsRedraw = true;
    
    
    updatePeakHold();
    
    
    m_valueLabel->setText(formatValue(value));
}

void MeterWidget::onUpdateTimer()
{
    if (m_needsRedraw) {
        
        double alpha = 0.3;  
        m_displayValue = alpha * m_currentValue + (1.0 - alpha) * m_displayValue;
        
        update();  
        m_needsRedraw = false;
    }
    
    
    if (m_peakTime.isValid() && 
        m_peakTime.msecsTo(QDateTime::currentDateTime()) > PEAK_HOLD_MS) {
        update();  
    }
}

void MeterWidget::updatePeakHold()
{
    if (m_currentValue > m_peakValue) {
        m_peakValue = m_currentValue;
        m_peakTime = QDateTime::currentDateTime();
    }
}

double MeterWidget::normalizeValue(double value) const
{
    if (m_maxValue <= m_minValue) {
        return 0.0;
    }
    
    double normalized = (value - m_minValue) / (m_maxValue - m_minValue);
    return qBound(0.0, normalized, 1.0);
}

QColor MeterWidget::getColorForLevel(double normalizedLevel) const
{
    if (normalizedLevel >= YELLOW_THRESHOLD) {
        return QColor(255, 0, 0);  
    } else if (normalizedLevel >= GREEN_THRESHOLD) {
        return QColor(255, 200, 0);  
    } else {
        return QColor(0, 200, 0);  
    }
}

QString MeterWidget::formatValue(double value) const
{
    
    if (m_minValue < 0 && m_maxValue <= 20) {
        return QString("%1 dB").arg(value, 0, 'f', 1);
    } else {
        return QString("%1").arg(value, 0, 'f', 2);
    }
}

void MeterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawMeter(painter);
}

void MeterWidget::drawMeter(QPainter &painter)
{
    
    int meterHeight = height() - 80;  
    int meterX = (width() - METER_WIDTH) / 2;
    int meterY = 60;
    
    QRect meterRect(meterX, meterY, METER_WIDTH, meterHeight);
    
    
    painter.fillRect(meterRect, QColor(40, 40, 40));
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(meterRect);
    
    
    double normalizedValue = normalizeValue(m_displayValue);
    int fillHeight = static_cast<int>(normalizedValue * meterHeight);
    
    
    int greenHeight = static_cast<int>(GREEN_THRESHOLD * meterHeight);
    int yellowHeight = static_cast<int>((YELLOW_THRESHOLD - GREEN_THRESHOLD) * meterHeight);
    int redHeight = meterHeight - greenHeight - yellowHeight;
    
    if (fillHeight > 0) {
        
        int fillY = meterY + meterHeight - fillHeight;
        
        
        int currentY = meterY + meterHeight;
        int remainingHeight = fillHeight;
        
        
        if (remainingHeight > 0) {
            int drawHeight = qMin(remainingHeight, greenHeight);
            QRect greenRect(meterX, currentY - drawHeight, METER_WIDTH, drawHeight);
            painter.fillRect(greenRect, QColor(0, 200, 0));
            currentY -= drawHeight;
            remainingHeight -= drawHeight;
        }
        
        
        if (remainingHeight > 0) {
            int drawHeight = qMin(remainingHeight, yellowHeight);
            QRect yellowRect(meterX, currentY - drawHeight, METER_WIDTH, drawHeight);
            painter.fillRect(yellowRect, QColor(255, 200, 0));
            currentY -= drawHeight;
            remainingHeight -= drawHeight;
        }
        
        
        if (remainingHeight > 0) {
            int drawHeight = remainingHeight;
            QRect redRect(meterX, currentY - drawHeight, METER_WIDTH, drawHeight);
            painter.fillRect(redRect, QColor(255, 0, 0));
        }
    }
    
    
    if (m_peakTime.isValid() && 
        m_peakTime.msecsTo(QDateTime::currentDateTime()) <= PEAK_HOLD_MS) {
        double normalizedPeak = normalizeValue(m_peakValue);
        int peakY = meterY + meterHeight - static_cast<int>(normalizedPeak * meterHeight);
        
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(meterX, peakY, meterX + METER_WIDTH, peakY);
    }
    
    
    painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
    int greenLine = meterY + meterHeight - greenHeight;
    int yellowLine = meterY + meterHeight - greenHeight - yellowHeight;
    painter.drawLine(meterX, greenLine, meterX + METER_WIDTH, greenLine);
    painter.drawLine(meterX, yellowLine, meterX + METER_WIDTH, yellowLine);
}

void MeterWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}


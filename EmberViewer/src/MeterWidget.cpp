







#include "MeterWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <cmath>

MeterWidget::MeterWidget(QWidget *parent)
    : QWidget(parent)
    , m_streamIdentifier(-1)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_targetValue(0.0)
    , m_displayValue(0.0)
    , m_peakValue(0.0)
    , m_lastRenderTime(QDateTime::currentMSecsSinceEpoch())
    , m_meterType(MeterType::VU_METER)  // Default to VU meter (similar to old behavior)
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
    
    // Add stretch to push meter bar to center and dropdown to bottom
    mainLayout->addStretch();
    
    // Meter type selector at the bottom
    QHBoxLayout *typeLayout = new QHBoxLayout();
    QLabel *typeLabel = new QLabel("Meter Type:", this);
    typeLabel->setStyleSheet("font-size: 9pt;");
    m_meterTypeCombo = new QComboBox(this);
    m_meterTypeCombo->addItem("VU Meter (300ms)");       // Index 0 = VU_METER
    m_meterTypeCombo->addItem("Digital Peak (Instant)"); // Index 1 = DIGITAL_PEAK
    m_meterTypeCombo->addItem("DIN PPM (10ms/1.5s)");    // Index 2 = DIN_PPM
    m_meterTypeCombo->addItem("BBC PPM (4ms/2.8s)");     // Index 3 = BBC_PPM
    m_meterTypeCombo->setCurrentIndex(0);  // Start with VU Meter
    m_meterTypeCombo->setStyleSheet("font-size: 9pt;");
    connect(m_meterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeterWidget::onMeterTypeChanged);
    
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(m_meterTypeCombo);
    typeLayout->addStretch();
    
    mainLayout->addLayout(typeLayout);
    
    
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
    
    
    m_targetValue = minValue;
    m_displayValue = minValue;
    m_peakValue = minValue;
    
    update();
}

void MeterWidget::updateValue(double value)
{
    // Store target value - no processing, ballistics handled in onUpdateTimer()
    m_targetValue = value;
    
    // Update text label immediately for quick feedback
    m_valueLabel->setText(formatValue(value));
    
    // Update peak hold if this is a new peak
    if (value > m_peakValue) {
        m_peakValue = value;
        m_peakTime = QDateTime::currentDateTime();
    }
}

void MeterWidget::onUpdateTimer()
{
    // Calculate time elapsed since last render
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double dt = (now - m_lastRenderTime) / 1000.0;  // Convert to seconds
    m_lastRenderTime = now;
    
    // Guard against extreme dt values
    if (dt < 0.001) dt = 0.001;  // Minimum 1ms
    if (dt > 1.0) dt = 1.0;      // Maximum 1 second (app pause/suspend)
    
    // Get time constants for current meter type
    double riseTime, fallTime;
    getMeterConstants(m_meterType, riseTime, fallTime);
    
    // Time-domain exponential approach to target
    double tau;
    if (m_targetValue > m_displayValue) {
        // Rising: Fast response for peaks
        tau = riseTime;
    } else {
        // Falling: Decay time
        tau = fallTime;
    }
    
    // Calculate alpha based on actual elapsed time
    double alpha = 1.0 - std::exp(-dt / tau);
    
    // Exponential approach to target
    m_displayValue += alpha * (m_targetValue - m_displayValue);
    
    // Always trigger repaint
    update();
}

void MeterWidget::onMeterTypeChanged(int index)
{
    // Map combo box index to MeterType enum
    switch (index) {
        case 0: m_meterType = MeterType::VU_METER; break;
        case 1: m_meterType = MeterType::DIGITAL_PEAK; break;
        case 2: m_meterType = MeterType::DIN_PPM; break;
        case 3: m_meterType = MeterType::BBC_PPM; break;
        default: m_meterType = MeterType::VU_METER; break;
    }
}

void MeterWidget::getMeterConstants(MeterType type, double &riseTime, double &fallTime) const
{
    switch (type) {
        case MeterType::DIN_PPM:
            // DIN 45406 - Fast attack, slow decay
            riseTime = 0.010;   // 10ms
            fallTime = 1.500;   // 1.5s
            break;
            
        case MeterType::BBC_PPM:
            // IEC 60268-10 Type IIa - Very fast attack, slower decay
            riseTime = 0.004;   // 4ms
            fallTime = 2.800;   // 2.8s
            break;
            
        case MeterType::VU_METER:
            // ANSI C16.5-1942 - Slow symmetric response
            riseTime = 0.300;   // 300ms
            fallTime = 0.300;   // 300ms
            break;
            
        case MeterType::DIGITAL_PEAK:
            // Modern digital - Instant attack, moderate decay
            riseTime = 0.001;   // ~instant (1ms)
            fallTime = 0.500;   // 500ms
            break;
            
        default:
            riseTime = 0.300;
            fallTime = 0.300;
            break;
    }
}

void MeterWidget::getColorZones(MeterType type, double &greenThreshold, double &yellowThreshold) const
{
    switch (type) {
        case MeterType::VU_METER:
            // VU Meter: Green below 0 VU (50%), Red above 0 VU
            // No yellow zone - just green and red
            greenThreshold = 0.50;   // 0 VU is at middle
            yellowThreshold = 1.00;  // No yellow, goes straight to red
            break;
            
        case MeterType::DIN_PPM:
            // DIN PPM: Green (-50 to -20 dBr), Yellow (-20 to 0 dBr), Red (0 to +5 dBr)
            greenThreshold = 0.40;   // -20 dBr
            yellowThreshold = 0.90;  // 0 dBr (reference level)
            break;
            
        case MeterType::BBC_PPM:
            // BBC PPM: Green (PPM 1-4), Yellow (PPM 5-6), Red (PPM 7)
            greenThreshold = 0.67;   // PPM 4 (reference level)
            yellowThreshold = 0.92;  // PPM 6 (permitted peak)
            break;
            
        case MeterType::DIGITAL_PEAK:
            // Digital Peak: Green (-∞ to -18 dBFS), Yellow (-18 to -6 dBFS), Red (-6 to 0 dBFS)
            greenThreshold = 0.70;   // -18 dBFS
            yellowThreshold = 0.90;  // -6 dBFS
            break;
            
        default:
            greenThreshold = 0.75;
            yellowThreshold = 0.90;
            break;
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
    // Get meter-specific color zone thresholds
    double greenThreshold, yellowThreshold;
    getColorZones(m_meterType, greenThreshold, yellowThreshold);
    
    if (normalizedLevel >= yellowThreshold) {
        return QColor(255, 0, 0);  // Red
    } else if (normalizedLevel >= greenThreshold) {
        return QColor(255, 200, 0);  // Yellow
    } else {
        return QColor(0, 200, 0);  // Green
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
    
    // Get meter-specific color zone thresholds
    double greenThreshold, yellowThreshold;
    getColorZones(m_meterType, greenThreshold, yellowThreshold);
    
    int greenHeight = static_cast<int>(greenThreshold * meterHeight);
    int yellowHeight = static_cast<int>((yellowThreshold - greenThreshold) * meterHeight);
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


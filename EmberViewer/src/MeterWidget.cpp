







#include "MeterWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSettings>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QContextMenuEvent>
#include <cmath>
#include <algorithm>

MeterWidget::MeterWidget(QWidget *parent)
    : QWidget(parent)
    , m_streamIdentifier(-1)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_targetValue(0.0)
    , m_displayValue(0.0)
    , m_peakValue(0.0)
    , m_lastRenderTime(QDateTime::currentMSecsSinceEpoch())
    , m_meterType(MeterType::VU_METER)
    , m_customGreenThreshold(-20.0)
    , m_customYellowThreshold(-6.0)
    , m_useCustomThresholds(false)
    , m_useLogarithmicScale(true)  // Default to logarithmic for dB scales
{
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    
    QVBoxLayout *valuesLayout = new QVBoxLayout();
    
    // Current value label
    m_valueLabel = new QLabel("-- dB", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet("font-size: 14pt; font-weight: bold;");
    valuesLayout->addWidget(m_valueLabel);
    
    // Peak hold label
    m_peakLabel = new QLabel("Peak: -- dB", this);
    m_peakLabel->setAlignment(Qt::AlignCenter);
    m_peakLabel->setStyleSheet("font-size: 10pt; color: #ff6666;");
    valuesLayout->addWidget(m_peakLabel);
    
    mainLayout->addLayout(valuesLayout);
    
    // Add stretch to push meter bar to center and dropdown to bottom
    mainLayout->addStretch();
    
    // Add spacing before dropdown to prevent overlap
    mainLayout->addSpacing(10);
    
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
    m_meterTypeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    
    // Load saved threshold settings
    QSettings settings("EmberViewer", "EmberViewer");
    m_useCustomThresholds = settings.value("meter/useCustomThresholds", false).toBool();
    m_customGreenThreshold = settings.value("meter/customGreenThreshold", -20.0).toDouble();
    m_customYellowThreshold = settings.value("meter/customYellowThreshold", -6.0).toDouble();
    m_useLogarithmicScale = settings.value("meter/useLogarithmicScale", true).toBool();
}

MeterWidget::~MeterWidget()
{
}

void MeterWidget::setParameterInfo(const QString &identifier, const QString &path, 
                                   double minValue, double maxValue,
                                   const QString &format, const QString &referenceLevel,
                                   int factor)
{
    m_identifier = identifier;
    m_parameterPath = path;
    m_format = format;
    m_referenceLevel = referenceLevel;
    
    // Detect likely mismatch: parameter range doesn't include negative values but reference is dB
    // Most audio dB scales use negative values (dBFS: -60 to 0, dBr: -50 to +5, etc.)
    // If min >= 0 and we have a dB reference, likely the range is wrong
    bool likelyMismatch = (minValue >= 0.0 && !referenceLevel.isEmpty() && 
                           referenceLevel.contains("dB", Qt::CaseInsensitive));
    
    if (likelyMismatch) {
        qDebug() << "[MeterWidget] Detected non-negative range with dB reference - likely mismatch";
        qDebug() << "[MeterWidget] Original range:" << minValue << "to" << maxValue;
        qDebug() << "[MeterWidget] Reference level:" << referenceLevel;
        qDebug() << "[MeterWidget] Factor:" << factor;
        
        // Use standard audio meter range based on reference level
        if (referenceLevel.contains("dBFS", Qt::CaseInsensitive) || 
            referenceLevel.contains("dBTP", Qt::CaseInsensitive)) {
            // Digital Full Scale: -60 to 0 dBFS
            m_minValue = -60.0;
            m_maxValue = 0.0;
        } else if (referenceLevel.contains("dBr", Qt::CaseInsensitive)) {
            // DIN PPM: -50 to +5 dBr
            m_minValue = -50.0;
            m_maxValue = 5.0;
        } else if (referenceLevel.contains("dBu", Qt::CaseInsensitive)) {
            // Professional line level: -20 to +20 dBu
            m_minValue = -20.0;
            m_maxValue = 20.0;
        } else if (referenceLevel.contains("LUFS", Qt::CaseInsensitive)) {
            // Loudness: -40 to 0 LUFS
            m_minValue = -40.0;
            m_maxValue = 0.0;
        } else {
            // Generic dB: use factor to calculate appropriate range if available
            if (factor > 1) {
                // Factor tells us the conversion: dB = rawValue / factor
                // Typical audio meters use ranges like -80 to +20 dB
                // With factor=32, common raw ranges are -2560 (80dB) to +640 (+20dB)
                // Use a symmetrical range based on factor with reasonable headroom
                double rangeInDb = 2560.0 / factor;  // ±80 dB at factor=32
                m_minValue = -rangeInDb;
                m_maxValue = rangeInDb / 4.0;  // Less headroom on top (typically don't need +80dB)
                
                qDebug() << "[MeterWidget] Calculated range from factor" << factor 
                         << ":" << m_minValue << "to" << m_maxValue << "dB";
            } else {
                // No factor info: use a balanced range around 0 dB
                m_minValue = -10.0;
                m_maxValue = 10.0;
                qDebug() << "[MeterWidget] Using default balanced range";
            }
        }
        
        qDebug() << "[MeterWidget] Overridden to dB range:" << m_minValue << "to" << m_maxValue;
    } else {
        m_minValue = minValue;
        m_maxValue = maxValue;
    }
    
    qDebug() << "[MeterWidget] setParameterInfo called - format:" << format << "referenceLevel:" << referenceLevel << "min:" << m_minValue << "max:" << m_maxValue;
    
    // Set tooltip showing cleaned format string
    if (!format.isEmpty()) {
        QString cleanFormat = format;
        cleanFormat = cleanFormat.replace("\\n", " ").replace("\n", " ").replace("°", "").simplified();
        setToolTip(QString("Format: %1\nRange: %2 to %3 %4")
                   .arg(cleanFormat)
                   .arg(minValue, 0, 'f', 1)
                   .arg(maxValue, 0, 'f', 1)
                   .arg(referenceLevel.isEmpty() ? "dB" : referenceLevel));
    }
    
    // Auto-detect meter type based on multiple heuristics
    bool autoDetected = false;
    
    // First priority: Specific reference level strings
    if (!referenceLevel.isEmpty()) {
        qDebug() << "[MeterWidget] Auto-detecting meter type from reference level:" << referenceLevel;
        
        if (referenceLevel == "dBFS" || referenceLevel == "dBTP") {
            setMeterTypeByIndex(1);  // Digital Peak
            qDebug() << "[MeterWidget] Auto-selected Digital Peak meter for" << referenceLevel;
            autoDetected = true;
        }
        else if (referenceLevel == "dBr") {
            setMeterTypeByIndex(2);  // DIN PPM
            qDebug() << "[MeterWidget] Auto-selected DIN PPM meter for dBr";
            autoDetected = true;
        }
        else if (referenceLevel == "dBu" || referenceLevel == "dBV") {
            setMeterTypeByIndex(0);  // VU Meter (analog levels)
            qDebug() << "[MeterWidget] Auto-selected VU meter for" << referenceLevel;
            autoDetected = true;
        }
        else if (referenceLevel == "LUFS" || referenceLevel == "LU") {
            setMeterTypeByIndex(1);  // Digital Peak (loudness)
            qDebug() << "[MeterWidget] Auto-selected Digital Peak meter for loudness:" << referenceLevel;
            autoDetected = true;
        }
        else if (referenceLevel == "VU") {
            setMeterTypeByIndex(0);  // VU Meter
            qDebug() << "[MeterWidget] Auto-selected VU meter";
            autoDetected = true;
        }
        else if (referenceLevel.contains("PPM")) {
            setMeterTypeByIndex(2);  // DIN PPM
            qDebug() << "[MeterWidget] Auto-selected DIN PPM meter for" << referenceLevel;
            autoDetected = true;
        }
    }
    
    // Second priority: Heuristics based on format string and range
    if (!autoDetected && !referenceLevel.isEmpty() && referenceLevel.contains("dB", Qt::CaseInsensitive)) {
        qDebug() << "[MeterWidget] Using heuristics for generic dB meter - format:" << format 
                 << "range:" << m_minValue << "to" << m_maxValue << "factor:" << factor;
        
        // Check format string for hints
        bool formatHintsPPM = format.contains("°");  // European PPM meters often use degree symbol
        
        // Range-based heuristics
        double range = m_maxValue - m_minValue;
        bool wideRange = (range > 50.0);  // Wide range (e.g., -80 to +20 = 100 dB) suggests PPM
        bool hasPositiveHeadroom = (m_maxValue > 5.0);  // Significant positive headroom suggests PPM
        bool narrowRange = (range < 30.0);  // Narrow range suggests VU meter
        
        // Factor-based heuristics
        bool professionalFactor = (factor == 32 || factor == 64);  // Common in pro audio PPM meters
        
        if (formatHintsPPM || (professionalFactor && wideRange)) {
            // Likely a PPM meter - prefer DIN PPM (more common in Europe)
            setMeterTypeByIndex(2);  // DIN PPM
            qDebug() << "[MeterWidget] Auto-selected DIN PPM based on heuristics:"
                     << "formatHint=" << formatHintsPPM 
                     << "wideRange=" << wideRange
                     << "factor=" << factor;
            autoDetected = true;
        }
        else if (wideRange && hasPositiveHeadroom) {
            // Wide range with positive headroom - likely digital peak meter
            setMeterTypeByIndex(1);  // Digital Peak
            qDebug() << "[MeterWidget] Auto-selected Digital Peak based on range:" << range << "dB";
            autoDetected = true;
        }
        else if (narrowRange) {
            // Narrow range - likely VU meter
            setMeterTypeByIndex(0);  // VU Meter
            qDebug() << "[MeterWidget] Auto-selected VU meter based on narrow range:" << range << "dB";
            autoDetected = true;
        }
    }
    
    if (!autoDetected) {
        qDebug() << "[MeterWidget] No auto-detection match, keeping default (VU Meter)";
    }
    
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
        m_peakLabel->setText(QString("Peak: %1").arg(formatValue(m_peakValue)));
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
    
    
    // Check if peak hold has expired
    if (m_peakTime.isValid() && m_peakTime.msecsTo(QDateTime::currentDateTime()) > PEAK_HOLD_MS) {
        m_peakLabel->setStyleSheet("font-size: 10pt; color: #888888;");  // Gray out expired peak
    } else if (m_peakValue > m_minValue) {
        m_peakLabel->setStyleSheet("font-size: 10pt; color: #ff6666;");  // Red for active peak
    }

    // Always trigger repaint
    update();
}

void MeterWidget::setMeterTypeByIndex(int comboIndex)
{
    // Set both combo box and enum value
    m_meterTypeCombo->blockSignals(true);
    m_meterTypeCombo->setCurrentIndex(comboIndex);
    m_meterTypeCombo->blockSignals(false);
    
    // Map combo box index to MeterType enum
    switch (comboIndex) {
        case 0: m_meterType = MeterType::VU_METER; break;
        case 1: m_meterType = MeterType::DIGITAL_PEAK; break;
        case 2: m_meterType = MeterType::DIN_PPM; break;
        case 3: m_meterType = MeterType::BBC_PPM; break;
        default: m_meterType = MeterType::VU_METER; break;
    }
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
    // Use custom thresholds if enabled
    if (m_useCustomThresholds) {
        greenThreshold = dBToNormalized(m_customGreenThreshold);
        yellowThreshold = dBToNormalized(m_customYellowThreshold);
        qDebug() << "[MeterWidget] Using custom thresholds: green=" << greenThreshold 
                 << "(" << m_customGreenThreshold << " dB), yellow=" << yellowThreshold 
                 << "(" << m_customYellowThreshold << " dB)";
        return;
    }
    
    // If we have a dB scale with known reference level, use absolute dB thresholds
    if (isDatabaseScale()) {
        if (m_referenceLevel == "dBFS" || m_referenceLevel == "dBTP") {
            // Digital Full Scale: Industry standard thresholds
            // Green: below -20 dBFS, Yellow: -20 to -6 dBFS, Red: above -6 dBFS
            greenThreshold = dBToNormalized(-20.0);
            yellowThreshold = dBToNormalized(-6.0);
            qDebug() << "[MeterWidget] Using dBFS thresholds: green=" << greenThreshold 
                     << "(-20 dBFS), yellow=" << yellowThreshold << "(-6 dBFS)";
            return;
        }
        else if (m_referenceLevel == "dBr") {
            // DIN PPM reference level
            // Green: below -9 dBr, Yellow: -9 to 0 dBr, Red: 0 to +5 dBr
            greenThreshold = dBToNormalized(-9.0);
            yellowThreshold = dBToNormalized(0.0);
            qDebug() << "[MeterWidget] Using dBr thresholds: green=" << greenThreshold 
                     << "(-9 dBr), yellow=" << yellowThreshold << "(0 dBr)";
            return;
        }
        else if (m_referenceLevel == "dBu") {
            // Professional line level (0 dBu = +4 dBu nominal, +20 dBu max)
            // Typical range: -20 to +20 dBu
            // Green: below +4 dBu, Yellow: +4 to +12 dBu, Red: above +12 dBu
            greenThreshold = dBToNormalized(4.0);
            yellowThreshold = dBToNormalized(12.0);
            qDebug() << "[MeterWidget] Using dBu thresholds: green=" << greenThreshold 
                     << "(+4 dBu), yellow=" << yellowThreshold << "(+12 dBu)";
            return;
        }
        else if (m_referenceLevel == "dBV") {
            // Consumer line level (0 dBV = -10 dBV nominal)
            // Typical range: -30 to +10 dBV
            // Green: below -10 dBV, Yellow: -10 to 0 dBV, Red: above 0 dBV
            greenThreshold = dBToNormalized(-10.0);
            yellowThreshold = dBToNormalized(0.0);
            qDebug() << "[MeterWidget] Using dBV thresholds: green=" << greenThreshold 
                     << "(-10 dBV), yellow=" << yellowThreshold << "(0 dBV)";
            return;
        }
        else if (m_referenceLevel == "LUFS" || m_referenceLevel == "LU") {
            // Loudness Units relative to Full Scale
            // EBU R128 targets: -23 LUFS (broadcast), max -1 LUFS True Peak
            // Green: below -23 LUFS, Yellow: -23 to -16 LUFS, Red: above -16 LUFS
            greenThreshold = dBToNormalized(-23.0);
            yellowThreshold = dBToNormalized(-16.0);
            qDebug() << "[MeterWidget] Using LUFS thresholds: green=" << greenThreshold 
                     << "(-23 LUFS), yellow=" << yellowThreshold << "(-16 LUFS)";
            return;
        }
        else if (m_referenceLevel == "dB") {
            // Generic dB scale - use sensible defaults based on range
            // For professional audio (typically -80 to +20 dB or similar):
            // Green: below -9 dB, Yellow: -9 to +3 dB, Red: above +3 dB
            // This provides good headroom while flagging potentially hot signals
            greenThreshold = dBToNormalized(-9.0);
            yellowThreshold = dBToNormalized(3.0);
            qDebug() << "[MeterWidget] Using generic dB thresholds: green=" << greenThreshold 
                     << "(-9 dB), yellow=" << yellowThreshold << "(+3 dB)";
            return;
        }
    }
    
    // Fallback to percentage-based thresholds (for non-dB scales)
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
    
    // Use logarithmic scaling for dB scales if enabled
    if (m_useLogarithmicScale && isDatabaseScale()) {
        // Convert to linear domain, normalize, then convert back
        // This provides perceptually accurate dB meter scaling
        
        // Define the dynamic range for logarithmic scaling (in dB)
        const double dynamicRange = m_maxValue - m_minValue;
        
        // For very small ranges, use linear scaling
        if (dynamicRange < 10.0) {
            double normalized = (value - m_minValue) / dynamicRange;
            return qBound(0.0, normalized, 1.0);
        }
        
        // Convert dB to linear (0 dB = 1.0)
        double valueLinear = std::pow(10.0, value / 20.0);
        double minLinear = std::pow(10.0, m_minValue / 20.0);
        double maxLinear = std::pow(10.0, m_maxValue / 20.0);
        
        // Normalize in linear domain
        double normalized = (valueLinear - minLinear) / (maxLinear - minLinear);
        
        // Apply logarithmic compression for better visual representation
        // This makes lower levels more visible
        if (normalized > 0.0) {
            // Use a gentle logarithmic curve
            normalized = std::log10(1.0 + 9.0 * normalized);
        }
        
        return qBound(0.0, normalized, 1.0);
    }
    
    // Linear scaling (default)
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
    // Use format string and reference level if available
    if (!m_referenceLevel.isEmpty()) {
        int precision = 1;  // default
        
        if (!m_format.isEmpty()) {
            precision = extractPrecision(m_format);
        }
        
        return QString("%1 %2").arg(value, 0, 'f', precision).arg(m_referenceLevel);
    }
    
    // Fallback to generic dB display
    return QString("%1 dB").arg(value, 0, 'f', 1);
}

int MeterWidget::extractPrecision(const QString &formatString) const
{
    // Extract precision from format string like "%.1f" -> 1
    QRegularExpression re(R"(%\.(\d+)[fdeEgG])");
    QRegularExpressionMatch match = re.match(formatString);
    
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    
    // Default precision
    return 1;
}

bool MeterWidget::isDatabaseScale() const
{
    // Check if we're using a dB-based scale
    return !m_referenceLevel.isEmpty() && (
        m_referenceLevel.contains("dB") || 
        m_referenceLevel == "VU" || 
        m_referenceLevel.contains("PPM"));
}




double MeterWidget::dBToNormalized(double dBValue) const
{
    // Convert a dB value to a normalized 0.0-1.0 position in the meter range
    if (m_maxValue <= m_minValue) {
        return 0.0;
    }
    
    double normalized = (dBValue - m_minValue) / (m_maxValue - m_minValue);
    return qBound(0.0, normalized, 1.0);
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
    // Calculate available space for meter, accounting for labels at top and dropdown at bottom
    // Top labels: ~80px (value + peak + spacing)
    // Bottom dropdown: ~60px (label + combo + margins)
    int meterHeight = height() - 140;  
    int meterX = (width() - METER_WIDTH) / 2;
    int meterY = 80;
    
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
    
    // Draw scale markings for PPM meters
    drawScaleMarkings(painter, meterRect);
}
void MeterWidget::drawScaleMarkings(QPainter &painter, const QRect &meterRect)
{
    // Only draw scale markings for dB-based meters
    if (!isDatabaseScale() && m_meterType != MeterType::VU_METER) {
        return;
    }
    
    qDebug() << "[MeterWidget::drawScaleMarkings] meter type:" << static_cast<int>(m_meterType) 
             << "range:" << m_minValue << "to" << m_maxValue 
             << "ref:" << m_referenceLevel;
    
    painter.save();
    painter.setPen(QColor(180, 180, 180));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    
    if (m_meterType == MeterType::BBC_PPM) {
        // BBC PPM scale: PPM 1-7 (4 dB steps per PPM number)
        // PPM 4 is typically the reference level
        // We need to distribute PPM markings across the actual parameter range
        
        double range = m_maxValue - m_minValue;
        QVector<double> markings;
        
        // Intelligent marking based on range
        if (range <= 15) {
            // Fine scale: every 2 dB with PPM equivalents
            for (double dB = std::ceil(m_minValue / 2.0) * 2.0; dB <= m_maxValue; dB += 2.0) {
                markings.append(dB);
            }
        } else if (range <= 30) {
            // Medium scale: every 4 dB (matches BBC PPM spacing)
            for (double dB = std::ceil(m_minValue / 4.0) * 4.0; dB <= m_maxValue; dB += 4.0) {
                markings.append(dB);
            }
        } else {
            // Wide scale: every 6 dB
            for (double dB = std::ceil(m_minValue / 6.0) * 6.0; dB <= m_maxValue; dB += 6.0) {
                markings.append(dB);
            }
        }
        
        // Always include 0 dB if in range (typical BBC PPM 4 reference)
        if (0.0 >= m_minValue && 0.0 <= m_maxValue && !markings.contains(0.0)) {
            markings.append(0.0);
            std::sort(markings.begin(), markings.end());
        }
        
        for (double dBValue : markings) {
            double normalized = normalizeValue(dBValue);
            int y = meterRect.bottom() - static_cast<int>(normalized * meterRect.height());
            
            if (y < meterRect.top() - 5 || y > meterRect.bottom() + 5) {
                continue;
            }
            
            // Longer tick for 0 dB (reference level)
            int tickLength = (std::abs(dBValue) < 0.1) ? 10 : 6;
            painter.drawLine(meterRect.right() + 2, y, meterRect.right() + 2 + tickLength, y);
            
            // Draw dB value
            QString label = QString::number(static_cast<int>(dBValue));
            QRect textRect(meterRect.right() + 14, y - 8, 35, 16);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
    else if (m_meterType == MeterType::DIN_PPM) {
        // DIN PPM (IEC 60268-10 Type I): 0 dBr reference, typical range -50 to +5 dBr
        // Major marks every 10 dB, minor marks every 5 dB in mid-range
        QVector<double> majorMarkings;
        QVector<double> minorMarkings;
        
        double range = m_maxValue - m_minValue;
        
        // Always mark 0 dBr (reference level)
        if (0.0 >= m_minValue && 0.0 <= m_maxValue) {
            majorMarkings.append(0.0);
        }
        
        // Add major markings (multiples of 10)
        for (double dB = std::ceil(m_minValue / 10.0) * 10.0; dB <= m_maxValue; dB += 10.0) {
            if (dB != 0.0) {  // Already added
                majorMarkings.append(dB);
            }
        }
        
        // Add minor markings (multiples of 5, but not 10) in the working range (-30 to +5)
        for (double dB = -30.0; dB <= 5.0; dB += 5.0) {
            if (dB >= m_minValue && dB <= m_maxValue && 
                static_cast<int>(dB) % 10 != 0) {  // Not already a major marking
                minorMarkings.append(dB);
            }
        }
        
        // Draw major markings
        for (double dBValue : majorMarkings) {
            double normalized = normalizeValue(dBValue);
            int y = meterRect.bottom() - static_cast<int>(normalized * meterRect.height());
            
            if (y < meterRect.top() - 5 || y > meterRect.bottom() + 5) {
                continue;
            }
            
            // Longer tick for reference level (0 dBr)
            int tickLength = (dBValue == 0.0) ? 10 : 8;
            painter.drawLine(meterRect.right() + 2, y, meterRect.right() + 2 + tickLength, y);
            
            // Draw label
            QString label = QString::number(static_cast<int>(dBValue));
            QRect textRect(meterRect.right() + 14, y - 8, 35, 16);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        }
        
        // Draw minor markings (shorter ticks, no labels)
        painter.save();
        painter.setPen(QColor(140, 140, 140));  // Dimmer for minor ticks
        for (double dBValue : minorMarkings) {
            double normalized = normalizeValue(dBValue);
            int y = meterRect.bottom() - static_cast<int>(normalized * meterRect.height());
            
            if (y < meterRect.top() - 5 || y > meterRect.bottom() + 5) {
                continue;
            }
            
            painter.drawLine(meterRect.right() + 2, y, meterRect.right() + 6, y);
        }
        painter.restore();
    }
    else if (m_meterType == MeterType::DIGITAL_PEAK) {
        // Digital Peak Meter: dBFS scale, 0 dBFS = digital full scale
        // Common markings: 0, -3, -6, -9, -12, -18, -24, -30, -40, -50, -60
        QVector<double> markings;
        
        // Always include 0 dBFS (full scale) if in range
        if (0.0 >= m_minValue && 0.0 <= m_maxValue) {
            markings.append(0.0);
        }
        
        // Critical levels for digital audio
        double criticalLevels[] = {-3.0, -6.0, -9.0, -12.0, -18.0, -24.0, -30.0, -40.0, -50.0, -60.0};
        for (double level : criticalLevels) {
            if (level >= m_minValue && level <= m_maxValue) {
                markings.append(level);
            }
        }
        
        // Sort markings
        std::sort(markings.begin(), markings.end());
        
        for (double dBValue : markings) {
            double normalized = normalizeValue(dBValue);
            int y = meterRect.bottom() - static_cast<int>(normalized * meterRect.height());
            
            if (y < meterRect.top() - 5 || y > meterRect.bottom() + 5) {
                continue;
            }
            
            // Longer tick for 0 dBFS and -18 dBFS (reference levels)
            int tickLength = (dBValue == 0.0 || dBValue == -18.0) ? 10 : 6;
            painter.drawLine(meterRect.right() + 2, y, meterRect.right() + 2 + tickLength, y);
            
            // Draw label
            QString label = QString::number(static_cast<int>(dBValue));
            QRect textRect(meterRect.right() + 14, y - 8, 35, 16);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
    else if (m_meterType == MeterType::VU_METER) {
        // VU Meter: Distributed scale across the actual range
        // We'll show dB markings distributed evenly across the available range
        
        QVector<double> markings;
        double range = m_maxValue - m_minValue;
        
        // Intelligent marking based on range size
        double step;
        if (range <= 12) {
            step = 2.0;  // Fine: every 2 dB
        } else if (range <= 30) {
            step = 3.0;  // Medium: every 3 dB (traditional VU spacing)
        } else if (range <= 60) {
            step = 5.0;  // Coarse: every 5 dB
        } else {
            step = 10.0; // Very coarse: every 10 dB
        }
        
        // Generate markings from min to max
        for (double dB = std::ceil(m_minValue / step) * step; dB <= m_maxValue; dB += step) {
            markings.append(dB);
        }
        
        // Always include 0 dB if in range (common reference)
        if (0.0 >= m_minValue && 0.0 <= m_maxValue && !markings.contains(0.0)) {
            markings.append(0.0);
            std::sort(markings.begin(), markings.end());
        }
        
        for (double dBValue : markings) {
            double normalized = normalizeValue(dBValue);
            int y = meterRect.bottom() - static_cast<int>(normalized * meterRect.height());
            
            if (y < meterRect.top() - 5 || y > meterRect.bottom() + 5) {
                continue;
            }
            
            // Longer tick for 0 dB (reference level)
            int tickLength = (std::abs(dBValue) < 0.1) ? 10 : 6;
            painter.drawLine(meterRect.right() + 2, y, meterRect.right() + 2 + tickLength, y);
            
            // Draw dB value
            QString label = QString::number(static_cast<int>(dBValue));
            QRect textRect(meterRect.right() + 14, y - 8, 35, 16);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
    
    painter.restore();
}

void MeterWidget::setCustomThresholds(double greenThreshold, double yellowThreshold)
{
    m_customGreenThreshold = greenThreshold;
    m_customYellowThreshold = yellowThreshold;
    m_useCustomThresholds = true;
    
    // Save to settings
    QSettings settings("EmberViewer", "EmberViewer");
    settings.setValue("meter/customGreenThreshold", greenThreshold);
    settings.setValue("meter/customYellowThreshold", yellowThreshold);
    settings.setValue("meter/useCustomThresholds", true);
    
    qDebug() << "[MeterWidget] Custom thresholds set: green=" << greenThreshold << "yellow=" << yellowThreshold;
    update();
}

void MeterWidget::resetToDefaultThresholds()
{
    m_useCustomThresholds = false;
    
    // Save to settings
    QSettings settings("EmberViewer", "EmberViewer");
    settings.setValue("meter/useCustomThresholds", false);
    
    qDebug() << "[MeterWidget] Reset to default thresholds";
    update();
}

void MeterWidget::showThresholdConfigDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Configure Color Thresholds");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // Info label
    QString infoText = QString("Configure color zone thresholds for %1\n"
                               "Current range: %2 to %3 %4")
                       .arg(m_identifier)
                       .arg(m_minValue, 0, 'f', 1)
                       .arg(m_maxValue, 0, 'f', 1)
                       .arg(m_referenceLevel.isEmpty() ? "dB" : m_referenceLevel);
    QLabel *infoLabel = new QLabel(infoText, &dialog);
    layout->addWidget(infoLabel);
    
    // Green threshold input
    QHBoxLayout *greenLayout = new QHBoxLayout();
    greenLayout->addWidget(new QLabel("Green threshold (dB):", &dialog));
    QDoubleSpinBox *greenSpin = new QDoubleSpinBox(&dialog);
    greenSpin->setRange(m_minValue, m_maxValue);
    greenSpin->setValue(m_useCustomThresholds ? m_customGreenThreshold : -20.0);
    greenSpin->setDecimals(1);
    greenLayout->addWidget(greenSpin);
    layout->addLayout(greenLayout);
    
    // Yellow threshold input
    QHBoxLayout *yellowLayout = new QHBoxLayout();
    yellowLayout->addWidget(new QLabel("Yellow threshold (dB):", &dialog));
    QDoubleSpinBox *yellowSpin = new QDoubleSpinBox(&dialog);
    yellowSpin->setRange(m_minValue, m_maxValue);
    yellowSpin->setValue(m_useCustomThresholds ? m_customYellowThreshold : -6.0);
    yellowSpin->setDecimals(1);
    yellowLayout->addWidget(yellowSpin);
    layout->addLayout(yellowLayout);
    
    // Use custom checkbox
    QCheckBox *useCustomCheck = new QCheckBox("Use custom thresholds", &dialog);
    useCustomCheck->setChecked(m_useCustomThresholds);
    layout->addWidget(useCustomCheck);
    
    // Help text
    QLabel *helpLabel = new QLabel(
        "Green zone: min to green threshold\n"
        "Yellow zone: green to yellow threshold\n"
        "Red zone: yellow to max", &dialog);
    helpLabel->setStyleSheet("color: gray; font-size: 9pt;");
    layout->addWidget(helpLabel);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("OK", &dialog);
    QPushButton *cancelButton = new QPushButton("Cancel", &dialog);
    QPushButton *resetButton = new QPushButton("Reset to Defaults", &dialog);
    
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, [&]() {
        greenSpin->setValue(-20.0);
        yellowSpin->setValue(-6.0);
        useCustomCheck->setChecked(false);
    });
    
    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (useCustomCheck->isChecked()) {
            setCustomThresholds(greenSpin->value(), yellowSpin->value());
        } else {
            resetToDefaultThresholds();
        }
    }
}

void MeterWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    QAction *configAction = menu.addAction("Configure Thresholds...");
    connect(configAction, &QAction::triggered, this, &MeterWidget::showThresholdConfigDialog);
    
    menu.addSeparator();
    
    // Add logarithmic scale toggle
    QAction *logScaleAction = menu.addAction("Use Logarithmic Scale");
    logScaleAction->setCheckable(true);
    logScaleAction->setChecked(m_useLogarithmicScale);
    connect(logScaleAction, &QAction::triggered, [this](bool checked) {
        m_useLogarithmicScale = checked;
        QSettings settings("EmberViewer", "EmberViewer");
        settings.setValue("meter/useLogarithmicScale", checked);
        update();
        qDebug() << "[MeterWidget] Logarithmic scale:" << (checked ? "enabled" : "disabled");
    });
    
    menu.addSeparator();
    
    QAction *resetPeakAction = menu.addAction("Reset Peak Hold");
    connect(resetPeakAction, &QAction::triggered, [this]() {
        m_peakValue = m_minValue;
        m_peakTime = QDateTime();
        m_peakLabel->setText(QString("Peak: %1").arg(formatValue(m_minValue)));
        m_peakLabel->setStyleSheet("font-size: 10pt; color: #888888;");
        update();
    });
    
    menu.addSeparator();
    
    QAction *resetAction = menu.addAction("Reset to Defaults");
    connect(resetAction, &QAction::triggered, this, &MeterWidget::resetToDefaultThresholds);
    
    menu.exec(event->globalPos());
}


void MeterWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}




#include "SliderWidget.h"
#include <QFont>
#include <QFrame>
#include <cmath>

SliderWidget::SliderWidget(QWidget *parent)
    : QWidget(parent)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_paramType(2)  // Default to Real
    , m_access(0)
    , m_updatingFromCode(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // Identifier label at top
    m_identifierLabel = new QLabel(this);
    QFont identFont = m_identifierLabel->font();
    identFont.setPointSize(14);
    identFont.setBold(true);
    m_identifierLabel->setFont(identFont);
    m_identifierLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_identifierLabel);
    
    mainLayout->addSpacing(10);
    
    // Large current value display
    m_currentValueLabel = new QLabel("0", this);
    QFont valueFont = m_currentValueLabel->font();
    valueFont.setPointSize(28);
    valueFont.setBold(true);
    m_currentValueLabel->setFont(valueFont);
    m_currentValueLabel->setAlignment(Qt::AlignCenter);
    m_currentValueLabel->setStyleSheet("color: #2196F3; padding: 10px;");
    mainLayout->addWidget(m_currentValueLabel);
    
    mainLayout->addSpacing(5);
    
    // Min/Max labels and slider in horizontal layout
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    
    m_minLabel = new QLabel("0", this);
    m_minLabel->setStyleSheet("color: #666; font-size: 10pt;");
    m_minLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_minLabel->setMinimumWidth(60);
    sliderLayout->addWidget(m_minLabel);
    
    // Horizontal slider
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 1000);  // Use fixed range, map to actual values
    m_slider->setValue(0);
    m_slider->setMinimumWidth(300);
    m_slider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    border: 1px solid #bbb;"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #4CAF50, stop:0.7 #FFC107, stop:1 #F44336);"
        "    height: 10px;"
        "    border-radius: 5px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #2196F3;"
        "    border: 2px solid #1976D2;"
        "    width: 20px;"
        "    margin: -5px 0;"
        "    border-radius: 10px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background: #1976D2;"
        "}"
    );
    connect(m_slider, &QSlider::valueChanged, this, &SliderWidget::onSliderValueChanged);
    sliderLayout->addWidget(m_slider);
    
    m_maxLabel = new QLabel("100", this);
    m_maxLabel->setStyleSheet("color: #666; font-size: 10pt;");
    m_maxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_maxLabel->setMinimumWidth(60);
    sliderLayout->addWidget(m_maxLabel);
    
    mainLayout->addLayout(sliderLayout);
    
    mainLayout->addSpacing(10);
    
    // Fine control spinbox
    QHBoxLayout *spinBoxLayout = new QHBoxLayout();
    QLabel *fineLabel = new QLabel("Fine control:", this);
    fineLabel->setStyleSheet("font-size: 10pt; color: #666;");
    spinBoxLayout->addWidget(fineLabel);
    
    m_spinBox = new QDoubleSpinBox(this);
    m_spinBox->setDecimals(3);
    m_spinBox->setRange(0.0, 100.0);
    m_spinBox->setValue(0.0);
    m_spinBox->setMinimumWidth(120);
    m_spinBox->setStyleSheet("font-size: 11pt; padding: 5px;");
    connect(m_spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SliderWidget::onSpinBoxValueChanged);
    spinBoxLayout->addWidget(m_spinBox);
    spinBoxLayout->addStretch();
    
    mainLayout->addLayout(spinBoxLayout);
    
    // Path info at bottom
    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setStyleSheet("color: #AAA; font-size: 9pt; padding-top: 10px;");
    m_pathLabel->setWordWrap(true);
    mainLayout->addWidget(m_pathLabel);
    
    mainLayout->addStretch();
}

SliderWidget::~SliderWidget()
{
}

void SliderWidget::setParameterInfo(const QString &identifier, const QString &path,
                                    double minValue, double maxValue, int paramType,
                                    int access, const QString &formula, const QString &format)
{
    m_identifier = identifier;
    m_parameterPath = path;
    m_minValue = minValue;
    m_maxValue = maxValue;
    m_paramType = paramType;
    m_access = access;
    m_formula = formula;
    m_format = format;
    
    m_identifierLabel->setText(identifier);
    m_pathLabel->setText(QString("Path: %1 | Range: %2 to %3")
                        .arg(path)
                        .arg(minValue)
                        .arg(maxValue));
    
    // Update labels
    m_minLabel->setText(formatDisplayValue(minValue));
    m_maxLabel->setText(formatDisplayValue(maxValue));
    
    // Configure spinbox
    m_updatingFromCode = true;
    m_spinBox->setRange(minValue, maxValue);
    if (paramType == 1) {  // Integer
        m_spinBox->setDecimals(0);
        m_spinBox->setSingleStep(1.0);
    } else {  // Real
        m_spinBox->setDecimals(3);
        double step = (maxValue - minValue) / 100.0;
        m_spinBox->setSingleStep(step);
    }
    m_updatingFromCode = false;
    
    // Enable/disable based on access
    bool canWrite = (access == 2 || access == 3);  // WriteOnly or ReadWrite
    setEditEnabled(canWrite);
}

void SliderWidget::setValue(double value)
{
    m_updatingFromCode = true;
    
    // Clamp value
    value = qBound(m_minValue, value, m_maxValue);
    
    // Update display
    m_currentValueLabel->setText(formatDisplayValue(value));
    
    // Update slider
    int sliderPos = doubleToSliderPosition(value);
    m_slider->setValue(sliderPos);
    
    // Update spinbox
    m_spinBox->setValue(value);
    
    m_updatingFromCode = false;
}

void SliderWidget::setEditEnabled(bool enabled)
{
    m_slider->setEnabled(enabled);
    m_spinBox->setEnabled(enabled);
    
    if (!enabled) {
        m_currentValueLabel->setStyleSheet("color: #999; padding: 10px;");
    } else {
        m_currentValueLabel->setStyleSheet("color: #2196F3; padding: 10px;");
    }
}

void SliderWidget::onSliderValueChanged(int value)
{
    if (m_updatingFromCode) return;
    
    double doubleValue = sliderPositionToDouble(value);
    
    // Round to integer if needed
    if (m_paramType == 1) {
        doubleValue = std::round(doubleValue);
    }
    
    // Update display and spinbox
    m_updatingFromCode = true;
    m_currentValueLabel->setText(formatDisplayValue(doubleValue));
    m_spinBox->setValue(doubleValue);
    m_updatingFromCode = false;
    
    // Emit value change
    QString valueStr;
    if (m_paramType == 1) {
        valueStr = QString::number(static_cast<int>(doubleValue));
    } else {
        valueStr = QString::number(doubleValue, 'f', 6);
    }
    emit valueChanged(m_parameterPath, valueStr, m_paramType);
}

void SliderWidget::onSpinBoxValueChanged(double value)
{
    if (m_updatingFromCode) return;
    
    // Update slider and display
    m_updatingFromCode = true;
    m_currentValueLabel->setText(formatDisplayValue(value));
    int sliderPos = doubleToSliderPosition(value);
    m_slider->setValue(sliderPos);
    m_updatingFromCode = false;
    
    // Emit value change
    QString valueStr;
    if (m_paramType == 1) {
        valueStr = QString::number(static_cast<int>(value));
    } else {
        valueStr = QString::number(value, 'f', 6);
    }
    emit valueChanged(m_parameterPath, valueStr, m_paramType);
}

QString SliderWidget::formatDisplayValue(double value) const
{
    // Use formula/format if available
    if (!m_format.isEmpty()) {
        // Simple formatting - just append units if present
        if (m_paramType == 1) {
            return QString::number(static_cast<int>(value)) + " " + m_format;
        } else {
            return QString::number(value, 'f', 2) + " " + m_format;
        }
    }
    
    // Default formatting
    if (m_paramType == 1) {
        return QString::number(static_cast<int>(value));
    } else {
        return QString::number(value, 'f', 2);
    }
}

int SliderWidget::doubleToSliderPosition(double value) const
{
    // Map [minValue, maxValue] to [0, 1000]
    if (m_maxValue == m_minValue) return 0;
    
    double normalized = (value - m_minValue) / (m_maxValue - m_minValue);
    return static_cast<int>(normalized * 1000.0);
}

double SliderWidget::sliderPositionToDouble(int position) const
{
    // Map [0, 1000] to [minValue, maxValue]
    double normalized = position / 1000.0;
    return m_minValue + normalized * (m_maxValue - m_minValue);
}

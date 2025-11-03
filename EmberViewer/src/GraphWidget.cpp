

#include "GraphWidget.h"
#include <QPainter>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <cmath>
#include <algorithm>

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
    , m_minValue(0.0)
    , m_maxValue(100.0)
    , m_streamIdentifier(-1)
    , m_timeWindowSeconds(30)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);
    
    
    m_identifierLabel = new QLabel(this);
    QFont identFont = m_identifierLabel->font();
    identFont.setPointSize(14);
    identFont.setBold(true);
    m_identifierLabel->setFont(identFont);
    m_identifierLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_identifierLabel);
    
    
    m_currentValueLabel = new QLabel("--", this);
    QFont valueFont = m_currentValueLabel->font();
    valueFont.setPointSize(18);
    valueFont.setBold(true);
    m_currentValueLabel->setFont(valueFont);
    m_currentValueLabel->setAlignment(Qt::AlignCenter);
    m_currentValueLabel->setStyleSheet("color: #2196F3; padding: 5px;");
    mainLayout->addWidget(m_currentValueLabel);
    
    
    m_statsLabel = new QLabel("No data", this);
    m_statsLabel->setAlignment(Qt::AlignCenter);
    m_statsLabel->setStyleSheet("color: #666; font-size: 10pt;");
    mainLayout->addWidget(m_statsLabel);
    
    mainLayout->addSpacing(10);
    
    
    QWidget *graphArea = new QWidget(this);
    graphArea->setMinimumHeight(250);
    graphArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(graphArea, 1);  
    
    
    QHBoxLayout *controlLayout = new QHBoxLayout();
    QLabel *timeLabel = new QLabel("Time window:", this);
    timeLabel->setStyleSheet("font-size: 10pt;");
    controlLayout->addWidget(timeLabel);
    
    m_timeWindowCombo = new QComboBox(this);
    m_timeWindowCombo->addItem("10 seconds", 10);
    m_timeWindowCombo->addItem("30 seconds", 30);
    m_timeWindowCombo->addItem("1 minute", 60);
    m_timeWindowCombo->addItem("5 minutes", 300);
    m_timeWindowCombo->setCurrentIndex(1);  
    m_timeWindowCombo->setStyleSheet("font-size: 10pt;");
    connect(m_timeWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GraphWidget::onTimeWindowChanged);
    controlLayout->addWidget(m_timeWindowCombo);
    controlLayout->addStretch();
    
    mainLayout->addLayout(controlLayout);
    
    
    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setStyleSheet("color: #AAA; font-size: 9pt; padding-top: 5px;");
    m_pathLabel->setWordWrap(true);
    mainLayout->addWidget(m_pathLabel);
    
    setMinimumSize(400, 400);
}

GraphWidget::~GraphWidget()
{
}

void GraphWidget::setParameterInfo(const QString &identifier, const QString &path,
                                   double minValue, double maxValue, const QString &format)
{
    m_identifier = identifier;
    m_parameterPath = path;
    m_minValue = minValue;
    m_maxValue = maxValue;
    m_format = format;
    
    m_identifierLabel->setText(identifier);
    m_pathLabel->setText(QString("Path: %1 | Range: %2 to %3")
                        .arg(path)
                        .arg(minValue)
                        .arg(maxValue));
}

void GraphWidget::addDataPoint(double value)
{
    DataPoint point;
    point.timestamp = QDateTime::currentMSecsSinceEpoch();
    point.value = value;
    
    m_dataPoints.append(point);
    
    
    pruneOldData();
    
    
    m_currentValueLabel->setText(formatValue(value));
    
    
    updateStats();
    
    
    update();
}

void GraphWidget::setTimeWindow(int seconds)
{
    m_timeWindowSeconds = seconds;
    pruneOldData();
    update();
}

void GraphWidget::onTimeWindowChanged(int index)
{
    int seconds = m_timeWindowCombo->itemData(index).toInt();
    setTimeWindow(seconds);
}

void GraphWidget::pruneOldData()
{
    if (m_dataPoints.isEmpty()) return;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 cutoff = now - (m_timeWindowSeconds * 1000);
    
    
    while (!m_dataPoints.isEmpty() && m_dataPoints.first().timestamp < cutoff) {
        m_dataPoints.removeFirst();
    }
}

void GraphWidget::updateStats()
{
    if (m_dataPoints.isEmpty()) {
        m_statsLabel->setText("No data");
        return;
    }
    
    double min = m_dataPoints.first().value;
    double max = m_dataPoints.first().value;
    double sum = 0.0;
    
    for (const DataPoint &point : m_dataPoints) {
        min = qMin(min, point.value);
        max = qMax(max, point.value);
        sum += point.value;
    }
    
    double avg = sum / m_dataPoints.size();
    
    m_statsLabel->setText(QString("Min: %1  |  Avg: %2  |  Max: %3  |  Samples: %4")
                         .arg(formatValue(min))
                         .arg(formatValue(avg))
                         .arg(formatValue(max))
                         .arg(m_dataPoints.size()));
}

void GraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    
    QRect graphRect = rect();
    graphRect.adjust(60, 100, -20, -120);  
    
    
    painter.fillRect(graphRect, QColor(250, 250, 250));
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(graphRect);
    
    
    drawGridLines(painter, graphRect);
    drawAxes(painter, graphRect);
    
    
    drawGraph(painter, graphRect);
}

void GraphWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    update();
}

void GraphWidget::drawAxes(QPainter &painter, const QRect &graphRect)
{
    painter.setPen(QPen(QColor(100, 100, 100), 1));
    QFont axisFont = painter.font();
    axisFont.setPointSize(8);
    painter.setFont(axisFont);
    
    
    int numYLabels = 5;
    for (int i = 0; i <= numYLabels; i++) {
        double value = m_minValue + (m_maxValue - m_minValue) * i / numYLabels;
        int y = graphRect.bottom() - (graphRect.height() * i / numYLabels);
        
        QString label = formatValue(value);
        painter.drawText(QRect(5, y - 10, 50, 20), Qt::AlignRight | Qt::AlignVCenter, label);
        
        
        painter.drawLine(graphRect.left() - 5, y, graphRect.left(), y);
    }
    
    
    if (!m_dataPoints.isEmpty()) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 windowStart = now - (m_timeWindowSeconds * 1000);
        
        int numXLabels = 4;
        for (int i = 0; i <= numXLabels; i++) {
            qint64 timestamp = windowStart + (m_timeWindowSeconds * 1000 * i / numXLabels);
            int x = graphRect.left() + (graphRect.width() * i / numXLabels);
            
            int secondsAgo = (now - timestamp) / 1000;
            QString label = QString("-%1s").arg(secondsAgo);
            painter.drawText(QRect(x - 30, graphRect.bottom() + 5, 60, 20), 
                           Qt::AlignCenter, label);
            
            
            painter.drawLine(x, graphRect.bottom(), x, graphRect.bottom() + 5);
        }
    }
}

void GraphWidget::drawGridLines(QPainter &painter, const QRect &graphRect)
{
    painter.setPen(QPen(QColor(220, 220, 220), 1, Qt::DotLine));
    
    
    int numHLines = 5;
    for (int i = 0; i <= numHLines; i++) {
        int y = graphRect.bottom() - (graphRect.height() * i / numHLines);
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }
    
    
    int numVLines = 4;
    for (int i = 0; i <= numVLines; i++) {
        int x = graphRect.left() + (graphRect.width() * i / numVLines);
        painter.drawLine(x, graphRect.top(), x, graphRect.bottom());
    }
}

void GraphWidget::drawGraph(QPainter &painter, const QRect &graphRect)
{
    if (m_dataPoints.size() < 2) return;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 windowStart = now - (m_timeWindowSeconds * 1000);
    
    
    QPainterPath path;
    bool firstPoint = true;
    
    for (const DataPoint &point : m_dataPoints) {
        
        double timeRatio = static_cast<double>(point.timestamp - windowStart) / (m_timeWindowSeconds * 1000);
        int x = graphRect.left() + static_cast<int>(timeRatio * graphRect.width());
        
        
        double valueRatio = (point.value - m_minValue) / (m_maxValue - m_minValue);
        valueRatio = qBound(0.0, valueRatio, 1.0);  
        int y = graphRect.bottom() - static_cast<int>(valueRatio * graphRect.height());
        
        if (firstPoint) {
            path.moveTo(x, y);
            firstPoint = false;
        } else {
            path.lineTo(x, y);
        }
    }
    
    
    painter.setPen(QPen(QColor(33, 150, 243), 2));  
    painter.drawPath(path);
    
    
    painter.setBrush(QColor(33, 150, 243));
    for (const DataPoint &point : m_dataPoints) {
        double timeRatio = static_cast<double>(point.timestamp - windowStart) / (m_timeWindowSeconds * 1000);
        int x = graphRect.left() + static_cast<int>(timeRatio * graphRect.width());
        
        double valueRatio = (point.value - m_minValue) / (m_maxValue - m_minValue);
        valueRatio = qBound(0.0, valueRatio, 1.0);
        int y = graphRect.bottom() - static_cast<int>(valueRatio * graphRect.height());
        
        painter.drawEllipse(QPoint(x, y), 3, 3);
    }
}

QString GraphWidget::formatValue(double value) const
{
    if (!m_format.isEmpty()) {
        return QString::number(value, 'f', 2) + " " + m_format;
    }
    return QString::number(value, 'f', 2);
}

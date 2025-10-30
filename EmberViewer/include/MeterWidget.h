/*
    EmberViewer - Audio Level Meter Widget
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef METERWIDGET_H
#define METERWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QLabel>

class MeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeterWidget(QWidget *parent = nullptr);
    ~MeterWidget();

    // Set parameter information
    void setParameterInfo(const QString &identifier, const QString &path, 
                         double minValue, double maxValue);
    
    // Update the meter value (called from parameter updates or stream collections)
    void updateValue(double value);
    
    // Get the stream identifier (if known)
    int streamIdentifier() const { return m_streamIdentifier; }
    void setStreamIdentifier(int id) { m_streamIdentifier = id; }
    
    // Get parameter path for routing updates
    QString parameterPath() const { return m_parameterPath; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onUpdateTimer();

private:
    void drawMeter(QPainter &painter);
    void updatePeakHold();
    double normalizeValue(double value) const;
    QColor getColorForLevel(double normalizedLevel) const;
    QString formatValue(double value) const;

    // Parameter info
    QString m_identifier;
    QString m_parameterPath;
    int m_streamIdentifier;
    double m_minValue;
    double m_maxValue;
    
    // Current state
    double m_currentValue;
    double m_displayValue;  // Smoothed value for display
    double m_peakValue;
    QDateTime m_peakTime;
    
    // Update optimization (50fps = 20ms)
    QTimer *m_updateTimer;
    bool m_needsRedraw;
    qint64 m_lastUpdateTime;
    
    // Visual configuration
    static constexpr int METER_WIDTH = 40;
    static constexpr int METER_MARGIN = 10;
    static constexpr int PEAK_HOLD_MS = 2000;  // Peak hold duration
    static constexpr int UPDATE_INTERVAL_MS = 20;  // 50fps
    static constexpr double GREEN_THRESHOLD = 0.75;   // -6dB typical
    static constexpr double YELLOW_THRESHOLD = 0.90;  // -2dB typical
    // Above yellow = red (clipping zone)
    
    // Numeric display
    QLabel *m_valueLabel;
};

#endif // METERWIDGET_H










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

    
    void setParameterInfo(const QString &identifier, const QString &path, 
                         double minValue, double maxValue);
    
    
    void updateValue(double value);
    
    
    int streamIdentifier() const { return m_streamIdentifier; }
    void setStreamIdentifier(int id) { m_streamIdentifier = id; }
    
    
    QString parameterPath() const { return m_parameterPath; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onUpdateTimer();

private:
    void drawMeter(QPainter &painter);
    double normalizeValue(double value) const;
    QColor getColorForLevel(double normalizedLevel) const;
    QString formatValue(double value) const;

    
    QString m_identifier;
    QString m_parameterPath;
    int m_streamIdentifier;
    double m_minValue;
    double m_maxValue;
    
    
    double m_targetValue;       // Latest value from provider
    double m_displayValue;      // Current display value with ballistics applied
    double m_peakValue;
    QDateTime m_peakTime;
    
    
    QTimer *m_updateTimer;
    qint64 m_lastRenderTime;    // For calculating dt in time-domain ballistics
    
    
    static constexpr int METER_WIDTH = 40;
    static constexpr int METER_MARGIN = 10;
    static constexpr int PEAK_HOLD_MS = 2000;  
    static constexpr int UPDATE_INTERVAL_MS = 20;  
    static constexpr double GREEN_THRESHOLD = 0.75;   
    static constexpr double YELLOW_THRESHOLD = 0.90;
    
    // Time constants for DIN PPM ballistics (in seconds)
    static constexpr double RISE_TIME_CONSTANT = 0.010;   // 10ms rise to 99% (5 * tau)
    static constexpr double FALL_TIME_CONSTANT = 1.500;   // 1.5s fall time  
    
    
    
    QLabel *m_valueLabel;
};

#endif 


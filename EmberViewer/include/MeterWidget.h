







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
    void updatePeakHold();
    double normalizeValue(double value) const;
    QColor getColorForLevel(double normalizedLevel) const;
    QString formatValue(double value) const;

    
    QString m_identifier;
    QString m_parameterPath;
    int m_streamIdentifier;
    double m_minValue;
    double m_maxValue;
    
    
    double m_currentValue;
    double m_displayValue;  
    double m_peakValue;
    QDateTime m_peakTime;
    
    
    QTimer *m_updateTimer;
    bool m_needsRedraw;
    qint64 m_lastUpdateTime;
    
    
    static constexpr int METER_WIDTH = 40;
    static constexpr int METER_MARGIN = 10;
    static constexpr int PEAK_HOLD_MS = 2000;  
    static constexpr int UPDATE_INTERVAL_MS = 20;  
    static constexpr double GREEN_THRESHOLD = 0.75;   
    static constexpr double YELLOW_THRESHOLD = 0.90;  
    
    
    
    QLabel *m_valueLabel;
};

#endif 


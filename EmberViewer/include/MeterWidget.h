







#ifndef METERWIDGET_H
#define METERWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QLabel>
#include <QComboBox>

class MeterWidget : public QWidget
{
    Q_OBJECT

public:
    enum class MeterType {
        DIN_PPM,      // DIN 45406 - Fast attack (10ms), slow decay (1.5s)
        BBC_PPM,      // IEC 60268-10 Type IIa - Very fast attack (4ms), slower decay (2.8s)
        VU_METER,     // ANSI C16.5-1942 - Slow symmetric (300ms rise/fall)
        DIGITAL_PEAK  // Modern digital - Instant attack, moderate decay (500ms)
    };

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
    void onMeterTypeChanged(int index);

private:
    void drawMeter(QPainter &painter);
    double normalizeValue(double value) const;
    QColor getColorForLevel(double normalizedLevel) const;
    QString formatValue(double value) const;
    void getMeterConstants(MeterType type, double &riseTime, double &fallTime) const;
    void getColorZones(MeterType type, double &greenThreshold, double &yellowThreshold) const;

    
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
    MeterType m_meterType;      // Current meter type
    
    
    static constexpr int METER_WIDTH = 40;
    static constexpr int METER_MARGIN = 10;
    static constexpr int PEAK_HOLD_MS = 2000;  
    static constexpr int UPDATE_INTERVAL_MS = 20;
    
    
    
    QLabel *m_valueLabel;
    QComboBox *m_meterTypeCombo;
};

#endif 


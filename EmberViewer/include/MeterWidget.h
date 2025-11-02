







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
                         double minValue, double maxValue,
                         const QString &format = QString(), const QString &referenceLevel = QString(),
                         int factor = 1);
    
    
    void updateValue(double value);
    
    
    int streamIdentifier() const { return m_streamIdentifier; }
    void setStreamIdentifier(int id) { m_streamIdentifier = id; }
    
    // Threshold configuration
    void setCustomThresholds(double greenThreshold, double yellowThreshold);
    void resetToDefaultThresholds();
    void showThresholdConfigDialog();
    
    
    QString parameterPath() const { return m_parameterPath; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onUpdateTimer();
    void onMeterTypeChanged(int index);

private:
    void drawMeter(QPainter &painter);
    void drawScaleMarkings(QPainter &painter, const QRect &meterRect);
    double normalizeValue(double value) const;
    QColor getColorForLevel(double normalizedLevel) const;
    QString formatValue(double value) const;
    void getMeterConstants(MeterType type, double &riseTime, double &fallTime) const;
    void getColorZones(MeterType type, double &greenThreshold, double &yellowThreshold) const;
    void setMeterTypeByIndex(int comboIndex);  // Helper to set both combo and enum
    
    // dB-aware helper methods
    bool isDatabaseScale() const;
    double dBToNormalized(double dBValue) const;
    double normalizedToDb(double normalized) const;
    int extractPrecision(const QString &formatString) const;

    
    QString m_identifier;
    QString m_parameterPath;
    QString m_format;           // Format string from provider
    QString m_referenceLevel;   // Detected reference level (dBFS, dBr, etc.)
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
    QLabel *m_peakLabel;  // Shows peak hold value
    QComboBox *m_meterTypeCombo;
    
    // User-configurable thresholds (in dB, -1 means use defaults)
    double m_customGreenThreshold;
    double m_customYellowThreshold;
    bool m_useCustomThresholds;
    bool m_useLogarithmicScale;  // Use logarithmic rendering for dB scales
};

#endif 


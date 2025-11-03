







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
        DIN_PPM,      
        BBC_PPM,      
        VU_METER,     
        DIGITAL_PEAK  
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
    void setMeterTypeByIndex(int comboIndex);  
    
    
    bool isDatabaseScale() const;
    double dBToNormalized(double dBValue) const;
    double normalizedToDb(double normalized) const;
    int extractPrecision(const QString &formatString) const;

    
    QString m_identifier;
    QString m_parameterPath;
    QString m_format;           
    QString m_referenceLevel;   
    int m_streamIdentifier;
    double m_minValue;
    double m_maxValue;
    
    
    double m_targetValue;       
    double m_displayValue;      
    double m_peakValue;
    QDateTime m_peakTime;
    
    
    QTimer *m_updateTimer;
    qint64 m_lastRenderTime;    
    qint64 m_lastLabelUpdateTime;  
    MeterType m_meterType;      
    
    
    static constexpr int METER_WIDTH = 40;
    static constexpr int METER_MARGIN = 10;
    static constexpr int PEAK_HOLD_MS = 2000;  
    static constexpr int UPDATE_INTERVAL_MS = 20;
    static constexpr int LABEL_UPDATE_INTERVAL_MS = 100;  
    
    
    
    QLabel *m_valueLabel;
    QLabel *m_peakLabel;  
    QComboBox *m_meterTypeCombo;
    
    
    double m_customGreenThreshold;
    double m_customYellowThreshold;
    bool m_useCustomThresholds;
    bool m_useLogarithmicScale;  
};

#endif 


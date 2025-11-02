

#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QPainter>
#include <QList>
#include <QString>
#include <QVBoxLayout>

class GraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GraphWidget(QWidget *parent = nullptr);
    ~GraphWidget();

    // Configure the graph parameter
    void setParameterInfo(const QString &identifier, const QString &path,
                         double minValue, double maxValue, const QString &format);
    
    // Add new data point
    void addDataPoint(double value);
    
    // Set time window in seconds
    void setTimeWindow(int seconds);
    
    // Stream identifier
    void setStreamIdentifier(int id) { m_streamIdentifier = id; }
    int streamIdentifier() const { return m_streamIdentifier; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onTimeWindowChanged(int index);

private:
    struct DataPoint {
        qint64 timestamp;  // milliseconds since epoch
        double value;
    };

    QString m_identifier;
    QString m_parameterPath;
    QString m_format;
    double m_minValue;
    double m_maxValue;
    int m_streamIdentifier;
    int m_timeWindowSeconds;
    
    QList<DataPoint> m_dataPoints;
    
    QLabel *m_identifierLabel;
    QLabel *m_currentValueLabel;
    QLabel *m_statsLabel;
    QComboBox *m_timeWindowCombo;
    QLabel *m_pathLabel;
    
    void pruneOldData();
    void drawGraph(QPainter &painter, const QRect &graphRect);
    void drawAxes(QPainter &painter, const QRect &graphRect);
    void drawGridLines(QPainter &painter, const QRect &graphRect);
    QString formatValue(double value) const;
    void updateStats();
};

#endif // GRAPHWIDGET_H

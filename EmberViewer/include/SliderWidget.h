

#ifndef SLIDERWIDGET_H
#define SLIDERWIDGET_H

#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>

class SliderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SliderWidget(QWidget *parent = nullptr);
    ~SliderWidget();

    
    void setParameterInfo(const QString &identifier, const QString &path,
                         double minValue, double maxValue, int paramType,
                         int access, const QString &formula, const QString &format);
    
    
    void setValue(double value);
    
    
    void setEditEnabled(bool enabled);

signals:
    void valueChanged(const QString &path, const QString &newValue, int paramType);

private slots:
    void onSliderValueChanged(int value);
    void onSpinBoxValueChanged(double value);

private:
    QString m_identifier;
    QString m_parameterPath;
    QString m_formula;
    QString m_format;
    double m_minValue;
    double m_maxValue;
    int m_paramType;  
    int m_access;
    
    QLabel *m_identifierLabel;
    QSlider *m_slider;
    QDoubleSpinBox *m_spinBox;
    QLabel *m_currentValueLabel;
    QLabel *m_minLabel;
    QLabel *m_maxLabel;
    QLabel *m_pathLabel;
    
    bool m_updatingFromCode;  
    
    QString formatDisplayValue(double value) const;
    int doubleToSliderPosition(double value) const;
    double sliderPositionToDouble(int position) const;
};

#endif 

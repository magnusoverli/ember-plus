

#ifndef TRIGGERWIDGET_H
#define TRIGGERWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QDateTime>
#include <QString>

class TriggerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TriggerWidget(QWidget *parent = nullptr);
    ~TriggerWidget();

    // Configure the trigger parameter
    void setParameterInfo(const QString &identifier, const QString &path, int access);
    
    // Enable/disable based on access rights or connection state
    void setTriggerEnabled(bool enabled);

signals:
    void triggerActivated(const QString &path, const QString &value);

private slots:
    void onTriggerButtonClicked();

private:
    QString m_identifier;
    QString m_parameterPath;
    int m_access;
    
    QPushButton *m_triggerButton;
    QLabel *m_identifierLabel;
    QLabel *m_lastTriggeredLabel;
    QLabel *m_statusLabel;
    QLabel *m_pathLabel;
    
    QDateTime m_lastTriggerTime;
    int m_triggerCount;
    
    void updateLastTriggeredDisplay();
    void showTriggerFeedback();
};

#endif // TRIGGERWIDGET_H

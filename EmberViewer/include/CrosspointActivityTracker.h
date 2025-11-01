#ifndef CROSSPOINTACTIVITYTRACKER_H
#define CROSSPOINTACTIVITYTRACKER_H

#include <QObject>
#include <QTimer>
#include <QString>

class QLabel;

class CrosspointActivityTracker : public QObject
{
    Q_OBJECT

public:
    explicit CrosspointActivityTracker(QLabel *statusLabel, QObject *parent = nullptr);
    ~CrosspointActivityTracker();

    bool isEnabled() const { return m_enabled; }
    int timeRemaining() const { return m_timeRemaining; }

public slots:
    void enable();
    void disable();
    void resetTimer();

signals:
    void timeout();
    void timeRemainingChanged(int seconds);

private slots:
    void onActivityTimeout();
    void onActivityTimerTick();

private:
    void updateStatusBar();

    QTimer *m_activityTimer;
    QTimer *m_tickTimer;
    QLabel *m_statusLabel;
    int m_timeRemaining;
    bool m_enabled;

    static constexpr int ACTIVITY_TIMEOUT_MS = 60000;
    static constexpr int TICK_INTERVAL_MS = 1000;
};

#endif

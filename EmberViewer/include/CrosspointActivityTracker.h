/*
    EmberViewer - Crosspoint Activity Tracker
    
    Tracks user activity and manages crosspoint editing timeout.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

    static constexpr int ACTIVITY_TIMEOUT_MS = 60000;  // 60 seconds
    static constexpr int TICK_INTERVAL_MS = 1000;       // 1 second
};

#endif // CROSSPOINTACTIVITYTRACKER_H

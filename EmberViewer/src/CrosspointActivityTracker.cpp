/*
    EmberViewer - Crosspoint Activity Tracker Implementation
    
    Tracks user activity and manages crosspoint editing timeout.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "CrosspointActivityTracker.h"
#include <QLabel>

CrosspointActivityTracker::CrosspointActivityTracker(QLabel *statusLabel, QObject *parent)
    : QObject(parent)
    , m_statusLabel(statusLabel)
    , m_timeRemaining(0)
    , m_enabled(false)
{
    // Setup activity timer (60 second timeout)
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(true);
    m_activityTimer->setInterval(ACTIVITY_TIMEOUT_MS);
    connect(m_activityTimer, &QTimer::timeout, this, &CrosspointActivityTracker::onActivityTimeout);
    
    // Setup tick timer (1 second updates for status bar)
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(TICK_INTERVAL_MS); // 1 second
    connect(m_tickTimer, &QTimer::timeout, this, &CrosspointActivityTracker::onActivityTimerTick);
}

CrosspointActivityTracker::~CrosspointActivityTracker()
{
}

void CrosspointActivityTracker::enable()
{
    m_enabled = true;
    resetTimer();
    m_tickTimer->start();
}

void CrosspointActivityTracker::disable()
{
    m_enabled = false;
    m_activityTimer->stop();
    m_tickTimer->stop();
    m_timeRemaining = 0;
    updateStatusBar();
}

void CrosspointActivityTracker::resetTimer()
{
    if (m_enabled) {
        m_timeRemaining = 60;
        m_activityTimer->start();
        updateStatusBar();
    }
}

void CrosspointActivityTracker::onActivityTimeout()
{
    // Timeout reached - emit signal
    emit timeout();
}

void CrosspointActivityTracker::onActivityTimerTick()
{
    if (m_enabled) {
        m_timeRemaining = m_activityTimer->remainingTime() / 1000; // Convert to seconds
        updateStatusBar();
        emit timeRemainingChanged(m_timeRemaining);
    }
}

void CrosspointActivityTracker::updateStatusBar()
{
    if (m_enabled && m_timeRemaining > 0) {
        m_statusLabel->setText(QString("⚠ Crosspoints Enabled (%1s)").arg(m_timeRemaining));
        m_statusLabel->setVisible(true);
    } else {
        m_statusLabel->setVisible(false);
    }
}











#include "CrosspointActivityTracker.h"
#include <QLabel>
#include <QEvent>

CrosspointActivityTracker::CrosspointActivityTracker(QLabel *statusLabel, QObject *parent)
    : QObject(parent)
    , m_statusLabel(statusLabel)
    , m_timeRemaining(0)
    , m_enabled(false)
{
    
    m_activityTimer = new QTimer(this);
    m_activityTimer->setSingleShot(true);
    m_activityTimer->setInterval(ACTIVITY_TIMEOUT_MS);
    connect(m_activityTimer, &QTimer::timeout, this, &CrosspointActivityTracker::onActivityTimeout);
    
    
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(TICK_INTERVAL_MS); 
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
    
    emit timeout();
}

void CrosspointActivityTracker::onActivityTimerTick()
{
    if (m_enabled) {
        m_timeRemaining = m_activityTimer->remainingTime() / 1000; 
        updateStatusBar();
        emit timeRemainingChanged(m_timeRemaining);
    }
}

bool CrosspointActivityTracker::eventFilter(QObject *watched, QEvent *event)
{
    
    if (m_enabled) {
        QEvent::Type type = event->type();
        if (type == QEvent::MouseButtonPress || 
            type == QEvent::MouseButtonRelease ||
            type == QEvent::MouseMove ||
            type == QEvent::KeyPress || 
            type == QEvent::KeyRelease ||
            type == QEvent::Wheel ||
            type == QEvent::FocusIn) {
            resetTimer();
        }
    }
    
    
    return QObject::eventFilter(watched, event);
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

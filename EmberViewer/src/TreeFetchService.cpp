/*
    EmberViewer - Tree Fetch Service Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "TreeFetchService.h"

TreeFetchService::TreeFetchService(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_totalEstimate(0)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(false);
    m_timer->setInterval(QUEUE_PROCESS_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &TreeFetchService::processQueue);
}

TreeFetchService::~TreeFetchService()
{
}

void TreeFetchService::startFetch(const QStringList &initialNodePaths)
{
    if (m_active) {
        qWarning() << "TreeFetchService: Fetch already in progress";
        return;
    }
    
    m_active = true;
    m_pendingPaths.clear();
    m_completedPaths.clear();
    m_activePaths.clear();
    m_totalEstimate = initialNodePaths.size();
    
    // Add all initial nodes to pending queue
    for (const QString &path : initialNodePaths) {
        QString type = path.split('|').value(1);  // Format: "path|type"
        QString nodePath = path.split('|').value(0);
        
        // Only fetch nodes (they have children), skip parameters/matrices/functions
        if (type == "Node") {
            m_pendingPaths.insert(nodePath);
        }
    }
    
    if (m_pendingPaths.isEmpty()) {
        m_active = false;
        emit fetchCompleted(true, "No nodes to fetch");
        return;
    }
    
    // Start processing the queue
    m_timer->start();
    processQueue();
}

void TreeFetchService::cancel()
{
    if (!m_active) {
        return;
    }
    
    m_active = false;
    m_timer->stop();
    m_pendingPaths.clear();
    m_activePaths.clear();
    m_completedPaths.clear();
    
    emit fetchCompleted(false, "Cancelled by user");
}

void TreeFetchService::onNodeReceived(const QString &nodePath)
{
    if (!m_active) {
        return;
    }
    
    // Add this new node to pending fetch queue (to fetch its children)
    if (!m_completedPaths.contains(nodePath) && 
        !m_activePaths.contains(nodePath) &&
        !m_pendingPaths.contains(nodePath)) {
        m_pendingPaths.insert(nodePath);
        m_totalEstimate++;
    }
    
    // Mark parent path as completed (we received its child)
    // The parent is complete when we receive at least one child from it
    if (!nodePath.isEmpty()) {
        QStringList parts = nodePath.split('.');
        if (parts.size() > 1) {
            parts.removeLast();
            QString parentPath = parts.join('.');
            if (m_activePaths.contains(parentPath)) {
                m_activePaths.remove(parentPath);
                m_completedPaths.insert(parentPath);
            }
        } else {
            // Root level node received - root request is complete
            if (m_activePaths.contains("")) {
                m_activePaths.remove("");
                m_completedPaths.insert("");
            }
        }
    }
}

void TreeFetchService::setSendGetDirectoryCallback(std::function<void(const QString&, bool)> callback)
{
    m_sendCallback = callback;
}

void TreeFetchService::processQueue()
{
    if (!m_active) {
        m_timer->stop();
        return;
    }
    
    // Send new requests if we have capacity
    while (m_activePaths.size() < MAX_PARALLEL_REQUESTS && !m_pendingPaths.isEmpty()) {
        // Take a path from pending queue
        QString path = *m_pendingPaths.begin();
        m_pendingPaths.remove(path);
        m_activePaths.insert(path);
        
        // Send GetDirectory request via callback
        if (m_sendCallback) {
            bool isRoot = path.isEmpty();
            m_sendCallback(path, isRoot);
        }
    }
    
    // Check if we're done
    if (m_pendingPaths.isEmpty() && m_activePaths.isEmpty()) {
        int fetchedCount = m_completedPaths.size();
        
        m_active = false;
        m_timer->stop();
        m_completedPaths.clear();
        
        emit fetchCompleted(true, QString("Fetched %1 nodes").arg(fetchedCount));
    }
    else {
        // Update progress
        int total = m_completedPaths.size() + m_activePaths.size() + m_pendingPaths.size();
        emit progressUpdated(m_completedPaths.size(), total);
    }
}

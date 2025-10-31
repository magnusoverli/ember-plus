/*
    EmberViewer - Tree Fetch Service
    
    Manages complete tree fetching operations for comprehensive device snapshots.
    Handles queuing, batching, and progress tracking for tree fetches.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef TREEFETCHSERVICE_H
#define TREEFETCHSERVICE_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

class TreeFetchService : public QObject
{
    Q_OBJECT

public:
    explicit TreeFetchService(QObject *parent = nullptr);
    ~TreeFetchService();

    // Start fetching complete tree from initial node paths
    // Paths are in format "path|type" (e.g., "1|Node", "1.2|Parameter")
    void startFetch(const QStringList &initialNodePaths);
    
    // Cancel ongoing fetch operation
    void cancel();
    
    // Check if fetch is currently active
    bool isActive() const { return m_active; }
    
    // Notify service that a node was received (to queue its children)
    void onNodeReceived(const QString &nodePath);
    
    // Set callback for sending GetDirectory requests
    // The callback will be called with: (path, isRoot)
    // where path is the node path to fetch (empty for root) and isRoot indicates if it's a root request
    void setSendGetDirectoryCallback(std::function<void(const QString&, bool)> callback);

signals:
    // Progress update: (fetchedCount, totalEstimateCount)
    void progressUpdated(int fetchedCount, int totalCount);
    
    // Fetch completed: (success, message)
    void fetchCompleted(bool success, const QString &message);

private slots:
    void processQueue();

private:
    bool m_active;
    QSet<QString> m_pendingPaths;    // Paths waiting to be requested
    QSet<QString> m_completedPaths;  // Paths that have been fetched
    QSet<QString> m_activePaths;     // Paths currently being requested
    int m_totalEstimate;             // Estimated total nodes
    
    QTimer *m_timer;                 // Queue processing timer
    
    std::function<void(const QString&, bool)> m_sendCallback;
    
    static constexpr int MAX_PARALLEL_REQUESTS = 5;
    static constexpr int QUEUE_PROCESS_INTERVAL_MS = 50;
};

#endif // TREEFETCHSERVICE_H

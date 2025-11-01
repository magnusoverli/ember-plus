










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

    
    
    void startFetch(const QStringList &initialNodePaths);
    
    
    void cancel();
    
    
    bool isActive() const { return m_active; }
    
    
    void onNodeReceived(const QString &nodePath);
    
    
    
    
    void setSendGetDirectoryCallback(std::function<void(const QString&, bool)> callback);

signals:
    
    void progressUpdated(int fetchedCount, int totalCount);
    
    
    void fetchCompleted(bool success, const QString &message);

private slots:
    void processQueue();

private:
    bool m_active;
    QSet<QString> m_pendingPaths;    
    QSet<QString> m_completedPaths;  
    QSet<QString> m_activePaths;     
    int m_totalEstimate;             
    
    QTimer *m_timer;                 
    
    std::function<void(const QString&, bool)> m_sendCallback;
    
    static constexpr int MAX_PARALLEL_REQUESTS = 5;
    static constexpr int QUEUE_PROCESS_INTERVAL_MS = 50;
};

#endif 

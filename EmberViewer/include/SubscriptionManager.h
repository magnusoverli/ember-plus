










#ifndef SUBSCRIPTIONMANAGER_H
#define SUBSCRIPTIONMANAGER_H

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QTreeWidgetItem>

class EmberConnection;
class QTreeWidget;

class SubscriptionManager : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionManager(EmberConnection *connection, QObject *parent = nullptr);
    ~SubscriptionManager();

    
    bool isSubscribed(const QString &path) const;
    void clear();

public slots:
    
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemCollapsed(QTreeWidgetItem *item);
    void subscribeToExpandedItems(QTreeWidget *treeWidget);

private:
    struct SubscriptionState {
        QDateTime subscribedAt;
        bool autoSubscribed;
    };

    EmberConnection *m_connection;
    QSet<QString> m_subscribedPaths;
    QMap<QString, SubscriptionState> m_subscriptionStates;
};

#endif 

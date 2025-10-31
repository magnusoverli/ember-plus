/*
    EmberViewer - Subscription Manager
    
    Manages Ember+ parameter and node subscriptions, tracking subscription state
    and handling automatic subscription/unsubscription based on tree visibility.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

    // Subscription state
    bool isSubscribed(const QString &path) const;
    void clear();

public slots:
    // Tree-driven subscription management
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

#endif // SUBSCRIPTIONMANAGER_H

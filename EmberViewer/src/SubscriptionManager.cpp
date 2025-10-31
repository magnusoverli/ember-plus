/*
    EmberViewer - Subscription Manager Implementation
    
    Manages Ember+ parameter and node subscriptions, tracking subscription state
    and handling automatic subscription/unsubscription based on tree visibility.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "SubscriptionManager.h"
#include "EmberConnection.h"
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QDebug>

SubscriptionManager::SubscriptionManager(EmberConnection *connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
}

SubscriptionManager::~SubscriptionManager()
{
}

bool SubscriptionManager::isSubscribed(const QString &path) const
{
    return m_subscribedPaths.contains(path);
}

void SubscriptionManager::clear()
{
    m_subscribedPaths.clear();
    m_subscriptionStates.clear();
}

void SubscriptionManager::onItemExpanded(QTreeWidgetItem *item)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    if (path.isEmpty()) {
        return;
    }
    
    // OPTIMIZATION: Batch subscription - collect all subscriptions needed
    // Subscribe to the expanded item + all visible children in one network packet
    QList<EmberConnection::SubscriptionRequest> subscriptions;
    
    // Subscribe to the expanded item itself
    if (!m_subscribedPaths.contains(path) && !type.isEmpty()) {
        subscriptions.append({path, type});
        m_subscribedPaths.insert(path);
        SubscriptionState state;
        state.subscribedAt = QDateTime::currentDateTime();
        state.autoSubscribed = true;
        m_subscriptionStates[path] = state;
    }
    
    // Subscribe to all immediate children (they're now visible)
    for (int i = 0; i < item->childCount(); i++) {
        QTreeWidgetItem *child = item->child(i);
        QString childPath = child->data(0, Qt::UserRole).toString();
        QString childType = child->text(1);
        
        if (!childPath.isEmpty() && !childType.isEmpty() && 
            !m_subscribedPaths.contains(childPath) &&
            childType != "Loading...") {  // Skip placeholder items
            
            subscriptions.append({childPath, childType});
            m_subscribedPaths.insert(childPath);
            SubscriptionState state;
            state.subscribedAt = QDateTime::currentDateTime();
            state.autoSubscribed = true;
            m_subscriptionStates[childPath] = state;
        }
    }
    
    // Send all subscriptions in one batch
    if (!subscriptions.isEmpty()) {
        if (subscriptions.size() == 1) {
            // Single subscription - use existing methods for code simplicity
            const auto& req = subscriptions.first();
            if (req.type == "Node") {
                m_connection->subscribeToNode(req.path, true);
            } else if (req.type == "Parameter") {
                m_connection->subscribeToParameter(req.path, true);
            } else if (req.type == "Matrix") {
                m_connection->subscribeToMatrix(req.path, true);
            }
        } else {
            // Multiple subscriptions - use batch API for efficiency
            qDebug().noquote() << QString("Batch subscribing to %1 paths (expanded: %2)")
                .arg(subscriptions.size()).arg(path);
            m_connection->sendBatchSubscribe(subscriptions);
        }
    }
}

void SubscriptionManager::onItemCollapsed(QTreeWidgetItem *item)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    if (path.isEmpty() || !m_subscribedPaths.contains(path)) {
        return;  // Not subscribed
    }
    
    // Only auto-unsubscribe if it was auto-subscribed
    if (m_subscriptionStates.contains(path) && m_subscriptionStates[path].autoSubscribed) {
        if (type == "Node") {
            m_connection->unsubscribeFromNode(path);
        } else if (type == "Parameter") {
            m_connection->unsubscribeFromParameter(path);
        } else if (type == "Matrix") {
            m_connection->unsubscribeFromMatrix(path);
        }
        
        m_subscribedPaths.remove(path);
        m_subscriptionStates.remove(path);
    }
}

void SubscriptionManager::subscribeToExpandedItems(QTreeWidget *treeWidget)
{
    // OPTIMIZATION: Tree updates no longer disabled with lazy loading
    // This function now only handles subscriptions for expanded items
    // Using batch subscription for better performance
    
    QList<EmberConnection::SubscriptionRequest> subscriptions;
    
    QTreeWidgetItemIterator it(treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            QString type = (*it)->text(1);
            
            if (!path.isEmpty() && !type.isEmpty() && !m_subscribedPaths.contains(path)) {
                subscriptions.append({path, type});
                m_subscribedPaths.insert(path);
                
                SubscriptionState state;
                state.subscribedAt = QDateTime::currentDateTime();
                state.autoSubscribed = true;
                m_subscriptionStates[path] = state;
            }
        }
        ++it;
    }
    
    if (!subscriptions.isEmpty()) {
        qDebug().noquote() << QString("Batch subscribing to %1 expanded items after tree population")
            .arg(subscriptions.size());
        m_connection->sendBatchSubscribe(subscriptions);
    }
}

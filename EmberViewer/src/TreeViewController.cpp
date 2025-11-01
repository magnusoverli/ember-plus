/*
    EmberViewer - Tree View Controller Implementation
    
    Manages the Ember+ tree widget, including lazy loading, item creation,
    and tree population from Ember+ protocol messages.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "TreeViewController.h"
#include "EmberConnection.h"
#include <QTreeWidget>
#include <QApplication>
#include <QStyle>
#include <QBrush>
#include <QColor>
#include <QDebug>

TreeViewController::TreeViewController(QTreeWidget *treeWidget, EmberConnection *connection, QObject *parent)
    : QObject(parent)
    , m_treeWidget(treeWidget)
    , m_connection(connection)
    , m_itemsAddedSinceUpdate(0)
{
}

TreeViewController::~TreeViewController()
{
}

QTreeWidgetItem* TreeViewController::findTreeItem(const QString &path) const
{
    return m_pathToItem.value(path, nullptr);
}

QStringList TreeViewController::getAllTreeItemPaths() const
{
    QStringList paths;
    
    // Iterate through all items in the tree
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QString path = (*it)->data(0, Qt::UserRole).toString();
        QString type = (*it)->text(1);
        
        if (!path.isEmpty() && !type.isEmpty()) {
            // Format: "path|type" for EmberConnection to filter nodes
            paths.append(QString("%1|%2").arg(path).arg(type));
        }
        
        ++it;
    }
    
    return paths;
}

bool TreeViewController::hasPathBeenFetched(const QString &path) const
{
    return m_fetchedPaths.contains(path);
}

void TreeViewController::markPathAsFetched(const QString &path)
{
    m_fetchedPaths.insert(path);
}

void TreeViewController::clear()
{
    m_pathToItem.clear();
    m_fetchedPaths.clear();
    m_itemsAddedSinceUpdate = 0;
}

void TreeViewController::onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new node (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        // For nodes: prefer description as display name (often more readable than UUID identifier)
        // If no description, fall back to identifier
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Node");
        item->setText(2, "");  // Keep Value column empty for nodes
        
        // Store isOnline state in item data (UserRole + 4 for nodes)
        // Note: Parameters use UserRole + 8 to avoid collision with parameter metadata
        item->setData(0, Qt::UserRole + 4, isOnline);
        
        // Apply offline styling if needed
        if (!isOnline) {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_MessageBoxWarning));
            item->setForeground(0, QBrush(QColor("#888888")));
            item->setForeground(1, QBrush(QColor("#888888")));
            item->setForeground(2, QBrush(QColor("#888888")));
            item->setToolTip(0, QString("%1 - Offline").arg(displayName));
        } else {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_DirIcon));
            // Reset foreground to default
            item->setForeground(0, QBrush());
            item->setForeground(1, QBrush());
            item->setForeground(2, QBrush());
            item->setToolTip(0, "");
        }
        
        if (isNew) {
            qDebug().noquote() << QString("Node: %1 [%2] - %3")
                .arg(displayName).arg(path).arg(isOnline ? "Online" : "Offline");
            
            // LAZY LOADING: Make nodes expandable - they might have children
            // Children will be fetched on-demand when user expands the node
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
        
        // Remove "Loading..." placeholder when real children arrive
        if (item->parent()) {
            QTreeWidgetItem *parent = item->parent();
            // Look for "Loading..." child
            for (int i = 0; i < parent->childCount(); i++) {
                QTreeWidgetItem *child = parent->child(i);
                if (child->text(0) == "Loading..." && child->text(1).isEmpty()) {
                    delete child;
                    break;  // Only one loading item per parent
                }
            }
        }
        
        // Batch updates for better responsiveness - process events every N items
        m_itemsAddedSinceUpdate++;
        if (m_itemsAddedSinceUpdate >= UPDATE_BATCH_SIZE) {
            // Process pending events to keep UI responsive
            // ExcludeUserInputEvents prevents user clicks during update
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            m_itemsAddedSinceUpdate = 0;
        }
    }
}

void TreeViewController::onParameterReceived(const QString &path, int /* number */, const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        // Only log if this is a new parameter (no text set yet)
        bool isNew = item->text(1).isEmpty();
        
        setItemDisplayName(item, identifier);
        item->setText(1, "Parameter");
        
        // Store parameter metadata in item for delegate access (do this BEFORE icon check)
        // Using custom roles (Qt::UserRole + N)
        item->setData(0, Qt::UserRole, path);          // Path (for sending updates)
        item->setData(0, Qt::UserRole + 1, type);       // TypeRole
        item->setData(0, Qt::UserRole + 2, access);     // AccessRole
        item->setData(0, Qt::UserRole + 3, minimum);    // MinimumRole
        item->setData(0, Qt::UserRole + 4, maximum);    // MaximumRole
        item->setData(0, Qt::UserRole + 5, enumOptions); // EnumOptionsRole
        item->setData(0, Qt::UserRole + 9, streamIdentifier);  // StreamIdentifier (always store, even if -1)
        
        // Convert QList<int> to QList<QVariant> for storage
        QList<QVariant> enumValuesVar;
        for (int val : enumValues) {
            enumValuesVar.append(val);
        }
        item->setData(0, Qt::UserRole + 6, QVariant::fromValue(enumValuesVar)); // EnumValuesRole
        item->setData(0, Qt::UserRole + 8, isOnline);   // IsOnlineRole (after Matrix's UserRole + 7)
        
        // Set icon based on whether this is an audio meter
        // Audio meters must have: streamIdentifier > 0, AND be numeric type (Integer=1 or Real=2)
        bool isAudioMeter = (streamIdentifier > 0) && (type == 1 || type == 2);
        if (isAudioMeter) {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_MediaVolume));
        } else {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_FileIcon));
        }
        
        // LAZY LOADING: Parameters are leaf elements - never expandable
        // No need to set child indicator policy
        
        // Remove "Loading..." placeholder when real children arrive
        if (item->parent()) {
            QTreeWidgetItem *parent = item->parent();
            for (int i = 0; i < parent->childCount(); i++) {
                QTreeWidgetItem *child = parent->child(i);
                if (child->text(0) == "Loading..." && child->text(1).isEmpty()) {
                    delete child;
                    break;
                }
            }
        }
        
        // Value display will be handled by MainWindow (needs widget creation logic)
        // Just store the value text for now
        item->setText(2, value);
        
        if (isNew) {
            qDebug().noquote() << QString("Parameter: %1 = %2 [%3] (Type: %4, Access: %5)").arg(identifier).arg(value).arg(path).arg(type).arg(access);
        }
        
        // Batch updates for better responsiveness
        m_itemsAddedSinceUpdate++;
        if (m_itemsAddedSinceUpdate >= UPDATE_BATCH_SIZE) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            m_itemsAddedSinceUpdate = 0;
        }
    }
}

void TreeViewController::onMatrixReceived(const QString &path, int /* number */, const QString &identifier, 
                                   const QString &description, int type, int targetCount, int sourceCount)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        bool isNew = item->text(1).isEmpty();
        
        // For matrices: prefer description as display name (like nodes)
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Matrix");
        item->setText(2, QString("%1×%2").arg(sourceCount).arg(targetCount));
        item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        
        // Store matrix path in user data for later retrieval
        item->setData(0, Qt::UserRole, path);
        item->setData(0, Qt::UserRole + 7, "Matrix");  // Store type as string for onTreeSelectionChanged
        
        if (isNew) {
            qInfo().noquote() << QString("Matrix discovered: %1 (%2×%3)").arg(displayName).arg(sourceCount).arg(targetCount);
            
            // LAZY LOADING: Make matrices expandable - they can have label children
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            
            // Notify MainWindow that a matrix item was created
            emit matrixItemCreated(path, item);
        }
    }
}

void TreeViewController::onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                                   const QStringList &argNames, const QList<int> &argTypes,
                                   const QStringList &resultNames, const QList<int> &resultTypes)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        bool isNew = item->text(1).isEmpty();
        
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Function");
        item->setText(2, "");
        item->setIcon(0, QIcon::fromTheme("system-run", m_treeWidget->style()->standardIcon(QStyle::SP_CommandLink)));
        
        if (isNew) {
            qDebug().noquote() << QString("Function: %1 [%2] (%3 args)")
                .arg(displayName).arg(path).arg(argNames.size());
            
            // LAZY LOADING: Functions are leaf elements - never expandable
            // No need to set child indicator policy
            
            // Notify MainWindow that a function item was created (it needs to store function metadata)
            emit functionItemCreated(path, item);
        }
    }
}

void TreeViewController::onItemExpanded(QTreeWidgetItem *item)
{
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->text(1);
    
    if (path.isEmpty()) {
        return;
    }
    
    // LAZY LOADING: Request children if not yet fetched
    // Only for nodes that haven't been fetched and have no real children yet
    if (type == "Node" && !m_fetchedPaths.contains(path)) {
        m_fetchedPaths.insert(path);  // Mark as fetched to avoid duplicate requests
        
        // AUTO-REQUEST: Children are automatically requested when node is received
        // So by the time user expands, children should already be present
        // Only fetch if somehow we don't have children yet
        bool needsToFetch = (item->childCount() == 0);
        
        if (needsToFetch) {
            // Add "Loading..." placeholder
            QTreeWidgetItem *loadingItem = new QTreeWidgetItem(item);
            loadingItem->setText(0, "Loading...");
            loadingItem->setText(1, "");  // No type
            loadingItem->setForeground(0, QBrush(QColor("#888888")));
            loadingItem->setFlags(Qt::ItemIsEnabled);  // Not selectable
            
            // OPTIMIZATION 1 & 3: Batch request with smart prefetching
            // Request the expanded node's children AND prefetch sibling nodes
            // to reduce latency when users explore horizontally after expanding
            QStringList pathsToPrefetch;
            pathsToPrefetch << path;  // The expanded node itself
            
            // Prefetch siblings (nodes at same level) - common navigation pattern
            if (QTreeWidgetItem* parent = item->parent()) {
                for (int i = 0; i < parent->childCount(); i++) {
                    QTreeWidgetItem* sibling = parent->child(i);
                    if (sibling != item) {  // Don't re-add the current item
                        QString siblingPath = sibling->data(0, Qt::UserRole).toString();
                        QString siblingType = sibling->text(1);
                        // Only prefetch Node types that haven't been fetched yet
                        if (siblingType == "Node" && !siblingPath.isEmpty() && 
                            !m_fetchedPaths.contains(siblingPath)) {
                            pathsToPrefetch << siblingPath;
                            m_fetchedPaths.insert(siblingPath);  // Mark as fetched
                        }
                    }
                }
            }
            
            // Request children from device using batch API for efficiency
            if (pathsToPrefetch.size() == 1) {
                qDebug().noquote() << QString("Lazy loading: Requesting children for %1").arg(path);
                m_connection->sendGetDirectoryForPath(path);
            } else {
                qDebug().noquote() << QString("Lazy loading: Batch requesting %1 paths (expanded: %2 + %3 siblings)")
                    .arg(pathsToPrefetch.size()).arg(path).arg(pathsToPrefetch.size() - 1);
                m_connection->sendBatchGetDirectory(pathsToPrefetch);
            }
        }
    }
}

QTreeWidgetItem* TreeViewController::findOrCreateTreeItem(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    
    // Check cache first for O(1) lookup
    if (m_pathToItem.contains(path)) {
        return m_pathToItem[path];
    }
    
    // Split path into segments
    QStringList pathSegments = path.split('.', Qt::SkipEmptyParts);
    
    QTreeWidgetItem *parent = nullptr;
    QString currentPath;
    
    // Walk through the tree, creating missing nodes
    for (int i = 0; i < pathSegments.size(); ++i) {
        if (!currentPath.isEmpty()) {
            currentPath += ".";
        }
        currentPath += pathSegments[i];
        
        // Check cache for this intermediate path (O(1) lookup)
        QTreeWidgetItem *found = m_pathToItem.value(currentPath, nullptr);
        
        // If not in cache, create it
        if (!found) {
            QStringList columns;
            columns << pathSegments[i] << "" << "";
            
            if (parent == nullptr) {
                found = new QTreeWidgetItem(m_treeWidget, columns);
            } else {
                found = new QTreeWidgetItem(parent, columns);
            }
            found->setData(0, Qt::UserRole, currentPath);
            
            // Add to cache immediately
            m_pathToItem[currentPath] = found;
        }
        
        parent = found;
    }
    
    return parent;
}

void TreeViewController::setItemDisplayName(QTreeWidgetItem *item, const QString &baseName)
{
    item->setText(0, baseName);
}

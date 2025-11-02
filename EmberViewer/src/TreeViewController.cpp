










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
    
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QString path = (*it)->data(0, Qt::UserRole).toString();
        QString type = (*it)->text(1);
        
        if (!path.isEmpty() && !type.isEmpty()) {
            
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
        
        bool isNew = item->text(1).isEmpty();
        
        
        
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Node");
        item->setText(2, "");  
        
        
        
        item->setData(0, Qt::UserRole + 4, isOnline);
        
        
        if (!isOnline) {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_MessageBoxWarning));
            item->setForeground(0, QBrush(QColor("#888888")));
            item->setForeground(1, QBrush(QColor("#888888")));
            item->setForeground(2, QBrush(QColor("#888888")));
            item->setToolTip(0, QString("%1 - Offline").arg(displayName));
        } else {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_DirIcon));
            
            item->setForeground(0, QBrush());
            item->setForeground(1, QBrush());
            item->setForeground(2, QBrush());
            item->setToolTip(0, "");
        }
        
        if (isNew) {
            qDebug().noquote() << QString("Node: %1 [%2] - %3")
                .arg(displayName).arg(path).arg(isOnline ? "Online" : "Offline");
            
            
            
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
        
        
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
        
        
        m_itemsAddedSinceUpdate++;
        if (m_itemsAddedSinceUpdate >= UPDATE_BATCH_SIZE) {
            
            
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            m_itemsAddedSinceUpdate = 0;
        }
    }
}

void TreeViewController::onParameterReceived(const QString &path, int , const QString &identifier, const QString &value, 
                                    int access, int type, const QVariant &minimum, const QVariant &maximum,
                                    const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier,
                                    const QString &format, const QString &referenceLevel, const QString &formula, int factor)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        
        bool isNew = item->text(1).isEmpty();
        
        setItemDisplayName(item, identifier);
        item->setText(1, "Parameter");
        
        
        
        item->setData(0, Qt::UserRole, path);          
        item->setData(0, Qt::UserRole + 1, type);       
        item->setData(0, Qt::UserRole + 2, access);     
        item->setData(0, Qt::UserRole + 3, minimum);    
        item->setData(0, Qt::UserRole + 4, maximum);    
        item->setData(0, Qt::UserRole + 5, enumOptions); 
        item->setData(0, Qt::UserRole + 9, streamIdentifier);  
        item->setData(0, Qt::UserRole + 10, format);  // Format string
        item->setData(0, Qt::UserRole + 11, referenceLevel);  // Reference level
        item->setData(0, Qt::UserRole + 12, formula);  // Formula string
        item->setData(0, Qt::UserRole + 13, factor);  // Factor
    qDebug() << "[TreeViewController] Storing format:" << format << "referenceLevel:" << referenceLevel 
             << "formula:" << formula << "factor:" << factor;
        
        
        QList<QVariant> enumValuesVar;
        for (int val : enumValues) {
            enumValuesVar.append(val);
        }
        item->setData(0, Qt::UserRole + 6, QVariant::fromValue(enumValuesVar)); 
        item->setData(0, Qt::UserRole + 8, isOnline);   
        
        
        
        bool isAudioMeter = (streamIdentifier > 0) && (type == 1 || type == 2);
        if (isAudioMeter) {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_MediaVolume));
        } else {
            item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_FileIcon));
        }
        
        
        
        
        
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
        
        
        // Don't display value in tree for audio meter parameters
        if (!isAudioMeter) {
            item->setText(2, value);
        } else {
            item->setText(2, "");  // Clear value column for audio meters
        }
        
        if (isNew) {
            qDebug().noquote() << QString("Parameter: %1 = %2 [%3] (Type: %4, Access: %5)").arg(identifier).arg(value).arg(path).arg(type).arg(access);
        }
        
        
        m_itemsAddedSinceUpdate++;
        if (m_itemsAddedSinceUpdate >= UPDATE_BATCH_SIZE) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            m_itemsAddedSinceUpdate = 0;
        }
    }
}

void TreeViewController::onMatrixReceived(const QString &path, int , const QString &identifier, 
                                   const QString &description, int type, int targetCount, int sourceCount)
{
    QTreeWidgetItem *item = findOrCreateTreeItem(path);
    if (item) {
        bool isNew = item->text(1).isEmpty();
        
        
        QString displayName = !description.isEmpty() ? description : identifier;
        
        setItemDisplayName(item, displayName);
        item->setText(1, "Matrix");
        item->setText(2, QString("%1×%2").arg(sourceCount).arg(targetCount));
        item->setIcon(0, m_treeWidget->style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        
        
        item->setData(0, Qt::UserRole, path);
        item->setData(0, Qt::UserRole + 7, "Matrix");  
        
        
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
        
        if (isNew) {
            qInfo().noquote() << QString("Matrix discovered: %1 (%2×%3)").arg(displayName).arg(sourceCount).arg(targetCount);
            
            
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            
            
            emit matrixItemCreated(path, item);
        }
        
        // Auto-request full details if matrix has no dimensions yet
        if ((targetCount == 0 || sourceCount == 0) && !m_fetchedPaths.contains(path)) {
            qInfo().noquote() << QString("Matrix has no dimensions, auto-requesting details for: %1").arg(path);
            m_fetchedPaths.insert(path);
            m_connection->sendGetDirectoryForPath(path);
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
    
    
    
    if (type == "Node" && !m_fetchedPaths.contains(path)) {
        m_fetchedPaths.insert(path);  
        
        
        
        
        bool needsToFetch = (item->childCount() == 0);
        
        if (needsToFetch) {
            
            QTreeWidgetItem *loadingItem = new QTreeWidgetItem(item);
            loadingItem->setText(0, "Loading...");
            loadingItem->setText(1, "");  
            loadingItem->setForeground(0, QBrush(QColor("#888888")));
            loadingItem->setFlags(Qt::ItemIsEnabled);  
            
            
            
            
            QStringList pathsToPrefetch;
            pathsToPrefetch << path;  
            
            
            if (QTreeWidgetItem* parent = item->parent()) {
                for (int i = 0; i < parent->childCount(); i++) {
                    QTreeWidgetItem* sibling = parent->child(i);
                    if (sibling != item) {  
                        QString siblingPath = sibling->data(0, Qt::UserRole).toString();
                        QString siblingType = sibling->text(1);
                        
                        if (siblingType == "Node" && !siblingPath.isEmpty() && 
                            !m_fetchedPaths.contains(siblingPath)) {
                            pathsToPrefetch << siblingPath;
                            m_fetchedPaths.insert(siblingPath);  
                        }
                    }
                }
            }
            
            
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
    else if (type == "Matrix" && !m_fetchedPaths.contains(path)) {
        
        
        
        m_fetchedPaths.insert(path);
        
        qDebug().noquote() << QString("Lazy loading: Requesting matrix details for %1").arg(path);
        m_connection->sendGetDirectoryForPath(path);
    }
}

QTreeWidgetItem* TreeViewController::findOrCreateTreeItem(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    
    
    if (m_pathToItem.contains(path)) {
        return m_pathToItem[path];
    }
    
    
    QStringList pathSegments = path.split('.', Qt::SkipEmptyParts);
    
    QTreeWidgetItem *parent = nullptr;
    QString currentPath;
    
    
    for (int i = 0; i < pathSegments.size(); ++i) {
        if (!currentPath.isEmpty()) {
            currentPath += ".";
        }
        currentPath += pathSegments[i];
        
        
        QTreeWidgetItem *found = m_pathToItem.value(currentPath, nullptr);
        
        
        if (!found) {
            QStringList columns;
            columns << pathSegments[i] << "" << "";
            
            if (parent == nullptr) {
                found = new QTreeWidgetItem(m_treeWidget, columns);
            } else {
                found = new QTreeWidgetItem(parent, columns);
            }
            found->setData(0, Qt::UserRole, currentPath);
            
            
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

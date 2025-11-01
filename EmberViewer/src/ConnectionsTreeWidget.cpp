







#include "ConnectionsTreeWidget.h"
#include "ConnectionDialog.h"
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QDebug>

ConnectionsTreeWidget::ConnectionsTreeWidget(ConnectionManager *manager, QWidget *parent)
    : QTreeWidget(parent)
    , m_manager(manager)
{
    setHeaderLabel("Saved Connections");
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    connect(m_manager, &ConnectionManager::dataChanged, this, &ConnectionsTreeWidget::onDataChanged);
    connect(this, &QTreeWidget::itemExpanded, this, &ConnectionsTreeWidget::onItemExpanded);
    connect(this, &QTreeWidget::itemCollapsed, this, &ConnectionsTreeWidget::onItemCollapsed);

    buildTree();
}

ConnectionsTreeWidget::~ConnectionsTreeWidget()
{
}

void ConnectionsTreeWidget::refreshTree()
{
    buildTree();
}

void ConnectionsTreeWidget::buildTree()
{
    
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->isExpanded() && isFolder(*it)) {
            QString id = getItemId(*it);
            m_expandedFolders.insert(id);
        }
        ++it;
    }

    clear();
    m_itemMap.clear();

    
    QList<ConnectionManager::Folder> rootFolders = m_manager->getRootFolders();
    
    
    std::sort(rootFolders.begin(), rootFolders.end(), 
        [](const ConnectionManager::Folder &a, const ConnectionManager::Folder &b) {
            return a.name.toLower() < b.name.toLower();
        });

    for (const ConnectionManager::Folder &folder : rootFolders) {
        addFolderToTree(folder.id, nullptr);
    }

    
    QList<QString> rootConnectionIds = m_manager->getRootConnectionIds();
    QList<ConnectionManager::Connection> rootConnections;
    for (const QString &id : rootConnectionIds) {
        rootConnections.append(m_manager->getConnection(id));
    }

    
    std::sort(rootConnections.begin(), rootConnections.end(),
        [](const ConnectionManager::Connection &a, const ConnectionManager::Connection &b) {
            return a.name.toLower() < b.name.toLower();
        });

    for (const ConnectionManager::Connection &conn : rootConnections) {
        addConnectionToTree(conn.id, nullptr);
    }

    
    for (const QString &folderId : m_expandedFolders) {
        QTreeWidgetItem *item = m_itemMap.value(folderId, nullptr);
        if (item) {
            item->setExpanded(true);
        }
    }
}

void ConnectionsTreeWidget::addFolderToTree(const QString &folderId, QTreeWidgetItem *parentItem)
{
    ConnectionManager::Folder folder = m_manager->getFolder(folderId);
    if (folder.id.isEmpty()) {
        return;
    }

    QTreeWidgetItem *item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(this);
    }

    item->setText(0, folder.name);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    item->setData(0, Qt::UserRole, folderId);  
    item->setData(0, Qt::UserRole + 1, "folder");  

    m_itemMap[folderId] = item;

    
    QList<ConnectionManager::Folder> childFolders;
    for (const QString &childId : folder.childIds) {
        if (m_manager->hasFolder(childId)) {
            childFolders.append(m_manager->getFolder(childId));
        }
    }

    
    std::sort(childFolders.begin(), childFolders.end(),
        [](const ConnectionManager::Folder &a, const ConnectionManager::Folder &b) {
            return a.name.toLower() < b.name.toLower();
        });

    for (const ConnectionManager::Folder &childFolder : childFolders) {
        addFolderToTree(childFolder.id, item);
    }

    
    QList<ConnectionManager::Connection> childConnections;
    for (const QString &childId : folder.childIds) {
        if (m_manager->hasConnection(childId)) {
            childConnections.append(m_manager->getConnection(childId));
        }
    }

    
    std::sort(childConnections.begin(), childConnections.end(),
        [](const ConnectionManager::Connection &a, const ConnectionManager::Connection &b) {
            return a.name.toLower() < b.name.toLower();
        });

    for (const ConnectionManager::Connection &conn : childConnections) {
        addConnectionToTree(conn.id, item);
    }
}

void ConnectionsTreeWidget::addConnectionToTree(const QString &connectionId, QTreeWidgetItem *parentItem)
{
    ConnectionManager::Connection conn = m_manager->getConnection(connectionId);
    if (conn.id.isEmpty()) {
        return;
    }

    QTreeWidgetItem *item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(this);
    }

    item->setText(0, conn.name);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DriveNetIcon));
    item->setData(0, Qt::UserRole, connectionId);  
    item->setData(0, Qt::UserRole + 1, "connection");  
    item->setToolTip(0, QString("%1:%2").arg(conn.host).arg(conn.port));

    m_itemMap[connectionId] = item;
}

QString ConnectionsTreeWidget::getItemId(QTreeWidgetItem *item) const
{
    if (!item) {
        return QString();
    }
    return item->data(0, Qt::UserRole).toString();
}

bool ConnectionsTreeWidget::isFolder(QTreeWidgetItem *item) const
{
    if (!item) {
        return false;
    }
    return item->data(0, Qt::UserRole + 1).toString() == "folder";
}

bool ConnectionsTreeWidget::isConnection(QTreeWidgetItem *item) const
{
    if (!item) {
        return false;
    }
    return item->data(0, Qt::UserRole + 1).toString() == "connection";
}

void ConnectionsTreeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    
    if (!item) {
        showRootContextMenu(event->globalPos());
    } else if (isFolder(item)) {
        showFolderContextMenu(event->globalPos(), item);
    } else if (isConnection(item)) {
        showConnectionContextMenu(event->globalPos(), item);
    }
}

void ConnectionsTreeWidget::showRootContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    
    QAction *newFolderAction = menu.addAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder");
    QAction *newConnectionAction = menu.addAction(style()->standardIcon(QStyle::SP_DriveNetIcon), "New Connection");

    QAction *selected = menu.exec(pos);
    if (selected == newFolderAction) {
        createNewFolder(QString());  
    } else if (selected == newConnectionAction) {
        createNewConnection(QString());  
    }
}

void ConnectionsTreeWidget::showFolderContextMenu(const QPoint &pos, QTreeWidgetItem *item)
{
    QString folderId = getItemId(item);
    
    QMenu menu(this);
    
    QAction *newFolderAction = menu.addAction("New Folder");
    QAction *newConnectionAction = menu.addAction("New Connection");
    menu.addSeparator();
    QAction *renameAction = menu.addAction("Rename");
    QAction *deleteAction = menu.addAction("Delete");

    QAction *selected = menu.exec(pos);
    if (selected == newFolderAction) {
        createNewFolder(folderId);
    } else if (selected == newConnectionAction) {
        createNewConnection(folderId);
    } else if (selected == renameAction) {
        editFolder(folderId);
    } else if (selected == deleteAction) {
        deleteFolder(folderId);
    }
}

void ConnectionsTreeWidget::showConnectionContextMenu(const QPoint &pos, QTreeWidgetItem *item)
{
    QString connectionId = getItemId(item);
    
    QMenu menu(this);
    
    QAction *connectAction = menu.addAction("Connect");
    menu.addSeparator();
    QAction *editAction = menu.addAction("Edit");
    QAction *deleteAction = menu.addAction("Delete");

    QAction *selected = menu.exec(pos);
    if (selected == connectAction) {
        connectToDevice(connectionId);
    } else if (selected == editAction) {
        editConnection(connectionId);
    } else if (selected == deleteAction) {
        deleteConnection(connectionId);
    }
}

void ConnectionsTreeWidget::createNewFolder(const QString &parentId)
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "", &ok);
    
    if (ok && !name.isEmpty()) {
        m_manager->addFolder(name, parentId);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::createNewConnection(const QString &folderId)
{
    ConnectionDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getConnectionName();
        QString host = dialog.getHost();
        int port = dialog.getPort();

        if (name.isEmpty() || host.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Name and host cannot be empty.");
            return;
        }

        m_manager->addConnection(name, host, port, folderId);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::editFolder(const QString &folderId)
{
    ConnectionManager::Folder folder = m_manager->getFolder(folderId);
    if (folder.id.isEmpty()) {
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Folder", "New folder name:", 
                                            QLineEdit::Normal, folder.name, &ok);
    
    if (ok && !newName.isEmpty() && newName != folder.name) {
        m_manager->updateFolder(folderId, newName);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::editConnection(const QString &connectionId)
{
    ConnectionManager::Connection conn = m_manager->getConnection(connectionId);
    if (conn.id.isEmpty()) {
        return;
    }

    ConnectionDialog dialog(this);
    dialog.setWindowTitle("Edit Connection");
    dialog.setConnectionName(conn.name);
    dialog.setHost(conn.host);
    dialog.setPort(conn.port);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getConnectionName();
        QString host = dialog.getHost();
        int port = dialog.getPort();

        if (name.isEmpty() || host.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Name and host cannot be empty.");
            return;
        }

        m_manager->updateConnection(connectionId, name, host, port);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::deleteFolder(const QString &folderId)
{
    ConnectionManager::Folder folder = m_manager->getFolder(folderId);
    if (folder.id.isEmpty()) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Folder",
        QString("Are you sure you want to delete the folder '%1' and all its contents?").arg(folder.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_manager->deleteFolder(folderId);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::deleteConnection(const QString &connectionId)
{
    ConnectionManager::Connection conn = m_manager->getConnection(connectionId);
    if (conn.id.isEmpty()) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Connection",
        QString("Are you sure you want to delete the connection '%1'?").arg(conn.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_manager->deleteConnection(connectionId);
        m_manager->saveToDefaultLocation();
    }
}

void ConnectionsTreeWidget::connectToDevice(const QString &connectionId)
{
    ConnectionManager::Connection conn = m_manager->getConnection(connectionId);
    if (conn.id.isEmpty()) {
        return;
    }

    qInfo() << "Connecting to saved connection:" << conn.name;
    emit connectionDoubleClicked(conn.name, conn.host, conn.port);
}

void ConnectionsTreeWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    
    if (item && isConnection(item)) {
        QString connectionId = getItemId(item);
        connectToDevice(connectionId);
    } else {
        
        QTreeWidget::mouseDoubleClickEvent(event);
    }
}

void ConnectionsTreeWidget::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *draggedItem = currentItem();
    QTreeWidgetItem *targetItem = itemAt(event->position().toPoint());

    if (!draggedItem) {
        return;
    }

    QString draggedId = getItemId(draggedItem);
    QString targetId = targetItem ? getItemId(targetItem) : QString();

    
    QString newParentId;
    if (targetItem && isFolder(targetItem)) {
        
        newParentId = targetId;
    } else if (targetItem && isConnection(targetItem)) {
        
        ConnectionManager::Connection targetConn = m_manager->getConnection(targetId);
        newParentId = targetConn.folderId;
    } else {
        
        newParentId = QString();
    }

    
    if (isConnection(draggedItem)) {
        m_manager->moveConnection(draggedId, newParentId);
        m_manager->saveToDefaultLocation();
    } else if (isFolder(draggedItem)) {
        m_manager->moveFolder(draggedId, newParentId);
        m_manager->saveToDefaultLocation();
    }

    event->accept();
}

void ConnectionsTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->source() == this) {
        event->acceptProposedAction();
    }
}

void ConnectionsTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->source() == this) {
        event->acceptProposedAction();
    }
}

void ConnectionsTreeWidget::onDataChanged()
{
    buildTree();
}

void ConnectionsTreeWidget::onItemExpanded(QTreeWidgetItem *item)
{
    if (isFolder(item)) {
        QString id = getItemId(item);
        m_expandedFolders.insert(id);
    }
}

void ConnectionsTreeWidget::onItemCollapsed(QTreeWidgetItem *item)
{
    if (isFolder(item)) {
        QString id = getItemId(item);
        m_expandedFolders.remove(id);
    }
}

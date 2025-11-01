#ifndef CONNECTIONSTREEWIDGET_H
#define CONNECTIONSTREEWIDGET_H

#include <QTreeWidget>
#include <QMap>
#include "ConnectionManager.h"

class ConnectionsTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ConnectionsTreeWidget(ConnectionManager *manager, QWidget *parent = nullptr);
    ~ConnectionsTreeWidget();

    void refreshTree();
    
signals:
    void connectionDoubleClicked(const QString &name, const QString &host, int port);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

private slots:
    void onDataChanged();
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemCollapsed(QTreeWidgetItem *item);

private:
    void buildTree();
    void addFolderToTree(const QString &folderId, QTreeWidgetItem *parentItem);
    void addConnectionToTree(const QString &connectionId, QTreeWidgetItem *parentItem);
    QString getItemId(QTreeWidgetItem *item) const;
    bool isFolder(QTreeWidgetItem *item) const;
    bool isConnection(QTreeWidgetItem *item) const;
    
    void showFolderContextMenu(const QPoint &pos, QTreeWidgetItem *item);
    void showConnectionContextMenu(const QPoint &pos, QTreeWidgetItem *item);
    void showRootContextMenu(const QPoint &pos);
    
    void createNewFolder(const QString &parentId);
    void createNewConnection(const QString &folderId);
    void editFolder(const QString &folderId);
    void editConnection(const QString &connectionId);
    void deleteFolder(const QString &folderId);
    void deleteConnection(const QString &connectionId);
    void connectToDevice(const QString &connectionId);

    ConnectionManager *m_manager;
    QMap<QString, QTreeWidgetItem*> m_itemMap;
    QSet<QString> m_expandedFolders;
};

#endif

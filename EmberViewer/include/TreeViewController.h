/*
    EmberViewer - Tree View Controller
    
    Manages the Ember+ tree widget, including lazy loading, item creation,
    and tree population from Ember+ protocol messages.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef TREEVIEWCONTROLLER_H
#define TREEVIEWCONTROLLER_H

#include <QObject>
#include <QTreeWidgetItem>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QStringList>

class QTreeWidget;
class EmberConnection;

class TreeViewController : public QObject
{
    Q_OBJECT

public:
    explicit TreeViewController(QTreeWidget *treeWidget, EmberConnection *connection, QObject *parent = nullptr);
    ~TreeViewController();

    // Access to tree state
    QTreeWidgetItem* findTreeItem(const QString &path) const;
    QStringList getAllTreeItemPaths() const;
    bool hasPathBeenFetched(const QString &path) const;
    void markPathAsFetched(const QString &path);
    void clear();

signals:
    // Forwarded/transformed signals for MainWindow
    void matrixItemCreated(const QString &path, QTreeWidgetItem *item);
    void functionItemCreated(const QString &path, QTreeWidgetItem *item);

public slots:
    // Ember+ protocol message handlers
    void onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void onParameterReceived(const QString &path, int number, const QString &identifier, const QString &value, 
                            int access, int type, const QVariant &minimum, const QVariant &maximum,
                            const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier);
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                           const QStringList &argNames, const QList<int> &argTypes,
                           const QStringList &resultNames, const QList<int> &resultTypes);

    // Tree item expansion
    void onItemExpanded(QTreeWidgetItem *item);

private:
    QTreeWidgetItem* findOrCreateTreeItem(const QString &path);
    void setItemDisplayName(QTreeWidgetItem *item, const QString &baseName);

    QTreeWidget *m_treeWidget;
    EmberConnection *m_connection;
    
    // Fast path lookup for tree items
    QMap<QString, QTreeWidgetItem*> m_pathToItem;
    
    // Lazy loading: track which paths have had their children fetched
    QSet<QString> m_fetchedPaths;
    
    // Tree update batching (for progressive UI updates)
    int m_itemsAddedSinceUpdate;
    
    static constexpr int UPDATE_BATCH_SIZE = 100;
    static constexpr int MATRIX_LABEL_PATH_MARKER = 666999666;
};

#endif // TREEVIEWCONTROLLER_H

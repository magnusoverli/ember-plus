/*
    EmberViewer - Connection Manager
    
    Manages saved Ember+ connections and folder structure.
    Handles JSON serialization/deserialization.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QUuid>

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    struct Connection {
        QString id;
        QString name;
        QString host;
        int port;
        QString folderId;  // Empty string = root level
    };

    struct Folder {
        QString id;
        QString name;
        QString parentId;  // Empty string = root level
        QList<QString> childIds;  // IDs of child folders and connections
    };

    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager();

    // Connection management
    QString addConnection(const QString &name, const QString &host, int port, const QString &folderId = QString());
    bool updateConnection(const QString &id, const QString &name, const QString &host, int port);
    bool deleteConnection(const QString &id);
    Connection getConnection(const QString &id) const;
    QList<Connection> getAllConnections() const;
    QList<Connection> getConnectionsInFolder(const QString &folderId) const;

    // Folder management
    QString addFolder(const QString &name, const QString &parentId = QString());
    bool updateFolder(const QString &id, const QString &name);
    bool deleteFolder(const QString &id);  // Deletes folder and all its contents recursively
    Folder getFolder(const QString &id) const;
    QList<Folder> getAllFolders() const;
    QList<Folder> getRootFolders() const;
    QList<QString> getRootConnectionIds() const;

    // Move operations
    bool moveConnection(const QString &connectionId, const QString &newFolderId);
    bool moveFolder(const QString &folderId, const QString &newParentId);

    // Persistence
    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath) const;
    bool loadFromDefaultLocation();
    bool saveToDefaultLocation() const;
    QString getDefaultFilePath() const;

    // Import/Export
    bool importConnections(const QString &filePath, bool merge);  // merge=true: add to existing, merge=false: replace all
    bool exportConnections(const QString &filePath) const;

    // Utility
    void clear();
    int connectionCount() const { return m_connections.size(); }
    int folderCount() const { return m_folders.size(); }
    bool hasConnection(const QString &id) const { return m_connections.contains(id); }
    bool hasFolder(const QString &id) const { return m_folders.contains(id); }

signals:
    void connectionAdded(const QString &id);
    void connectionUpdated(const QString &id);
    void connectionDeleted(const QString &id);
    void folderAdded(const QString &id);
    void folderUpdated(const QString &id);
    void folderDeleted(const QString &id);
    void dataChanged();

private:
    QString generateUuid();
    void removeFolderRecursive(const QString &folderId);
    bool loadFromJson(const QByteArray &jsonData, bool merge);
    QByteArray saveToJson() const;

    QMap<QString, Connection> m_connections;
    QMap<QString, Folder> m_folders;
};

#endif // CONNECTIONMANAGER_H

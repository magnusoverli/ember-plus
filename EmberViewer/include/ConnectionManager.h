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
        QString folderId;
    };

    struct Folder {
        QString id;
        QString name;
        QString parentId;
        QList<QString> childIds;
    };

    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager();

    QString addConnection(const QString &name, const QString &host, int port, const QString &folderId = QString());
    bool updateConnection(const QString &id, const QString &name, const QString &host, int port);
    bool deleteConnection(const QString &id);
    Connection getConnection(const QString &id) const;
    QList<Connection> getAllConnections() const;
    QList<Connection> getConnectionsInFolder(const QString &folderId) const;

    QString addFolder(const QString &name, const QString &parentId = QString());
    bool updateFolder(const QString &id, const QString &name);
    bool deleteFolder(const QString &id);
    Folder getFolder(const QString &id) const;
    QList<Folder> getAllFolders() const;
    QList<Folder> getRootFolders() const;
    QList<QString> getRootConnectionIds() const;

    bool moveConnection(const QString &connectionId, const QString &newFolderId);
    bool moveFolder(const QString &folderId, const QString &newParentId);

    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath) const;
    bool loadFromDefaultLocation();
    bool saveToDefaultLocation() const;
    QString getDefaultFilePath() const;

    bool importConnections(const QString &filePath, bool merge);
    bool exportConnections(const QString &filePath) const;

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

#endif

/*
    EmberViewer - Connection Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "ConnectionManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
{
}

ConnectionManager::~ConnectionManager()
{
}

QString ConnectionManager::generateUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString ConnectionManager::addConnection(const QString &name, const QString &host, int port, const QString &folderId)
{
    Connection conn;
    conn.id = generateUuid();
    conn.name = name;
    conn.host = host;
    conn.port = port;
    conn.folderId = folderId;

    m_connections[conn.id] = conn;

    // Add to parent folder's children list
    if (!folderId.isEmpty() && m_folders.contains(folderId)) {
        m_folders[folderId].childIds.append(conn.id);
    }

    emit connectionAdded(conn.id);
    emit dataChanged();
    
    qInfo() << "Connection added:" << name << "(" << host << ":" << port << ")";
    return conn.id;
}

bool ConnectionManager::updateConnection(const QString &id, const QString &name, const QString &host, int port)
{
    if (!m_connections.contains(id)) {
        return false;
    }

    m_connections[id].name = name;
    m_connections[id].host = host;
    m_connections[id].port = port;

    emit connectionUpdated(id);
    emit dataChanged();
    
    qInfo() << "Connection updated:" << name;
    return true;
}

bool ConnectionManager::deleteConnection(const QString &id)
{
    if (!m_connections.contains(id)) {
        return false;
    }

    Connection conn = m_connections[id];
    
    // Remove from parent folder's children list
    if (!conn.folderId.isEmpty() && m_folders.contains(conn.folderId)) {
        m_folders[conn.folderId].childIds.removeAll(id);
    }

    m_connections.remove(id);

    emit connectionDeleted(id);
    emit dataChanged();
    
    qInfo() << "Connection deleted:" << conn.name;
    return true;
}

ConnectionManager::Connection ConnectionManager::getConnection(const QString &id) const
{
    return m_connections.value(id);
}

QList<ConnectionManager::Connection> ConnectionManager::getAllConnections() const
{
    return m_connections.values();
}

QList<ConnectionManager::Connection> ConnectionManager::getConnectionsInFolder(const QString &folderId) const
{
    QList<Connection> result;
    for (const Connection &conn : m_connections.values()) {
        if (conn.folderId == folderId) {
            result.append(conn);
        }
    }
    return result;
}

QString ConnectionManager::addFolder(const QString &name, const QString &parentId)
{
    Folder folder;
    folder.id = generateUuid();
    folder.name = name;
    folder.parentId = parentId;

    m_folders[folder.id] = folder;

    // Add to parent folder's children list
    if (!parentId.isEmpty() && m_folders.contains(parentId)) {
        m_folders[parentId].childIds.append(folder.id);
    }

    emit folderAdded(folder.id);
    emit dataChanged();
    
    qInfo() << "Folder added:" << name;
    return folder.id;
}

bool ConnectionManager::updateFolder(const QString &id, const QString &name)
{
    if (!m_folders.contains(id)) {
        return false;
    }

    m_folders[id].name = name;

    emit folderUpdated(id);
    emit dataChanged();
    
    qInfo() << "Folder updated:" << name;
    return true;
}

bool ConnectionManager::deleteFolder(const QString &id)
{
    if (!m_folders.contains(id)) {
        return false;
    }

    Folder folder = m_folders[id];
    
    // Remove from parent folder's children list
    if (!folder.parentId.isEmpty() && m_folders.contains(folder.parentId)) {
        m_folders[folder.parentId].childIds.removeAll(id);
    }

    // Recursively delete all children
    removeFolderRecursive(id);

    emit folderDeleted(id);
    emit dataChanged();
    
    qInfo() << "Folder deleted:" << folder.name;
    return true;
}

void ConnectionManager::removeFolderRecursive(const QString &folderId)
{
    if (!m_folders.contains(folderId)) {
        return;
    }

    Folder folder = m_folders[folderId];

    // Delete all children (folders and connections)
    for (const QString &childId : folder.childIds) {
        if (m_folders.contains(childId)) {
            // It's a folder - recurse
            removeFolderRecursive(childId);
        } else if (m_connections.contains(childId)) {
            // It's a connection - delete
            m_connections.remove(childId);
            emit connectionDeleted(childId);
        }
    }

    // Delete the folder itself
    m_folders.remove(folderId);
}

ConnectionManager::Folder ConnectionManager::getFolder(const QString &id) const
{
    return m_folders.value(id);
}

QList<ConnectionManager::Folder> ConnectionManager::getAllFolders() const
{
    return m_folders.values();
}

QList<ConnectionManager::Folder> ConnectionManager::getRootFolders() const
{
    QList<Folder> result;
    for (const Folder &folder : m_folders.values()) {
        if (folder.parentId.isEmpty()) {
            result.append(folder);
        }
    }
    return result;
}

QList<QString> ConnectionManager::getRootConnectionIds() const
{
    QList<QString> result;
    for (const Connection &conn : m_connections.values()) {
        if (conn.folderId.isEmpty()) {
            result.append(conn.id);
        }
    }
    return result;
}

bool ConnectionManager::moveConnection(const QString &connectionId, const QString &newFolderId)
{
    if (!m_connections.contains(connectionId)) {
        return false;
    }

    Connection &conn = m_connections[connectionId];
    QString oldFolderId = conn.folderId;

    // Remove from old folder's children
    if (!oldFolderId.isEmpty() && m_folders.contains(oldFolderId)) {
        m_folders[oldFolderId].childIds.removeAll(connectionId);
    }

    // Add to new folder's children
    if (!newFolderId.isEmpty() && m_folders.contains(newFolderId)) {
        m_folders[newFolderId].childIds.append(connectionId);
    }

    conn.folderId = newFolderId;

    emit connectionUpdated(connectionId);
    emit dataChanged();
    
    qInfo() << "Connection moved:" << conn.name;
    return true;
}

bool ConnectionManager::moveFolder(const QString &folderId, const QString &newParentId)
{
    if (!m_folders.contains(folderId)) {
        return false;
    }

    // Prevent moving a folder into itself or its descendants
    QString checkId = newParentId;
    while (!checkId.isEmpty()) {
        if (checkId == folderId) {
            qWarning() << "Cannot move folder into itself or its descendants";
            return false;
        }
        if (m_folders.contains(checkId)) {
            checkId = m_folders[checkId].parentId;
        } else {
            break;
        }
    }

    Folder &folder = m_folders[folderId];
    QString oldParentId = folder.parentId;

    // Remove from old parent's children
    if (!oldParentId.isEmpty() && m_folders.contains(oldParentId)) {
        m_folders[oldParentId].childIds.removeAll(folderId);
    }

    // Add to new parent's children
    if (!newParentId.isEmpty() && m_folders.contains(newParentId)) {
        m_folders[newParentId].childIds.append(folderId);
    }

    folder.parentId = newParentId;

    emit folderUpdated(folderId);
    emit dataChanged();
    
    qInfo() << "Folder moved:" << folder.name;
    return true;
}

QString ConnectionManager::getDefaultFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appDir = dataPath + "/EmberViewer";
    QDir().mkpath(appDir);
    return appDir + "/connections.json";
}

bool ConnectionManager::loadFromDefaultLocation()
{
    QString filePath = getDefaultFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        qInfo() << "No saved connections file found at:" << filePath;
        return true;  // Not an error - just no saved connections yet
    }

    return loadFromFile(filePath);
}

bool ConnectionManager::saveToDefaultLocation() const
{
    return saveToFile(getDefaultFilePath());
}

bool ConnectionManager::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open connections file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return loadFromJson(data, false);  // Replace existing data
}

bool ConnectionManager::saveToFile(const QString &filePath) const
{
    QByteArray data = saveToJson();
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save connections file:" << filePath;
        return false;
    }

    file.write(data);
    file.close();

    qInfo() << "Connections saved to:" << filePath;
    return true;
}

bool ConnectionManager::importConnections(const QString &filePath, bool merge)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open import file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    bool success = loadFromJson(data, merge);
    
    if (success) {
        qInfo() << "Connections imported from:" << filePath << "(merge:" << merge << ")";
    }
    
    return success;
}

bool ConnectionManager::exportConnections(const QString &filePath) const
{
    return saveToFile(filePath);
}

bool ConnectionManager::loadFromJson(const QByteArray &jsonData, bool merge)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format";
        return false;
    }

    QJsonObject root = doc.object();

    // Clear existing data if not merging
    if (!merge) {
        m_connections.clear();
        m_folders.clear();
    }

    // Load folders
    QJsonArray foldersArray = root["folders"].toArray();
    for (const QJsonValue &folderValue : foldersArray) {
        QJsonObject folderObj = folderValue.toObject();
        
        Folder folder;
        folder.id = folderObj["id"].toString();
        folder.name = folderObj["name"].toString();
        folder.parentId = folderObj["parentId"].toString();
        
        QJsonArray childrenArray = folderObj["children"].toArray();
        for (const QJsonValue &childValue : childrenArray) {
            folder.childIds.append(childValue.toString());
        }

        m_folders[folder.id] = folder;
    }

    // Load connections
    QJsonArray connectionsArray = root["connections"].toArray();
    for (const QJsonValue &connValue : connectionsArray) {
        QJsonObject connObj = connValue.toObject();
        
        Connection conn;
        conn.id = connObj["id"].toString();
        conn.name = connObj["name"].toString();
        conn.host = connObj["host"].toString();
        conn.port = connObj["port"].toInt();
        conn.folderId = connObj["folderId"].toString();

        m_connections[conn.id] = conn;
    }

    emit dataChanged();
    
    qInfo() << "Loaded" << m_connections.size() << "connections and" << m_folders.size() << "folders";
    return true;
}

QByteArray ConnectionManager::saveToJson() const
{
    QJsonObject root;
    root["version"] = "1.0";

    // Save folders
    QJsonArray foldersArray;
    for (const Folder &folder : m_folders.values()) {
        QJsonObject folderObj;
        folderObj["id"] = folder.id;
        folderObj["name"] = folder.name;
        folderObj["parentId"] = folder.parentId;
        
        QJsonArray childrenArray;
        for (const QString &childId : folder.childIds) {
            childrenArray.append(childId);
        }
        folderObj["children"] = childrenArray;

        foldersArray.append(folderObj);
    }
    root["folders"] = foldersArray;

    // Save connections
    QJsonArray connectionsArray;
    for (const Connection &conn : m_connections.values()) {
        QJsonObject connObj;
        connObj["id"] = conn.id;
        connObj["name"] = conn.name;
        connObj["host"] = conn.host;
        connObj["port"] = conn.port;
        connObj["folderId"] = conn.folderId;

        connectionsArray.append(connObj);
    }
    root["connections"] = connectionsArray;

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

void ConnectionManager::clear()
{
    m_connections.clear();
    m_folders.clear();
    emit dataChanged();
    qInfo() << "All connections and folders cleared";
}

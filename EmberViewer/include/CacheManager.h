/*
    EmberViewer - Cache Manager
    
    Manages caching of device metadata including parameter properties,
    device names, and root node information for improved performance
    and user experience across reconnections.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QDateTime>

class CacheManager : public QObject
{
    Q_OBJECT

public:
    // Parameter metadata cache
    struct ParameterCache {
        QString identifier;  // Parameter name/identifier
        int access = -1;     // -1 means unknown, use default ReadWrite (3) when first seen
        int type = 0;        // Type: 0=None
    };
    
    // Root node tracking for smart device name discovery
    struct RootNodeInfo {
        QString path;           // Node path (e.g., "1")
        QString displayName;    // Current display name
        bool isGeneric;         // Whether the name is generic and needs improvement
        QString identityPath;   // Path to identity node (e.g., "1.4")
    };
    
    // Device name caching for instant reconnection
    struct DeviceCache {
        QString deviceName;        // Cached device name
        QString rootPath;          // Root node path (e.g., "1")
        QString identityPath;      // Identity node path (e.g., "1.4")
        QDateTime lastSeen;        // When we last connected
        bool isValid;              // Whether cache is still valid
    };

    explicit CacheManager(QObject *parent = nullptr);
    ~CacheManager();

    // Parameter cache management
    void cacheParameter(const QString &path, const QString &identifier, int access, int type);
    ParameterCache getParameterCache(const QString &path) const;
    bool hasParameterCache(const QString &path) const;
    void clearParameterCache();
    
    // Root node management
    void setRootNode(const QString &path, const QString &displayName, bool isGeneric, const QString &identityPath = QString());
    void updateRootNodeIdentityPath(const QString &path, const QString &identityPath);
    void updateRootNodeDisplayName(const QString &path, const QString &displayName, bool isGeneric);
    RootNodeInfo getRootNode(const QString &path) const;
    bool hasRootNode(const QString &path) const;
    bool isRootNodeGeneric(const QString &path) const;
    void clearRootNodes();
    
    // Device cache management (static, persists across connections)
    static void cacheDevice(const QString &hostPort, const QString &deviceName, const QString &rootPath, const QString &identityPath);
    static DeviceCache getDeviceCache(const QString &hostPort);
    static bool hasDeviceCache(const QString &hostPort);
    static void clearDeviceCache(const QString &hostPort);
    
    // Clear all caches
    void clear();

private:
    QMap<QString, ParameterCache> m_parameterCache;
    QMap<QString, RootNodeInfo> m_rootNodes;
    
    static QMap<QString, DeviceCache> s_deviceCache;
    static constexpr int CACHE_EXPIRY_HOURS = 24;
};

#endif // CACHEMANAGER_H

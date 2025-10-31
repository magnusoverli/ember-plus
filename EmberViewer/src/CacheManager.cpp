/*
    EmberViewer - Cache Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "CacheManager.h"

// Static device cache initialization
QMap<QString, CacheManager::DeviceCache> CacheManager::s_deviceCache;

CacheManager::CacheManager(QObject *parent)
    : QObject(parent)
{
}

CacheManager::~CacheManager()
{
}

// Parameter cache management
void CacheManager::cacheParameter(const QString &path, const QString &identifier, int access, int type)
{
    ParameterCache cache;
    cache.identifier = identifier;
    cache.access = access;
    cache.type = type;
    m_parameterCache[path] = cache;
}

CacheManager::ParameterCache CacheManager::getParameterCache(const QString &path) const
{
    return m_parameterCache.value(path);
}

bool CacheManager::hasParameterCache(const QString &path) const
{
    return m_parameterCache.contains(path);
}

void CacheManager::clearParameterCache()
{
    m_parameterCache.clear();
}

// Root node management
void CacheManager::setRootNode(const QString &path, const QString &displayName, bool isGeneric, const QString &identityPath)
{
    RootNodeInfo info;
    info.path = path;
    info.displayName = displayName;
    info.isGeneric = isGeneric;
    info.identityPath = identityPath;
    m_rootNodes[path] = info;
}

void CacheManager::updateRootNodeIdentityPath(const QString &path, const QString &identityPath)
{
    if (m_rootNodes.contains(path)) {
        m_rootNodes[path].identityPath = identityPath;
    }
}

void CacheManager::updateRootNodeDisplayName(const QString &path, const QString &displayName, bool isGeneric)
{
    if (m_rootNodes.contains(path)) {
        m_rootNodes[path].displayName = displayName;
        m_rootNodes[path].isGeneric = isGeneric;
    }
}

CacheManager::RootNodeInfo CacheManager::getRootNode(const QString &path) const
{
    return m_rootNodes.value(path);
}

bool CacheManager::hasRootNode(const QString &path) const
{
    return m_rootNodes.contains(path);
}

bool CacheManager::isRootNodeGeneric(const QString &path) const
{
    return m_rootNodes.contains(path) && m_rootNodes[path].isGeneric;
}

void CacheManager::clearRootNodes()
{
    m_rootNodes.clear();
}

// Device cache management (static)
void CacheManager::cacheDevice(const QString &hostPort, const QString &deviceName, const QString &rootPath, const QString &identityPath)
{
    DeviceCache cache;
    cache.deviceName = deviceName;
    cache.rootPath = rootPath;
    cache.identityPath = identityPath;
    cache.lastSeen = QDateTime::currentDateTime();
    cache.isValid = true;
    s_deviceCache[hostPort] = cache;
}

CacheManager::DeviceCache CacheManager::getDeviceCache(const QString &hostPort)
{
    if (s_deviceCache.contains(hostPort)) {
        DeviceCache &cache = s_deviceCache[hostPort];
        
        // Check if cache is expired
        QDateTime now = QDateTime::currentDateTime();
        qint64 hoursSinceLastSeen = cache.lastSeen.secsTo(now) / 3600;
        
        if (hoursSinceLastSeen > CACHE_EXPIRY_HOURS) {
            cache.isValid = false;
        }
        
        return cache;
    }
    
    // Return invalid cache
    DeviceCache invalid;
    invalid.isValid = false;
    return invalid;
}

bool CacheManager::hasDeviceCache(const QString &hostPort)
{
    DeviceCache cache = getDeviceCache(hostPort);
    return cache.isValid;
}

void CacheManager::clearDeviceCache(const QString &hostPort)
{
    s_deviceCache.remove(hostPort);
}

// Clear all caches
void CacheManager::clear()
{
    m_parameterCache.clear();
    m_rootNodes.clear();
}

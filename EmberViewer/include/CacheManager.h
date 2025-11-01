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
    struct ParameterCache {
        QString identifier;
        int access = -1;
        int type = 0;
    };
    
    struct RootNodeInfo {
        QString path;
        QString displayName;
        bool isGeneric;
        QString identityPath;
    };
    
    struct DeviceCache {
        QString deviceName;
        QString rootPath;
        QString identityPath;
        QDateTime lastSeen;
        bool isValid;
    };

    explicit CacheManager(QObject *parent = nullptr);
    ~CacheManager();

    void cacheParameter(const QString &path, const QString &identifier, int access, int type);
    ParameterCache getParameterCache(const QString &path) const;
    bool hasParameterCache(const QString &path) const;
    void clearParameterCache();
    
    void setRootNode(const QString &path, const QString &displayName, bool isGeneric, const QString &identityPath = QString());
    void updateRootNodeIdentityPath(const QString &path, const QString &identityPath);
    void updateRootNodeDisplayName(const QString &path, const QString &displayName, bool isGeneric);
    RootNodeInfo getRootNode(const QString &path) const;
    bool hasRootNode(const QString &path) const;
    bool isRootNodeGeneric(const QString &path) const;
    void clearRootNodes();
    
    static void cacheDevice(const QString &hostPort, const QString &deviceName, const QString &rootPath, const QString &identityPath);
    static DeviceCache getDeviceCache(const QString &hostPort);
    static bool hasDeviceCache(const QString &hostPort);
    static void clearDeviceCache(const QString &hostPort);
    
    void clear();

private:
    QMap<QString, ParameterCache> m_parameterCache;
    QMap<QString, RootNodeInfo> m_rootNodes;
    
    static QMap<QString, DeviceCache> s_deviceCache;
    static constexpr int CACHE_EXPIRY_HOURS = 24;
};

#endif

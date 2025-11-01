









#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QVersionNumber>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QJsonObject>

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
    virtual ~UpdateManager();

    
    struct UpdateInfo {
        QString version;           
        QString tagName;           
        QString releaseNotes;      
        QString downloadUrl;       
        QString assetName;         
        qint64 assetSize;          
        QString publishedAt;       
        bool isPrerelease;         
    };

    
    void checkForUpdates();

    
    virtual void installUpdate(const UpdateInfo &updateInfo) = 0;

    
    static QString getCurrentVersion();

    
    void skipVersion(const QString &version);

signals:
    
    void updateAvailable(const UpdateInfo &updateInfo);
    void noUpdateAvailable();
    void updateCheckFailed(const QString &errorMessage);

    
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void installationStarted();
    void installationFinished(bool success, const QString &message);

protected:
    
    virtual QString selectAssetForPlatform(const QJsonObject &release) = 0;

    
    static constexpr const char* GITHUB_API_BASE = "https://api.github.com";
    static constexpr const char* GITHUB_REPO_OWNER = "magnusoverli";
    static constexpr const char* GITHUB_REPO_NAME = "ember-plus";

    
    QNetworkAccessManager *m_networkManager;

    
    QSettings *m_settings;

    
    bool isVersionSkipped(const QString &version) const;

private slots:
    void onUpdateCheckFinished();

private:
    UpdateInfo parseReleaseJson(const QJsonObject &release);
    bool isNewerVersion(const QString &remoteVersion, const QString &currentVersion) const;
};

#endif 

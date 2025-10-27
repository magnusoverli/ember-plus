/*
    EmberViewer - Update Manager Base Class
    
    Abstract base class for platform-specific update managers.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

    // Version information
    struct UpdateInfo {
        QString version;           // e.g., "0.4.0"
        QString tagName;           // e.g., "v0.4.0"
        QString releaseNotes;      // Markdown release notes from GitHub
        QString downloadUrl;       // Platform-specific asset URL
        QString assetName;         // Filename of asset
        qint64 assetSize;          // File size in bytes
        QString publishedAt;       // ISO 8601 timestamp
        bool isPrerelease;         // Is this a pre-release version?
    };

    // Check for updates from GitHub Releases API
    void checkForUpdates();

    // Install an update (platform-specific implementation)
    virtual void installUpdate(const UpdateInfo &updateInfo) = 0;

    // Get current application version
    static QString getCurrentVersion();

    // Skip a specific version (user doesn't want to be notified about it)
    void skipVersion(const QString &version);

signals:
    // Emitted when update check completes
    void updateAvailable(const UpdateInfo &updateInfo);
    void noUpdateAvailable();
    void updateCheckFailed(const QString &errorMessage);

    // Emitted during download/installation
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void installationStarted();
    void installationFinished(bool success, const QString &message);

protected:
    // Platform-specific asset selection (must be implemented by subclasses)
    virtual QString selectAssetForPlatform(const QJsonObject &release) = 0;

    // GitHub API configuration
    static constexpr const char* GITHUB_API_BASE = "https://api.github.com";
    static constexpr const char* GITHUB_REPO_OWNER = "magnusoverli";
    static constexpr const char* GITHUB_REPO_NAME = "ember-plus";

    // Network manager for API requests
    QNetworkAccessManager *m_networkManager;

    // Settings for skipped versions
    QSettings *m_settings;

    // Check if a version should be skipped
    bool isVersionSkipped(const QString &version) const;
    void clearSkippedVersion();

private slots:
    void onUpdateCheckFinished();

private:
    UpdateInfo parseReleaseJson(const QJsonObject &release);
    bool isNewerVersion(const QString &remoteVersion, const QString &currentVersion) const;
};

#endif // UPDATEMANAGER_H

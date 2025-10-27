/*
    EmberViewer - Update Manager Base Class Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "UpdateManager.h"
#include "version.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_settings(new QSettings("EmberViewer", "UpdateSystem", this))
{
}

UpdateManager::~UpdateManager()
{
}

void UpdateManager::checkForUpdates()
{
    // Check if enough time has passed since last check (throttling)
    if (!canCheckForUpdates()) {
        qInfo() << "Update check throttled. Please wait before checking again.";
        emit updateCheckFailed("Please wait a few minutes before checking for updates again.");
        return;
    }
    
    // Update last check time
    m_lastCheckTime = QDateTime::currentDateTime();
    
    // Build GitHub API URL for latest release
    QString apiUrl = QString("%1/repos/%2/%3/releases/latest")
        .arg(GITHUB_API_BASE)
        .arg(GITHUB_REPO_OWNER)
        .arg(GITHUB_REPO_NAME);

    qInfo() << "Checking for updates at:" << apiUrl;

    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", QString("EmberViewer/%1").arg(getCurrentVersion()).toUtf8());
    
    // Add GitHub API token if available (increases rate limit from 60 to 5000 req/hour)
#ifdef GITHUB_API_TOKEN
    request.setRawHeader("Authorization", QString("Bearer %1").arg(GITHUB_API_TOKEN).toUtf8());
    qDebug() << "Using authenticated GitHub API requests";
#endif

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::onUpdateCheckFinished);
}

void UpdateManager::onUpdateCheckFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qWarning() << "Update check failed: invalid reply object";
        emit updateCheckFailed("Invalid network reply");
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;
        
        // Check for rate limiting
        if (reply->errorString().contains("rate limit", Qt::CaseInsensitive)) {
            errorMsg = "GitHub API rate limit exceeded. Please try again later (limit resets hourly).";
            qWarning() << "Update check failed: GitHub API rate limit exceeded";
        } else {
            errorMsg = QString("Network error: %1").arg(reply->errorString());
            qWarning() << "Update check failed:" << errorMsg;
        }
        
        emit updateCheckFailed(errorMsg);
        return;
    }

    // Parse JSON response
    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);

    if (!doc.isObject()) {
        qWarning() << "Update check failed: invalid JSON response";
        emit updateCheckFailed("Invalid JSON response from GitHub API");
        return;
    }

    QJsonObject release = doc.object();

    // Parse release information
    UpdateInfo updateInfo = parseReleaseJson(release);

    if (updateInfo.version.isEmpty()) {
        qWarning() << "Update check failed: could not parse version from release";
        emit updateCheckFailed("Could not parse version information");
        return;
    }

    // Check if this version should be skipped
    if (isVersionSkipped(updateInfo.version)) {
        qInfo() << "Update available but skipped by user:" << updateInfo.version;
        emit noUpdateAvailable();
        return;
    }

    // Compare versions
    QString currentVersion = getCurrentVersion();
    if (isNewerVersion(updateInfo.version, currentVersion)) {
        qInfo() << "Update available:" << updateInfo.version << "(current:" << currentVersion << ")";
        emit updateAvailable(updateInfo);
    } else {
        qInfo() << "No update available. Current version:" << currentVersion;
        emit noUpdateAvailable();
    }
}

UpdateManager::UpdateInfo UpdateManager::parseReleaseJson(const QJsonObject &release)
{
    UpdateInfo info;

    // Extract basic release information
    info.tagName = release["tag_name"].toString();
    info.releaseNotes = release["body"].toString();
    info.publishedAt = release["published_at"].toString();
    info.isPrerelease = release["prerelease"].toBool(false);

    // Extract version number from tag (e.g., "v0.4.0" -> "0.4.0")
    info.version = info.tagName;
    if (info.version.startsWith("v")) {
        info.version = info.version.mid(1);
    }

    // Let platform-specific subclass select the appropriate asset
    QString assetUrl = selectAssetForPlatform(release);

    if (!assetUrl.isEmpty()) {
        info.downloadUrl = assetUrl;

        // Extract asset details from the assets array
        QJsonArray assets = release["assets"].toArray();
        for (const QJsonValue &assetValue : assets) {
            QJsonObject asset = assetValue.toObject();
            if (asset["browser_download_url"].toString() == assetUrl) {
                info.assetName = asset["name"].toString();
                info.assetSize = asset["size"].toVariant().toLongLong();
                break;
            }
        }
    }

    return info;
}

bool UpdateManager::isNewerVersion(const QString &remoteVersion, const QString &currentVersion) const
{
    // Use QVersionNumber for robust semantic version comparison
    QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
    QVersionNumber current = QVersionNumber::fromString(currentVersion);

    return remote > current;
}

QString UpdateManager::getCurrentVersion()
{
    // Returns version string from version.h (e.g., "0.3.2")
    return EMBERVIEWER_VERSION_STRING;
}

bool UpdateManager::isVersionSkipped(const QString &version) const
{
    QString skippedVersion = m_settings->value("skippedVersion", "").toString();
    return skippedVersion == version;
}

void UpdateManager::skipVersion(const QString &version)
{
    m_settings->setValue("skippedVersion", version);
    m_settings->sync();
    qInfo() << "User skipped version:" << version;
}

void UpdateManager::clearSkippedVersion()
{
    m_settings->remove("skipped_version");
}

bool UpdateManager::canCheckForUpdates() const
{
    // Allow first check (lastCheckTime will be null/invalid)
    if (!m_lastCheckTime.isValid()) {
        return true;
    }
    
    // Check if enough time has passed
    qint64 secondsSinceLastCheck = m_lastCheckTime.secsTo(QDateTime::currentDateTime());
    return secondsSinceLastCheck >= MIN_CHECK_INTERVAL_SECONDS;
}

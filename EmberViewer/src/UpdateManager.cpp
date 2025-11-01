







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
    
    QString apiUrl = QString("%1/repos/%2/%3/releases/latest")
        .arg(GITHUB_API_BASE)
        .arg(GITHUB_REPO_OWNER)
        .arg(GITHUB_REPO_NAME);

    qInfo() << "Checking for updates at:" << apiUrl;

    QNetworkRequest request;
    request.setUrl(QUrl(apiUrl));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", QString("EmberViewer/%1").arg(getCurrentVersion()).toUtf8());
    
    
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

    
    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);

    if (!doc.isObject()) {
        qWarning() << "Update check failed: invalid JSON response";
        emit updateCheckFailed("Invalid JSON response from GitHub API");
        return;
    }

    QJsonObject release = doc.object();

    
    UpdateInfo updateInfo = parseReleaseJson(release);

    if (updateInfo.version.isEmpty()) {
        qWarning() << "Update check failed: could not parse version from release";
        emit updateCheckFailed("Could not parse version information");
        return;
    }

    
    if (isVersionSkipped(updateInfo.version)) {
        qInfo() << "Update available but skipped by user:" << updateInfo.version;
        emit noUpdateAvailable();
        return;
    }

    
    QString currentVersion = getCurrentVersion();
    if (isNewerVersion(updateInfo.version, currentVersion)) {
        
        if (updateInfo.downloadUrl.isEmpty()) {
            qInfo() << "Update" << updateInfo.version << "found but assets not ready yet";
            QString message = QString("Version %1 is available but the download files are still being prepared.\n"
                                    "Please check again in a few minutes.")
                .arg(updateInfo.version);
            emit updateCheckFailed(message);
            return;
        }
        
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

    
    info.tagName = release["tag_name"].toString();
    info.releaseNotes = release["body"].toString();
    info.publishedAt = release["published_at"].toString();
    info.isPrerelease = release["prerelease"].toBool(false);

    
    info.version = info.tagName;
    if (info.version.startsWith("v")) {
        info.version = info.version.mid(1);
    }

    
    QString assetUrl = selectAssetForPlatform(release);

    if (!assetUrl.isEmpty()) {
        info.downloadUrl = assetUrl;

        
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
    
    QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
    QVersionNumber current = QVersionNumber::fromString(currentVersion);

    return remote > current;
}

QString UpdateManager::getCurrentVersion()
{
    
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

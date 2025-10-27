/*
    EmberViewer - Linux Update Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "LinuxUpdateManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

LinuxUpdateManager::LinuxUpdateManager(QObject *parent)
    : UpdateManager(parent)
{
}

LinuxUpdateManager::~LinuxUpdateManager()
{
}

QString LinuxUpdateManager::selectAssetForPlatform(const QJsonObject &release)
{
    QJsonArray assets = release["assets"].toArray();

    // Look for AppImage asset (e.g., "EmberViewer-x86_64.AppImage")
    for (const QJsonValue &assetValue : assets) {
        QJsonObject asset = assetValue.toObject();
        QString assetName = asset["name"].toString();

        // Match AppImage files for x86_64 architecture
        if (assetName.contains("AppImage", Qt::CaseInsensitive) &&
            assetName.contains("x86_64", Qt::CaseInsensitive)) {
            QString downloadUrl = asset["browser_download_url"].toString();
            qInfo() << "Selected Linux asset:" << assetName;
            return downloadUrl;
        }
    }

    qWarning() << "No suitable AppImage asset found for Linux";
    return QString();
}

void LinuxUpdateManager::installUpdate(const UpdateInfo &updateInfo)
{
    qInfo() << "Opening release page for Linux update:" << updateInfo.version;

    // Build the release URL from the version tag
    QString releaseUrl = QString("https://github.com/magnusoverli/ember-plus/releases/tag/%1")
                            .arg(updateInfo.version);

    qInfo() << "Opening URL:" << releaseUrl;
    
    // Open the release page in the default browser
    bool success = QDesktopServices::openUrl(QUrl(releaseUrl));
    
    if (success) {
        qInfo() << "Successfully opened release page in browser";
        emit installationFinished(true, 
            "Release page opened in your browser.\n"
            "Please download and install the update manually.");
    } else {
        qWarning() << "Failed to open release page in browser";
        emit installationFinished(false, 
            QString("Failed to open browser.\n"
                    "Please visit manually:\n%1").arg(releaseUrl));
    }
}

/*
    EmberViewer - Linux Update Manager
    
    Platform-specific update manager for Linux using AppImage updates.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef LINUXUPDATEMANAGER_H
#define LINUXUPDATEMANAGER_H

#include "UpdateManager.h"
#include <QFile>
#include <QNetworkReply>

class LinuxUpdateManager : public UpdateManager
{
    Q_OBJECT

public:
    explicit LinuxUpdateManager(QObject *parent = nullptr);
    ~LinuxUpdateManager();

    // Install update (download AppImage, replace current, restart)
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    // Select AppImage asset from GitHub release
    QString selectAssetForPlatform(const QJsonObject &release) override;

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    bool isRunningFromAppImage() const;
    QString getCurrentAppImagePath() const;
    void restartApplication();

    QFile *m_downloadFile;
    QNetworkReply *m_downloadReply;
    QString m_currentAppImagePath;
    QString m_newAppImagePath;  // Path to newly installed AppImage
};

#endif // LINUXUPDATEMANAGER_H

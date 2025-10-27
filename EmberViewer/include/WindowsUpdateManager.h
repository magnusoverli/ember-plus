/*
    EmberViewer - Windows Update Manager
    
    Platform-specific update manager for Windows using NSIS installer.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef WINDOWSUPDATEMANAGER_H
#define WINDOWSUPDATEMANAGER_H

#include "UpdateManager.h"
#include <QFile>
#include <QNetworkReply>
#include <QTemporaryDir>

class WindowsUpdateManager : public UpdateManager
{
    Q_OBJECT

public:
    explicit WindowsUpdateManager(QObject *parent = nullptr);
    ~WindowsUpdateManager();

    // Install update (download installer, execute, exit app)
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    // Select Windows installer asset from GitHub release
    QString selectAssetForPlatform(const QJsonObject &release) override;

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void executeInstaller(const QString &installerPath);

    QTemporaryDir *m_tempDir;
    QFile *m_downloadFile;
    QNetworkReply *m_downloadReply;
};

#endif // WINDOWSUPDATEMANAGER_H

/*
    EmberViewer - macOS Update Manager
    
    Platform-specific update manager for macOS using DMG installer.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MACUPDATEMANAGER_H
#define MACUPDATEMANAGER_H

#include "UpdateManager.h"
#include <QFile>
#include <QNetworkReply>
#include <QTemporaryDir>

class MacUpdateManager : public UpdateManager
{
    Q_OBJECT

public:
    explicit MacUpdateManager(QObject *parent = nullptr);
    ~MacUpdateManager();

    // Install update (download DMG, mount, copy to Applications, restart)
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    // Select DMG asset from GitHub release
    QString selectAssetForPlatform(const QJsonObject &release) override;

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    bool mountDMG(const QString &dmgPath, QString &mountPoint);
    bool unmountDMG(const QString &mountPoint);
    bool copyAppToApplications(const QString &sourcePath);
    void restartApplication();

    QTemporaryDir *m_tempDir;
    QFile *m_downloadFile;
    QNetworkReply *m_downloadReply;
};

#endif // MACUPDATEMANAGER_H

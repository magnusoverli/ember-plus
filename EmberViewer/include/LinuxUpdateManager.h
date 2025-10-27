/*
    EmberViewer - Linux Update Manager
    
    Platform-specific update manager for Linux.
    Opens the GitHub release page in the browser for manual download.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef LINUXUPDATEMANAGER_H
#define LINUXUPDATEMANAGER_H

#include "UpdateManager.h"

class LinuxUpdateManager : public UpdateManager
{
    Q_OBJECT

public:
    explicit LinuxUpdateManager(QObject *parent = nullptr);
    ~LinuxUpdateManager();

    // Opens the GitHub release page in the default browser
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    // Select AppImage asset from GitHub release
    QString selectAssetForPlatform(const QJsonObject &release) override;
};

#endif // LINUXUPDATEMANAGER_H

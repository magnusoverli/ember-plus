










#ifndef LINUXUPDATEMANAGER_H
#define LINUXUPDATEMANAGER_H

#include "UpdateManager.h"

class LinuxUpdateManager : public UpdateManager
{
    Q_OBJECT

public:
    explicit LinuxUpdateManager(QObject *parent = nullptr);
    ~LinuxUpdateManager();

    
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    
    QString selectAssetForPlatform(const QJsonObject &release) override;
};

#endif 

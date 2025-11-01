









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

    
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    
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

#endif 

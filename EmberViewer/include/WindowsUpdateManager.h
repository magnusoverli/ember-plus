









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

    
    void installUpdate(const UpdateInfo &updateInfo) override;

protected:
    
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

#endif 

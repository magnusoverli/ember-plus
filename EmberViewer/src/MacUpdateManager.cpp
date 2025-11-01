







#include "MacUpdateManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QDir>

MacUpdateManager::MacUpdateManager(QObject *parent)
    : UpdateManager(parent)
    , m_tempDir(nullptr)
    , m_downloadFile(nullptr)
    , m_downloadReply(nullptr)
{
}

MacUpdateManager::~MacUpdateManager()
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }

    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }

    delete m_tempDir;
}

QString MacUpdateManager::selectAssetForPlatform(const QJsonObject &release)
{
    QJsonArray assets = release["assets"].toArray();

    
    for (const QJsonValue &assetValue : assets) {
        QJsonObject asset = assetValue.toObject();
        QString assetName = asset["name"].toString();

        
        if (assetName.endsWith(".dmg", Qt::CaseInsensitive)) {
            QString downloadUrl = asset["browser_download_url"].toString();
            qInfo() << "Selected macOS asset:" << assetName;
            return downloadUrl;
        }
    }

    qWarning() << "No suitable DMG asset found for macOS";
    return QString();
}

void MacUpdateManager::installUpdate(const UpdateInfo &updateInfo)
{
    qInfo() << "Starting macOS update installation for version:" << updateInfo.version;

    if (updateInfo.downloadUrl.isEmpty()) {
        qWarning() << "No download URL available";
        emit installationFinished(false, "No download URL available for this update.");
        return;
    }

    
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        qWarning() << "Failed to create temporary directory";
        emit installationFinished(false, "Failed to create temporary directory.");
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    
    QString downloadPath = m_tempDir->path() + "/" + updateInfo.assetName;
    m_downloadFile = new QFile(downloadPath);

    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open download file:" << downloadPath;
        emit installationFinished(false, "Failed to create download file.");
        delete m_downloadFile;
        m_downloadFile = nullptr;
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    qInfo() << "Downloading DMG to:" << downloadPath;

    QNetworkRequest request;
    request.setUrl(QUrl(updateInfo.downloadUrl));
    request.setRawHeader("User-Agent", QString("EmberViewer/%1").arg(getCurrentVersion()).toUtf8());

    m_downloadReply = m_networkManager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, 
            this, &MacUpdateManager::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile) {
            m_downloadFile->write(m_downloadReply->readAll());
        }
    });
    connect(m_downloadReply, &QNetworkReply::finished, 
            this, &MacUpdateManager::onDownloadFinished);

    emit installationStarted();
}

void MacUpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void MacUpdateManager::onDownloadFinished()
{
    if (!m_downloadReply || !m_downloadFile) {
        return;
    }

    
    m_downloadFile->write(m_downloadReply->readAll());
    m_downloadFile->close();

    QString downloadPath = m_downloadFile->fileName();

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        qWarning() << "Download failed:" << m_downloadReply->errorString();
        emit installationFinished(false, 
            QString("Download failed: %1").arg(m_downloadReply->errorString()));
        
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        delete m_downloadFile;
        m_downloadFile = nullptr;
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    qInfo() << "Download completed:" << downloadPath;

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    delete m_downloadFile;
    m_downloadFile = nullptr;

    
    QString mountPoint;
    if (!mountDMG(downloadPath, mountPoint)) {
        qWarning() << "Failed to mount DMG";
        emit installationFinished(false, "Failed to mount DMG image.");
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    qInfo() << "DMG mounted at:" << mountPoint;

    
    QDir mountDir(mountPoint);
    QStringList appBundles = mountDir.entryList(QStringList() << "*.app", QDir::Dirs);

    if (appBundles.isEmpty()) {
        qWarning() << "No .app bundle found in DMG";
        unmountDMG(mountPoint);
        emit installationFinished(false, "No application bundle found in DMG.");
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    QString appBundlePath = mountPoint + "/" + appBundles.first();
    qInfo() << "Found app bundle:" << appBundlePath;

    
    if (!copyAppToApplications(appBundlePath)) {
        qWarning() << "Failed to copy app to Applications";
        unmountDMG(mountPoint);
        emit installationFinished(false, "Failed to install application.");
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    
    unmountDMG(mountPoint);

    qInfo() << "Update installed successfully - restarting application";
    emit installationFinished(true, "Update installed successfully. Restarting...");

    
    QTimer::singleShot(1000, this, &MacUpdateManager::restartApplication);
}

bool MacUpdateManager::mountDMG(const QString &dmgPath, QString &mountPoint)
{
    
    QProcess process;
    process.start("hdiutil", QStringList() << "attach" << dmgPath << "-nobrowse");
    
    if (!process.waitForFinished(30000)) { 
        qWarning() << "hdiutil attach timed out";
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "hdiutil attach failed:" << process.readAllStandardError();
        return false;
    }

    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n');
    
    for (const QString &line : lines) {
        if (line.contains("/Volumes/")) {
            
            QStringList parts = line.split('\t', Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                mountPoint = parts.last().trimmed();
                return true;
            }
        }
    }

    qWarning() << "Could not parse mount point from hdiutil output";
    return false;
}

bool MacUpdateManager::unmountDMG(const QString &mountPoint)
{
    QProcess process;
    process.start("hdiutil", QStringList() << "detach" << mountPoint);
    
    if (!process.waitForFinished(10000)) { 
        qWarning() << "hdiutil detach timed out";
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "hdiutil detach failed:" << process.readAllStandardError();
        return false;
    }

    qInfo() << "DMG unmounted successfully";
    return true;
}

bool MacUpdateManager::copyAppToApplications(const QString &sourcePath)
{
    QString applicationsPath = "/Applications/EmberViewer.app";

    
    if (QDir(applicationsPath).exists()) {
        qInfo() << "Removing existing installation:" << applicationsPath;
        
        QProcess removeProcess;
        removeProcess.start("rm", QStringList() << "-rf" << applicationsPath);
        
        if (!removeProcess.waitForFinished(10000)) {
            qWarning() << "Failed to remove existing installation";
            return false;
        }
    }

    
    qInfo() << "Copying app bundle to Applications folder";
    
    QProcess copyProcess;
    copyProcess.start("cp", QStringList() << "-R" << sourcePath << applicationsPath);
    
    if (!copyProcess.waitForFinished(30000)) { 
        qWarning() << "Copy operation timed out";
        return false;
    }

    if (copyProcess.exitCode() != 0) {
        qWarning() << "Copy operation failed:" << copyProcess.readAllStandardError();
        return false;
    }

    qInfo() << "App copied successfully to Applications folder";
    return true;
}

void MacUpdateManager::restartApplication()
{
    qInfo() << "Restarting application";

    
    QString appPath = "/Applications/EmberViewer.app";

    
    if (!QDir(appPath).exists()) {
        qCritical() << "Application bundle does not exist:" << appPath;
        emit installationFinished(false, 
            "Failed to restart: Application bundle not found.\n"
            "Please launch EmberViewer manually from Applications folder.");
        return;
    }

    
    qInfo() << "Launching new instance...";
    bool success = QProcess::startDetached("open", QStringList() << appPath);

    if (success) {
        qInfo() << "Successfully launched new instance - exiting current instance";
        
        QCoreApplication::quit();
    } else {
        qCritical() << "Failed to launch new instance with open command";
        emit installationFinished(false, 
            "Update installed successfully, but failed to restart automatically.\n"
            "Please close this window and launch EmberViewer from Applications folder.");
    }
}

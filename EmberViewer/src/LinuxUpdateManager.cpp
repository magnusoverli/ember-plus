/*
    EmberViewer - Linux Update Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "LinuxUpdateManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

LinuxUpdateManager::LinuxUpdateManager(QObject *parent)
    : UpdateManager(parent)
    , m_tempDir(nullptr)
    , m_downloadFile(nullptr)
    , m_downloadReply(nullptr)
{
    m_currentAppImagePath = getCurrentAppImagePath();
}

LinuxUpdateManager::~LinuxUpdateManager()
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
    qInfo() << "Starting Linux update installation for version:" << updateInfo.version;

    if (!isRunningFromAppImage()) {
        qWarning() << "Not running from AppImage - manual installation required";
        emit installationFinished(false, 
            "This application is not running from an AppImage.\n"
            "Please download and install the update manually from GitHub.");
        return;
    }

    if (updateInfo.downloadUrl.isEmpty()) {
        qWarning() << "No download URL available";
        emit installationFinished(false, "No download URL available for this update.");
        return;
    }

    // Create temporary directory for download
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        qWarning() << "Failed to create temporary directory";
        emit installationFinished(false, "Failed to create temporary directory.");
        delete m_tempDir;
        m_tempDir = nullptr;
        return;
    }

    // Download the new AppImage
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

    qInfo() << "Downloading AppImage to:" << downloadPath;

    QNetworkRequest request;
    request.setUrl(QUrl(updateInfo.downloadUrl));
    request.setRawHeader("User-Agent", QString("EmberViewer/%1").arg(getCurrentVersion()).toUtf8());

    m_downloadReply = m_networkManager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, 
            this, &LinuxUpdateManager::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile) {
            m_downloadFile->write(m_downloadReply->readAll());
        }
    });
    connect(m_downloadReply, &QNetworkReply::finished, 
            this, &LinuxUpdateManager::onDownloadFinished);

    emit installationStarted();
}

void LinuxUpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void LinuxUpdateManager::onDownloadFinished()
{
    if (!m_downloadReply || !m_downloadFile) {
        return;
    }

    // Write any remaining data
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

    // Make the new AppImage executable
    QFile::setPermissions(downloadPath, 
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::ExeOther);

    // Replace the current AppImage
    bool success = replaceAppImage(downloadPath);

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    delete m_downloadFile;
    m_downloadFile = nullptr;

    if (success) {
        qInfo() << "Update installed successfully - restarting application";
        emit installationFinished(true, "Update installed successfully. Restarting...");
        
        // Restart the application with a small delay
        QTimer::singleShot(1000, this, &LinuxUpdateManager::restartApplication);
    } else {
        qWarning() << "Failed to replace AppImage";
        emit installationFinished(false, "Failed to install update.");
        delete m_tempDir;
        m_tempDir = nullptr;
    }
}

bool LinuxUpdateManager::isRunningFromAppImage() const
{
    // Check if APPIMAGE environment variable is set
    QString appImagePath = qEnvironmentVariable("APPIMAGE");
    return !appImagePath.isEmpty() && QFile::exists(appImagePath);
}

QString LinuxUpdateManager::getCurrentAppImagePath() const
{
    // Get path from APPIMAGE environment variable
    QString appImagePath = qEnvironmentVariable("APPIMAGE");
    
    if (appImagePath.isEmpty()) {
        qWarning() << "APPIMAGE environment variable not set";
        return QString();
    }

    if (!QFile::exists(appImagePath)) {
        qWarning() << "AppImage path does not exist:" << appImagePath;
        return QString();
    }

    return appImagePath;
}

bool LinuxUpdateManager::replaceAppImage(const QString &newAppImagePath)
{
    if (m_currentAppImagePath.isEmpty()) {
        qWarning() << "Current AppImage path is empty";
        return false;
    }

    QFileInfo currentInfo(m_currentAppImagePath);
    QString backupPath = m_currentAppImagePath + ".backup";

    // Create backup of current AppImage
    if (QFile::exists(backupPath)) {
        QFile::remove(backupPath);
    }

    if (!QFile::copy(m_currentAppImagePath, backupPath)) {
        qWarning() << "Failed to create backup of current AppImage";
        return false;
    }

    qInfo() << "Created backup:" << backupPath;

    // Remove current AppImage
    if (!QFile::remove(m_currentAppImagePath)) {
        qWarning() << "Failed to remove current AppImage";
        // Restore from backup
        QFile::copy(backupPath, m_currentAppImagePath);
        QFile::remove(backupPath);
        return false;
    }

    // Copy new AppImage to current location
    if (!QFile::copy(newAppImagePath, m_currentAppImagePath)) {
        qWarning() << "Failed to copy new AppImage";
        // Restore from backup
        QFile::copy(backupPath, m_currentAppImagePath);
        QFile::remove(backupPath);
        return false;
    }

    // Ensure executable permissions
    QFile::setPermissions(m_currentAppImagePath,
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::ExeOther);

    qInfo() << "Successfully replaced AppImage";

    // Remove backup (keep it for now in case restart fails)
    // QFile::remove(backupPath);

    return true;
}

void LinuxUpdateManager::restartApplication()
{
    qInfo() << "Restarting application from:" << m_currentAppImagePath;

    // Start new instance
    QProcess::startDetached(m_currentAppImagePath, QStringList());

    // Exit current instance
    QCoreApplication::quit();
}

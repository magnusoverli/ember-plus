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
    , m_downloadFile(nullptr)
    , m_downloadReply(nullptr)
    , m_newAppImagePath("")
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

    // Get directory of current AppImage
    QFileInfo currentInfo(m_currentAppImagePath);
    QString targetDir = currentInfo.absolutePath();
    
    // Download directly to target directory with new version filename
    m_newAppImagePath = targetDir + "/" + updateInfo.assetName;
    
    qInfo() << "Downloading AppImage directly to:" << m_newAppImagePath;
    
    // Remove any existing file with the same name
    if (QFile::exists(m_newAppImagePath)) {
        qInfo() << "Removing existing AppImage:" << m_newAppImagePath;
        if (!QFile::remove(m_newAppImagePath)) {
            qWarning() << "Failed to remove existing AppImage:" << m_newAppImagePath;
            emit installationFinished(false, "Failed to remove existing file. Please check file permissions.");
            return;
        }
    }

    // Open file for writing
    m_downloadFile = new QFile(m_newAppImagePath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open download file:" << m_newAppImagePath;
        emit installationFinished(false, "Failed to create download file.");
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

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

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        qWarning() << "Download failed:" << m_downloadReply->errorString();
        emit installationFinished(false, 
            QString("Download failed: %1").arg(m_downloadReply->errorString()));
        
        // Clean up failed download
        delete m_downloadFile;
        m_downloadFile = nullptr;
        QFile::remove(m_newAppImagePath);
        
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    qInfo() << "Download completed:" << m_newAppImagePath;

    // Make the new AppImage executable
    QFile::setPermissions(m_newAppImagePath, 
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::ExeOther);

    qInfo() << "Successfully installed new AppImage";

    // Clean up download objects
    delete m_downloadFile;
    m_downloadFile = nullptr;
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    qInfo() << "Update installed successfully - restarting application";
    emit installationFinished(true, "Update installed successfully. Restarting...");
    
    // Restart the application with a small delay
    QTimer::singleShot(1000, this, &LinuxUpdateManager::restartApplication);
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

void LinuxUpdateManager::restartApplication()
{
    qInfo() << "Restarting application from:" << m_newAppImagePath;

    // Verify the new AppImage exists and is executable
    QFileInfo appImageInfo(m_newAppImagePath);
    if (!appImageInfo.exists()) {
        qCritical() << "New AppImage does not exist:" << m_newAppImagePath;
        emit installationFinished(false, 
            "Failed to restart: Updated AppImage not found.\n"
            "Please launch the application manually from:\n" + m_newAppImagePath);
        return;
    }

    if (!appImageInfo.isExecutable()) {
        qCritical() << "New AppImage is not executable:" << m_newAppImagePath;
        emit installationFinished(false, 
            "Failed to restart: Updated AppImage is not executable.\n"
            "Please check file permissions and launch manually.");
        return;
    }

    // Verify file size is reasonable (not empty or corrupted)
    qint64 fileSize = appImageInfo.size();
    if (fileSize < 1000000) {  // Less than 1MB is definitely wrong
        qCritical() << "New AppImage file size is suspiciously small:" << fileSize << "bytes";
        emit installationFinished(false, 
            "Failed to restart: Downloaded file appears to be corrupted.\n"
            "File size: " + QString::number(fileSize) + " bytes\n"
            "Please check your network connection and try again.");
        return;
    }

    qInfo() << "New AppImage verified: size =" << fileSize << "bytes";

    // Launch the new AppImage directly (no helper script needed!)
    qInfo() << "Launching new AppImage...";
    bool success = QProcess::startDetached(m_newAppImagePath, QStringList());

    if (success) {
        qInfo() << "Successfully launched new AppImage - exiting current instance";
        // Exit current instance - the new version is now running
        QCoreApplication::quit();
    } else {
        qCritical() << "Failed to launch new AppImage";
        emit installationFinished(false, 
            "Update installed successfully, but failed to restart automatically.\n"
            "Please close this window and launch the application manually from:\n" + m_newAppImagePath);
    }
}

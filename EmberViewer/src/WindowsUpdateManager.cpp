/*
    EmberViewer - Windows Update Manager Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "WindowsUpdateManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

WindowsUpdateManager::WindowsUpdateManager(QObject *parent)
    : UpdateManager(parent)
    , m_tempDir(nullptr)
    , m_downloadFile(nullptr)
    , m_downloadReply(nullptr)
{
}

WindowsUpdateManager::~WindowsUpdateManager()
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

QString WindowsUpdateManager::selectAssetForPlatform(const QJsonObject &release)
{
    QJsonArray assets = release["assets"].toArray();

    // Look for Windows installer (e.g., "EmberViewer-Setup.exe" or "EmberViewer-x64-Setup.exe")
    for (const QJsonValue &assetValue : assets) {
        QJsonObject asset = assetValue.toObject();
        QString assetName = asset["name"].toString();

        // Match Windows installer files
        if (assetName.contains("Setup", Qt::CaseInsensitive) &&
            assetName.endsWith(".exe", Qt::CaseInsensitive)) {
            QString downloadUrl = asset["browser_download_url"].toString();
            qInfo() << "Selected Windows asset:" << assetName;
            return downloadUrl;
        }
    }

    qWarning() << "No suitable Windows installer asset found";
    return QString();
}

void WindowsUpdateManager::installUpdate(const UpdateInfo &updateInfo)
{
    qInfo() << "Starting Windows update installation for version:" << updateInfo.version;

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

    // Download the installer
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

    qInfo() << "Downloading installer to:" << downloadPath;

    QNetworkRequest request;
    request.setUrl(QUrl(updateInfo.downloadUrl));
    request.setRawHeader("User-Agent", QString("EmberViewer/%1").arg(getCurrentVersion()).toUtf8());

    m_downloadReply = m_networkManager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, 
            this, &WindowsUpdateManager::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile) {
            m_downloadFile->write(m_downloadReply->readAll());
        }
    });
    connect(m_downloadReply, &QNetworkReply::finished, 
            this, &WindowsUpdateManager::onDownloadFinished);

    emit installationStarted();
}

void WindowsUpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void WindowsUpdateManager::onDownloadFinished()
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

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    delete m_downloadFile;
    m_downloadFile = nullptr;

    // Execute the installer
    executeInstaller(downloadPath);
}

void WindowsUpdateManager::executeInstaller(const QString &installerPath)
{
    qInfo() << "Executing installer:" << installerPath;

    // Run installer with /S flag for silent installation
    // Note: NSIS installer will handle the update process
    bool success = QProcess::startDetached(installerPath, QStringList() << "/S");

    if (success) {
        qInfo() << "Installer started successfully - exiting application";
        emit installationFinished(true, "Installer started. The application will now close.");
        
        // Give the installer a moment to start, then quit
        QTimer::singleShot(1000, []() {
            QCoreApplication::quit();
        });
    } else {
        qWarning() << "Failed to start installer";
        emit installationFinished(false, "Failed to start installer.");
    }

    // Cleanup will happen when app quits
}

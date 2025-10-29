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
#include <QStandardPaths>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

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

    // Verify the installer file exists and is readable
    QFile installerFile(installerPath);
    if (!installerFile.exists()) {
        qWarning() << "Installer file does not exist:" << installerPath;
        emit installationFinished(false, "Installer file not found.");
        return;
    }

    // Copy installer to a persistent location to avoid temp directory cleanup issues
    QString persistentPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                           + "/EmberViewer-Update-" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".exe";
    
    qInfo() << "Copying installer to persistent location:" << persistentPath;
    
    // Remove existing file if present
    if (QFile::exists(persistentPath)) {
        qInfo() << "Removing existing update file at persistent location";
        QFile::remove(persistentPath);
    }
    
    if (!QFile::copy(installerPath, persistentPath)) {
        qWarning() << "Failed to copy installer to persistent location. Using original path instead.";
        qWarning() << "Original path:" << installerPath;
        // Fall back to original path
        persistentPath = installerPath;
    } else {
        qInfo() << "Installer copied successfully to persistent location";
        
        // Verify the copied file
        QFile copiedFile(persistentPath);
        if (!copiedFile.exists()) {
            qWarning() << "Copied installer file does not exist! Falling back to original path.";
            persistentPath = installerPath;
        } else {
            qInfo() << "Verified copied installer exists, size:" << copiedFile.size() << "bytes";
        }
    }

    // Run installer with /S (silent) and /UPDATE (auto-update mode) flags
    // /UPDATE tells the installer to wait for the app to close gracefully
    QStringList arguments;
    arguments << "/S" << "/UPDATE";
    
    qInfo() << "Starting installer with arguments:" << arguments;

#ifdef Q_OS_WIN
    // Use Windows API for better UAC handling and process launching
    // Convert QString to wide string for Windows API
    std::wstring installerPathW = persistentPath.toStdWString();
    std::wstring argumentsW = arguments.join(" ").toStdWString();
    
    // Use ShellExecuteEx for proper UAC elevation
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";  // Request elevation
    sei.lpFile = installerPathW.c_str();
    sei.lpParameters = argumentsW.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    qInfo() << "Attempting to launch installer with UAC elevation";
    
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            DWORD pid = GetProcessId(sei.hProcess);
            qInfo() << "Installer started successfully with PID" << pid;
            CloseHandle(sei.hProcess);
            
            emit installationFinished(true, "Installer started. The application will now close.");
            
            // Signal success and quit the application
            // The installer will wait for us to close (via /UPDATE flag)
            QTimer::singleShot(500, []() {
                qInfo() << "Closing application for update installation";
                QCoreApplication::quit();
            });
        } else {
            qWarning() << "Installer started but no process handle received";
            emit installationFinished(false, "Failed to verify installer startup.");
        }
    } else {
        DWORD error = GetLastError();
        QString errorMsg;
        
        // Provide user-friendly error messages
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
                errorMsg = "Installer file not found.";
                break;
            case ERROR_ACCESS_DENIED:
                errorMsg = "Access denied. Administrator privileges may be required.";
                break;
            case ERROR_CANCELLED:
                errorMsg = "Installation cancelled by user (UAC prompt declined).";
                break;
            default:
                errorMsg = QString("System error %1").arg(error);
                break;
        }
        
        qWarning() << "ShellExecuteEx failed:" << errorMsg << "(error code:" << error << ")";
        
        // Fall back to QProcess if ShellExecuteEx fails
        qInfo() << "Falling back to QProcess::startDetached";
        QProcess process;
        process.setProgram(persistentPath);
        process.setArguments(arguments);
        
        qint64 pid;
        bool success = process.startDetached(&pid);
        
        if (success) {
            qInfo() << "Installer started successfully with PID" << pid << "(fallback method)";
            emit installationFinished(true, "Installer started. The application will now close.");
            
            QTimer::singleShot(500, []() {
                QCoreApplication::quit();
            });
        } else {
            qWarning() << "Failed to start installer with both methods. QProcess error:" << process.errorString();
            emit installationFinished(false, 
                QString("Failed to start installer.\n\nShellExecuteEx: %1\nQProcess: %2\n\nPlease try downloading and running the installer manually.")
                    .arg(errorMsg)
                    .arg(process.errorString()));
        }
    }
#else
    // Non-Windows platform (shouldn't happen in WindowsUpdateManager, but for safety)
    QProcess process;
    process.setProgram(persistentPath);
    process.setArguments(arguments);
    
    qint64 pid;
    bool success = process.startDetached(&pid);
    
    if (success) {
        qInfo() << "Installer started successfully with PID" << pid;
        emit installationFinished(true, "Installer started. The application will now close.");
        
        QTimer::singleShot(500, []() {
            QCoreApplication::quit();
        });
    } else {
        qWarning() << "Failed to start installer. Error:" << process.errorString();
        emit installationFinished(false, QString("Failed to start installer: %1").arg(process.errorString()));
    }
#endif

    // Don't cleanup temp directory here - the copied installer will remain accessible
    // The installer will clean itself up after completion
}

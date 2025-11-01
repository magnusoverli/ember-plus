/*
    EmberViewer - Cross-platform Ember+ Protocol Viewer
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QMutex>
#include <QDebug>
#include <QIcon>
#include <QLoggingCategory>
#include "MainWindow.h"
#include "version.h"

static QFile *logFile = nullptr;
static QMutex logMutex;
static MainWindow *globalMainWindow = nullptr;
static bool enableQtInternalLogging = false;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&logMutex);
    
    QString level;
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO "; break;
        case QtWarningMsg:  level = "WARN "; break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp).arg(level).arg(msg);
    
    // Determine if this is an application message or a Qt internal message
    // Use Qt's category system (proper way to filter)
    QString category = context.category ? QString(context.category) : QString("default");
    bool isQtInternal = category.startsWith("qt.");
    
    // Check if we should filter out Qt internal messages
    bool shouldLog = !isQtInternal || (globalMainWindow && globalMainWindow->isQtInternalLoggingEnabled());
    
    // LOG FILE: Write filtered messages (Debug, Info, Warning, Error, Fatal)
    if (logFile && logFile->isOpen() && shouldLog) {
        QTextStream stream(logFile);
        stream << logLine << "\n";
        stream.flush();
    }
    
    // GUI CONSOLE: Show WARNING+ application messages only (filter Qt internals and Debug/Info)
    if (globalMainWindow && type >= QtWarningMsg && !isQtInternal) {
        QString guiMessage = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz")).arg(msg);
        QMetaObject::invokeMethod(globalMainWindow, "appendToConsole", Qt::DirectConnection, Q_ARG(QString, guiMessage));
    }
    
    // STDERR: Write INFO+ filtered messages
    if (type >= QtInfoMsg && shouldLog) {
        fprintf(stderr, "%s\n", logLine.toUtf8().constData());
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application metadata
    QApplication::setApplicationName("EmberViewer");
    QApplication::setApplicationVersion(EMBERVIEWER_VERSION_STRING);
    QApplication::setOrganizationName("Magnus Overli");
    QApplication::setOrganizationDomain("github.com/magnusoverli");
    
    // Set desktop file name for Wayland (helps window managers find the correct icon)
    // Must match the desktop file name (without .desktop extension)
    QApplication::setDesktopFileName("EmberViewer.desktop");
    
    // Set application icon (used for window decorations and taskbar)
    // Try to load from Qt resources first (embedded in binary)
    QIcon appIcon(":/icon.png");
    if (!appIcon.isNull() && !appIcon.availableSizes().isEmpty()) {
        QApplication::setWindowIcon(appIcon);
    } else {
        // Fallback: try to load from system theme (for installed applications)
        appIcon = QIcon::fromTheme("emberviewer");
        if (!appIcon.isNull()) {
            QApplication::setWindowIcon(appIcon);
        }
    }
    
    // Set up logging infrastructure
    // Log routing:
    //   - Log file: ALL messages (debug, info, warning, error, fatal)
    //   - stderr: INFO+ messages (info, warning, error, fatal) 
    //   - GUI console: WARNING+ application messages only (warning, error, fatal, no Qt internals)
    // This provides complete logs for troubleshooting while keeping UI clean
    
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/EmberViewer/logs";
    QDir().mkpath(logDir);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString logPath = logDir + "/emberviewer_" + timestamp + ".log";
    logFile = new QFile(logPath);
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        // Qt logging filter rules (runtime control)
        // Enable all levels - filtering is done in messageHandler based on output destination
        QLoggingCategory::setFilterRules(
            "*.debug=true\n"            // Enable debug globally (messageHandler filters per-destination)
            "*.info=true\n"             // Enable info globally
            "*.warning=true\n"          // Enable warnings
            "*.critical=true"           // Enable critical/fatal
        );
        qInstallMessageHandler(messageHandler);
        qInfo() << "EmberViewer started - Version:" << QApplication::applicationVersion();
        qInfo() << "Log file:" << logPath;
    } else {
        fprintf(stderr, "Failed to open log file: %s\n", logPath.toUtf8().constData());
    }
    
    MainWindow window;
    globalMainWindow = &window;
    window.show();
    
    int result = app.exec();
    
    globalMainWindow = nullptr;
    
    qWarning() << "EmberViewer shutting down";
    
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}


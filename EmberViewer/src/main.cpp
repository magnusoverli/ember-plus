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
#include "MainWindow.h"
#include "version.h"

static QFile *logFile = nullptr;
static QMutex logMutex;
static MainWindow *globalMainWindow = nullptr;

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
    
    if (logFile && logFile->isOpen()) {
        QTextStream stream(logFile);
        stream << logLine << "\n";
        stream.flush();
    }
    
    // Determine if this is an Ember+ application message or a Qt system message
    bool isEmberMessage = false;
    QString category = context.category ? QString(context.category) : QString();
    
    // Qt system messages typically have categories starting with "qt." (e.g., "qt.qpa.wayland", "qt.network")
    // EmberViewer application messages come from "default" category or have no category
    if (category.startsWith("qt.")) {
        // This is a Qt internal message - don't show in console
        isEmberMessage = false;
    } else if (category == "default" || category.isEmpty()) {
        // For default/empty category, check if it's from our source files
        QString file = context.file ? QString(context.file) : QString();
        
        // If we have file context and it's from our code, it's an Ember message
        if (!file.isEmpty()) {
            isEmberMessage = file.contains("EmberViewer") || 
                            file.contains("MainWindow") || 
                            file.contains("EmberConnection") ||
                            file.contains("EmberProvider") ||
                            file.contains("MatrixWidget") ||
                            file.contains("DeviceSnapshot") ||
                            file.contains("ParameterDelegate") ||
                            file.contains("EmulatorWindow") ||
                            file.contains("FunctionInvocationDialog");
        } else {
            // No file context - check if message looks like a Qt warning
            // Common Qt warnings contain these patterns
            if (msg.contains("QWindow::") || 
                msg.contains("QPlatform") ||
                msg.contains("Wayland") ||
                msg.contains("QWidget::") ||
                msg.contains("QObject::") ||
                msg.toLower().contains("does not support")) {
                isEmberMessage = false;
            } else {
                // Assume it's our message if we can't determine otherwise
                isEmberMessage = true;
            }
        }
    } else {
        // Unknown category - assume it's an Ember message
        isEmberMessage = true;
    }
    
    // Only show Ember+ messages in the GUI console
    // Filter out Qt system warnings (like Wayland warnings)
    if (globalMainWindow && type != QtDebugMsg && isEmberMessage) {
        QString guiMessage = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz")).arg(msg);
        QMetaObject::invokeMethod(globalMainWindow, "appendToConsole", Qt::QueuedConnection, Q_ARG(QString, guiMessage));
    }
    
    // Only write INFO and above to stderr (same filtering as console)
    if (type != QtDebugMsg) {
        fprintf(stderr, "%s\n", logLine.toUtf8().constData());
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QApplication::setApplicationName("EmberViewer");
    QApplication::setApplicationVersion(EMBERVIEWER_VERSION_STRING);
    QApplication::setOrganizationName("Magnus Overli");
    QApplication::setOrganizationDomain("github.com/magnusoverli");
    
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/EmberViewer/logs";
    QDir().mkpath(logDir);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString logPath = logDir + "/emberviewer_" + timestamp + ".log";
    logFile = new QFile(logPath);
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
        qInfo() << "EmberViewer started";
        qInfo() << "Log file:" << logPath;
        qInfo() << "Version:" << QApplication::applicationVersion();
    } else {
        fprintf(stderr, "Failed to open log file: %s\n", logPath.toUtf8().constData());
    }
    
    MainWindow window;
    globalMainWindow = &window;
    window.show();
    
    int result = app.exec();
    
    globalMainWindow = nullptr;
    
    qInfo() << "EmberViewer shutting down";
    
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    return result;
}


/*
 * EmberViewer - Cross-platform Ember+ Protocol Viewer
 * Copyright (c) 2025 Magnus Overli
 * 
 * This application provides a modern, cross-platform GUI for browsing
 * and controlling Ember+ providers.
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
    
    // Only show INFO and above (ERROR, CRITICAL, WARNING, INFO) in the GUI console
    // DEBUG and lower are only written to the log file
    if (globalMainWindow && type != QtDebugMsg) {
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
    QApplication::setApplicationVersion("1.0.0");
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


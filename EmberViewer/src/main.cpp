







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
    
    
    
    QString category = context.category ? QString(context.category) : QString("default");
    bool isQtInternal = category.startsWith("qt.");
    
    
    bool shouldLog = !isQtInternal || (globalMainWindow && globalMainWindow->isQtInternalLoggingEnabled());
    
    
    if (logFile && logFile->isOpen() && shouldLog) {
        QTextStream stream(logFile);
        stream << logLine << "\n";
        stream.flush();
    }
    
    
    if (globalMainWindow && type >= QtWarningMsg && type != QtInfoMsg && !isQtInternal) {
        QString guiMessage = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz")).arg(msg);
        QMetaObject::invokeMethod(globalMainWindow, "appendToConsole", Qt::DirectConnection, Q_ARG(QString, guiMessage));
    }
    
    
    if (type >= QtInfoMsg && shouldLog) {
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
    
    
    
    QApplication::setDesktopFileName("EmberViewer.desktop");
    
    
    
    QIcon appIcon(":/icon.png");
    if (!appIcon.isNull() && !appIcon.availableSizes().isEmpty()) {
        QApplication::setWindowIcon(appIcon);
    } else {
        
        appIcon = QIcon::fromTheme("emberviewer");
        if (!appIcon.isNull()) {
            QApplication::setWindowIcon(appIcon);
        }
    }
    
    
    
    
    
    
    
    
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/EmberViewer/logs";
    QDir().mkpath(logDir);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString logPath = logDir + "/emberviewer_" + timestamp + ".log";
    logFile = new QFile(logPath);
    
    
    
    QLoggingCategory::setFilterRules(
        "qt.*.debug=false\n"        
        "qt.*.info=false\n"         
        "*.warning=true\n"          
        "*.critical=true"           
    );
    
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
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


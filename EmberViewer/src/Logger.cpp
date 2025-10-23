/*
 * Logger.cpp - Cross-platform structured logging implementation
 */

#include "Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonArray>
#include <QUuid>
#include <QDebug>

Logger* Logger::s_instance = nullptr;

Logger* Logger::instance()
{
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_minimumLevel(Level::Info)
    , m_maxLogFileSize(10 * 1024 * 1024)  // 10MB default
    , m_maxLogFiles(5)
    , m_sessionStart(QDateTime::currentDateTime())
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Logger::~Logger()
{
    shutdown();
}

void Logger::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    // Determine log directory based on platform
    // Linux: ~/.local/share/EmberViewer/logs/
    // Windows: C:/Users/<user>/AppData/Local/EmberViewer/logs/
    // macOS: ~/Library/Application Support/EmberViewer/logs/
    m_logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    
    if (m_logDirectory.isEmpty()) {
        // Fallback to temp directory
        m_logDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        qWarning() << "Could not determine app data location, using temp:" << m_logDirectory;
    }
    
    m_logDirectory = QDir(m_logDirectory).filePath("logs");
    
    // Create log directory if it doesn't exist
    QDir logDir(m_logDirectory);
    if (!logDir.exists()) {
        if (!logDir.mkpath(".")) {
            qCritical() << "Failed to create log directory:" << m_logDirectory;
            return;
        }
    }
    
    // Create log file with timestamp
    QString timestamp = m_sessionStart.toString("yyyy-MM-dd_HH-mm-ss");
    QString logFileName = QString("ember-viewer_%1.log").arg(timestamp);
    m_logFilePath = logDir.filePath(logFileName);
    
    // Open log file
    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCritical() << "Failed to open log file:" << m_logFilePath;
        return;
    }
    
    m_logStream.setDevice(&m_logFile);
    
    // Write session header
    QJsonObject sessionInfo;
    sessionInfo["session_id"] = m_sessionId;
    sessionInfo["session_start"] = m_sessionStart.toString(Qt::ISODate);
    sessionInfo["application"] = QCoreApplication::applicationName();
    sessionInfo["version"] = QCoreApplication::applicationVersion();
    sessionInfo["qt_version"] = qVersion();
    
#ifdef Q_OS_LINUX
    sessionInfo["platform"] = "linux";
#elif defined(Q_OS_WIN)
    sessionInfo["platform"] = "windows";
#elif defined(Q_OS_MACOS)
    sessionInfo["platform"] = "macos";
#else
    sessionInfo["platform"] = "unknown";
#endif
    
    // Log session start
    info("application", "Session started", sessionInfo);
    
    // Install Qt message handler to capture qDebug/qWarning/etc
    qInstallMessageHandler(Logger::messageHandler);
    
    // Clean up old logs
    cleanupOldLogs();
    
    qInfo() << "Logging initialized. Log file:" << m_logFilePath;
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_logFile.isOpen()) {
        info("application", "Session ended");
        m_logStream.flush();
        m_logFile.close();
    }
    
    // Restore default message handler
    qInstallMessageHandler(nullptr);
}

QString Logger::logFilePath() const
{
    return m_logFilePath;
}

QString Logger::logDirectory() const
{
    return m_logDirectory;
}

void Logger::log(Level level, const QString &category, const QString &message, const QJsonObject &metadata)
{
    writeLog(level, category, message, QString(), QString(), 0, metadata);
}

void Logger::debug(const QString &category, const QString &message, const QJsonObject &metadata)
{
    log(Level::Debug, category, message, metadata);
}

void Logger::info(const QString &category, const QString &message, const QJsonObject &metadata)
{
    log(Level::Info, category, message, metadata);
}

void Logger::warning(const QString &category, const QString &message, const QJsonObject &metadata)
{
    log(Level::Warning, category, message, metadata);
}

void Logger::error(const QString &category, const QString &message, const QJsonObject &metadata)
{
    log(Level::Error, category, message, metadata);
}

void Logger::critical(const QString &category, const QString &message, const QJsonObject &metadata)
{
    log(Level::Critical, category, message, metadata);
}

void Logger::setMinimumLevel(Level level)
{
    m_minimumLevel = level;
}

void Logger::setMaxLogFileSize(qint64 bytes)
{
    m_maxLogFileSize = bytes;
}

void Logger::setMaxLogFiles(int count)
{
    m_maxLogFiles = count;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Logger *logger = Logger::instance();
    if (!logger) {
        return;
    }
    
    Level level = qtMsgTypeToLevel(type);
    
    // Extract category and message
    QString category = context.category ? QString(context.category) : "qt";
    QString file = context.file ? QString(context.file) : QString();
    QString function = context.function ? QString(context.function) : QString();
    
    // Write to log file
    logger->writeLog(level, category, msg, file, function, context.line, QJsonObject());
}

void Logger::writeLog(Level level, const QString &category, const QString &message, 
                     const QString &file, const QString &function, int line,
                     const QJsonObject &metadata)
{
    QMutexLocker locker(&m_mutex);
    
    // Check minimum level
    if (level < m_minimumLevel) {
        return;
    }
    
    if (!m_logFile.isOpen()) {
        return;
    }
    
    // Check if rotation is needed
    rotateLogsIfNeeded();
    
    // Build JSON log entry
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = levelToString(level);
    logEntry["category"] = category;
    logEntry["message"] = message;
    logEntry["session_id"] = m_sessionId;
    
    // Add source location if available
    if (!file.isEmpty() || !function.isEmpty()) {
        QJsonObject source;
        if (!file.isEmpty()) {
            source["file"] = file;
        }
        if (!function.isEmpty()) {
            source["function"] = function;
        }
        if (line > 0) {
            source["line"] = line;
        }
        logEntry["source"] = source;
    }
    
    // Add metadata if provided
    if (!metadata.isEmpty()) {
        logEntry["metadata"] = metadata;
    }
    
    // Write as single-line JSON
    QJsonDocument doc(logEntry);
    m_logStream << doc.toJson(QJsonDocument::Compact) << "\n";
    m_logStream.flush();
}

void Logger::rotateLogsIfNeeded()
{
    // Check current file size
    if (m_logFile.size() >= m_maxLogFileSize) {
        // Close current file
        m_logStream.flush();
        m_logFile.close();
        
        // Create new log file
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString logFileName = QString("ember-viewer_%1.log").arg(timestamp);
        m_logFilePath = QDir(m_logDirectory).filePath(logFileName);
        
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open new log file:" << m_logFilePath;
            return;
        }
        
        m_logStream.setDevice(&m_logFile);
        
        // Log rotation event
        QJsonObject metadata;
        metadata["reason"] = "size_limit_reached";
        info("logging", "Log file rotated", metadata);
        
        // Clean up old logs
        cleanupOldLogs();
    }
}

void Logger::cleanupOldLogs()
{
    QDir logDir(m_logDirectory);
    QStringList filters;
    filters << "ember-viewer_*.log";
    
    QFileInfoList logFiles = logDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    // Keep only the most recent N files
    if (logFiles.size() > m_maxLogFiles) {
        for (int i = m_maxLogFiles; i < logFiles.size(); ++i) {
            QString oldLogPath = logFiles[i].absoluteFilePath();
            if (QFile::remove(oldLogPath)) {
                qInfo() << "Removed old log file:" << oldLogPath;
            }
        }
    }
}

QString Logger::levelToString(Level level)
{
    switch (level) {
        case Level::Debug:    return "DEBUG";
        case Level::Info:     return "INFO";
        case Level::Warning:  return "WARNING";
        case Level::Error:    return "ERROR";
        case Level::Critical: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

Logger::Level Logger::qtMsgTypeToLevel(QtMsgType type)
{
    switch (type) {
        case QtDebugMsg:    return Level::Debug;
        case QtInfoMsg:     return Level::Info;
        case QtWarningMsg:  return Level::Warning;
        case QtCriticalMsg: return Level::Critical;
        case QtFatalMsg:    return Level::Critical;
        default:            return Level::Info;
    }
}


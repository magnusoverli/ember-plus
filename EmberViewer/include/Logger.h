/*
 * Logger.h - Cross-platform structured logging to file
 * 
 * Provides JSON-formatted logging with automatic rotation and
 * platform-appropriate log file locations.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    // Get singleton instance
    static Logger* instance();
    
    // Initialize logging system
    void initialize();
    
    // Shutdown logging (flush and close files)
    void shutdown();
    
    // Get log file path
    QString logFilePath() const;
    
    // Get log directory path
    QString logDirectory() const;
    
    // Log a structured message
    void log(Level level, const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    
    // Convenience methods
    void debug(const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    void info(const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    void warning(const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    void error(const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    void critical(const QString &category, const QString &message, const QJsonObject &metadata = QJsonObject());
    
    // Configure logging
    void setMinimumLevel(Level level);
    void setMaxLogFileSize(qint64 bytes);  // Default: 10MB
    void setMaxLogFiles(int count);         // Default: 5 files
    
private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    
    // Qt message handler callback
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    
    // Internal logging
    void writeLog(Level level, const QString &category, const QString &message, 
                  const QString &file, const QString &function, int line,
                  const QJsonObject &metadata);
    
    // Log rotation
    void rotateLogsIfNeeded();
    void cleanupOldLogs();
    
    // Convert enum to string
    static QString levelToString(Level level);
    static Level qtMsgTypeToLevel(QtMsgType type);
    
    // Singleton instance
    static Logger *s_instance;
    
    // File handling
    QFile m_logFile;
    QTextStream m_logStream;
    QMutex m_mutex;
    
    // Configuration
    Level m_minimumLevel;
    qint64 m_maxLogFileSize;
    int m_maxLogFiles;
    QString m_logDirectory;
    QString m_logFilePath;
    
    // Session info
    QDateTime m_sessionStart;
    QString m_sessionId;
};

// Global convenience macros for structured logging
#define LOG_DEBUG(category, message, ...) \
    Logger::instance()->debug(category, message, ##__VA_ARGS__)

#define LOG_INFO(category, message, ...) \
    Logger::instance()->info(category, message, ##__VA_ARGS__)

#define LOG_WARNING(category, message, ...) \
    Logger::instance()->warning(category, message, ##__VA_ARGS__)

#define LOG_ERROR(category, message, ...) \
    Logger::instance()->error(category, message, ##__VA_ARGS__)

#define LOG_CRITICAL(category, message, ...) \
    Logger::instance()->critical(category, message, ##__VA_ARGS__)

#endif // LOGGER_H


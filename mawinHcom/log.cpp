#include "log.h"
#include <QDebug>

Logger* Logger::s_instance = nullptr;

Logger* Logger::instance()
{
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

Logger::Logger()
    : m_minLevel(DEBUG)
    , m_initialized(false)
{
}

void Logger::init(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_initialized = true;

    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&file);
        out << "=== Application Started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " ===\n";
        file.close();
    }
}

void Logger::debug(const QString &msg)
{
    log(DEBUG, msg);
}

void Logger::info(const QString &msg)
{
    log(INFO, msg);
}

void Logger::warning(const QString &msg)
{
    log(WARNING, msg);
}

void Logger::error(const QString &msg)
{
    log(ERROR, msg);
}

void Logger::fatal(const QString &msg)
{
    log(FATAL, msg);
}

void Logger::log(Level level, const QString &msg)
{
    if (!m_initialized) {
        return;
    }
    QMutexLocker locker(&m_mutex);

    QString message = QString("[%1] [%2] %3")
                          .arg(currentTimestamp())
                          .arg(levelToString(level))
                          .arg(msg);

    writeToFile(message);
    emit logMessage(message);
}

QString Logger::levelToString(Level level)
{
    switch (level) {
    case DEBUG:   return "DEBUG";
    case INFO:    return "INFO";
    case WARNING: return "WARNING";
    case ERROR:   return "ERROR";
    case FATAL:   return "FATAL";
    default:      return "UNKNOWN";
    }
}

QString Logger::currentTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

void Logger::writeToFile(const QString &message)
{
    QFile file(m_filePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << message << "\n";
        file.close();
    }
}
#include "Log.h"
#include <QDebug>

QString Logger::m_filePath;
Logger::Level Logger::m_minLevel = DEBUG;
QMutex Logger::m_mutex;
bool Logger::m_initialized = false;

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
    Log(DEBUG, msg);
}

void Logger::info(const QString &msg)
{
    Log(INFO, msg);
}

void Logger::warning(const QString &msg)
{
    Log(WARNING, msg);
}

void Logger::error(const QString &msg)
{
    Log(ERROR, msg);
}

void Logger::fatal(const QString &msg)
{
    Log(FATAL, msg);
}

void Logger::Log(Level level, const QString &msg)
{
    if (!m_initialized) {
        qDebug() << msg;
        return;
    }
    QMutexLocker locker(&m_mutex);

    QString message = QString::asprintf("[%s] [%s] %s",
                                        qPrintable(currentTimestamp()),
                                        qPrintable(levelToString(level)),
                                        qPrintable(msg));
    qDebug().noquote() << message;

    // 输出到文件
    writeToFile(message);
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

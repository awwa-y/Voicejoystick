#include "log.h"
#include <QDebug>

Logger* Logger::s_instance = nullptr;

// 单例：第一次调用时创建，之后直接返回同一个实例
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

// 初始化：记录文件路径，清空旧文件并写启动时间头
// 必须先 init 再写日志，没 init 的 log() 直接丢弃
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

// ===== 五个级别便捷入口，全部转调 log() =====

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

// 通用入口：拼 "[时间] [级别] 消息"，写文件的同时发信号
// 持锁保证多线程写文件不交叉
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

// 枚举级别转字符串
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

// 追加写一行。每次打开-写-关闭，简单可靠，日志量不大时够用
void Logger::writeToFile(const QString &message)
{
    QFile file(m_filePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << message << "\n";
        file.close();
    }
}
#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <QObject>

class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger* instance();
    explicit Logger();

    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    void init(const QString &filePath = "app.log");
    void debug(const QString &msg);
    void info(const QString &msg);
    void warning(const QString &msg);
    void error(const QString &msg);
    void fatal(const QString &msg);
    void log(Level level, const QString &msg);

signals:
    void logMessage(const QString &message);

private:
    // 禁用拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QString levelToString(Level level);
    QString currentTimestamp();
    void writeToFile(const QString &message);

    QString m_filePath;
    Level m_minLevel;
    QMutex m_mutex;
    bool m_initialized;

    static Logger* s_instance;
};

#endif
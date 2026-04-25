#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

class Logger
{
public:
    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    static void init(const QString &filePath = "app.log");
    static void debug(const QString &msg);
    static void info(const QString &msg);
    static void warning(const QString &msg);
    static void error(const QString &msg);
    static void fatal(const QString &msg);
    static void Log(Level level, const QString &msg);

private:
    static QString levelToString(Level level);
    static QString currentTimestamp();
    static void writeToFile(const QString &message);

    static QString m_filePath;
    static Level m_minLevel;
    static QMutex m_mutex;
    static bool m_initialized;
};

#endif
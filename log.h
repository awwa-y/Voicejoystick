#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <QObject>

// 单例日志器：写文件 + 发信号给 UI
// 多线程使用安全（写文件持锁）
class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger* instance();   // 懒加载单例入口
    explicit Logger();

    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    void init(const QString &filePath = "app.log");   // 打开日志文件，写启动头
    void debug(const QString &msg);
    void info(const QString &msg);
    void warning(const QString &msg);
    void error(const QString &msg);
    void fatal(const QString &msg);
    void log(Level level, const QString &msg);        // 通用入口，格式化后写文件+发信号

signals:
    void logMessage(const QString &message);   // 每条日志都发出，UI 想显示就接

private:
    // 单例禁拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QString levelToString(Level level);       // 枚举转字符串
    QString currentTimestamp();
    void writeToFile(const QString &message); // 追加写一行，每次开关文件（简单但慢，够用）

    QString m_filePath;
    Level m_minLevel;
    QMutex m_mutex;          // 写文件互斥，多线程调用不交叉
    bool m_initialized;

    static Logger* s_instance;
};

#endif

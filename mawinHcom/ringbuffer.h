#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <atomic>
#include <QWaitCondition>

class RingBuffer : public QObject
{
    Q_OBJECT
public:
    enum WritePolicy {
        OverwriteOld,  // 覆盖旧数据
        Block,         // 阻塞直到有空间
        DiscardNew     // 丢弃新数据
    };

    explicit RingBuffer(int size = 4096, QObject *parent = nullptr);
    ~RingBuffer();
    
    // 写入数据，返回实际写入长度
    int write(const char *data, int len, WritePolicy policy = OverwriteOld);
    int write(const QByteArray &data, WritePolicy policy = OverwriteOld);
    
    // 读取数据，返回实际读取长度
    int read(char *dest, int maxLen);
    QByteArray readAll();
    
    bool hasData() const;
    int size() const;
    int freeSpace() const;
    bool isEmpty() const;
    bool isFull() const;
    
    // 操作
    void clear();

signals:
    void newDataAvailable();

private:
    char *buffer;
    int capacity;
    int head;
    int tail;
    std::atomic<int> count;  // 当前数据量
    mutable QMutex mutex;
    QWaitCondition m_notFull;
    QWaitCondition m_notEmpty;
private:
    int readLocked(char *dest, int maxLen);
};

#endif
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <atomic>

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
    
    // 状态查询
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
    char *buffer;       // 缓冲区
    int capacity;       // 总容量
    int head;           // 写入位置
    int tail;           // 读取位置
    std::atomic<int> count;  // 当前数据量
    mutable QMutex mutex;     // 互斥锁
};

#endif
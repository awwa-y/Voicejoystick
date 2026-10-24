#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <atomic>
#include <QWaitCondition>

// 线程安全环形缓冲区，串口线程和上层线程之间的数据中转
// 两个线程速度不一样：串口随时来数据（生产者），上层按自己节奏取（消费者）。
// 定长缓冲写满绕回头部继续写，内存固定不膨胀
//
// 索引约定：head = 下一个写入位置，tail = 下一个读取位置，count = 当前数据量
// 空：count == 0，满：count == capacity
// 线程安全：mutex 保护索引和 memcpy，count 用 atomic，查询接口免锁
class RingBuffer : public QObject
{
    Q_OBJECT
public:
    // 缓冲写满时的策略
    enum WritePolicy {
        OverwriteOld,  // 覆盖旧数据，实时流场景新数据重要
        Block,         // 预留阻塞（当前实现等同 DiscardNew）
        DiscardNew     // 丢弃新数据，不允许丢历史
    };

    explicit RingBuffer(int size = 4096, QObject *parent = nullptr);
    ~RingBuffer();

    // 写入数据，返回实际写入长度
    int write(const char *data, int len, WritePolicy policy = OverwriteOld);
    int write(const QByteArray &data, WritePolicy policy = OverwriteOld);

    // 读取数据，返回实际读取长度
    int read(char *dest, int maxLen);
    QByteArray readAll();

    bool hasData() const;      // 有没有未读数据
    int size() const;          // 当前数据量（字节）
    int freeSpace() const;     // 剩余可写空间（字节）
    bool isEmpty() const;
    bool isFull() const;

    void clear();              // 清空，索引归零

signals:
    void newDataAvailable();   // 每次成功写入后发出，通知消费者来取

private:
    char *buffer;              // 底层定长数组
    int capacity;              // 总容量
    int head;                  // 写索引
    int tail;                  // 读索引
    std::atomic<int> count;    // 当前数据量，查询免锁
    mutable QMutex mutex;      // 保护索引移动和 memcpy
    QWaitCondition m_notFull;  // 预留
    QWaitCondition m_notEmpty; // 预留
private:
    int readLocked(char *dest, int maxLen);  // 已持锁版本的读，read/readAll 复用
};

#endif

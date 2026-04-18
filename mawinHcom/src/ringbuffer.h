#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QObject>
#include <QMutex>
#include <QPair>
#include <QByteArray>

class RingBuffer : public QObject
{
    Q_OBJECT
public:
    explicit RingBuffer(int size = 4096, QObject *parent = nullptr);
    ~RingBuffer();
    bool write(const QByteArray &data);
    QByteArray readAll();
    bool hasData();
    int size();
    void clear();

signals:
    void newDataAvailable();

private:
    char *buffer;
    int capacity;
    int head;
    int tail;
    int count;
    QMutex mutex;
    QAtomicInt atomicCount;
};

#endif
#include "ringbuffer.h"
#include <string.h>

RingBuffer::RingBuffer(int size, QObject *parent)
    : QObject(parent), capacity(size), head(0), tail(0), atomicCount(0)
{
    buffer = new char[capacity];
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

bool RingBuffer::write(const QByteArray &data)
{
    QMutexLocker locker(&mutex);

    int dataSize = data.size();

    // 清空缓冲区
    head = 0;
    tail = 0;

    // 复制数据
    int copySize = qMin(dataSize, capacity);
    memcpy(buffer, data.constData(), copySize);
    head = copySize;

    // 使用ref和dref操作原子变量
    atomicCount.storeRelease(copySize);

    return true;
}

QByteArray RingBuffer::readAll()
{
    QMutexLocker locker(&mutex);

    if (head == tail) {
        return QByteArray();
    }

    QByteArray result;
    if (head > tail) {
        result = QByteArray(buffer + tail, head - tail);
    } else {
        result = QByteArray(buffer + tail, capacity - tail);
        result.append(buffer, head);
    }

    head = 0;
    tail = 0;
    atomicCount.storeRelease(0);

    return result;
}

bool RingBuffer::hasData()
{
    return atomicCount.loadAcquire() > 0;
}

int RingBuffer::size()
{
    return atomicCount.loadAcquire();
}

void RingBuffer::clear()
{
    QMutexLocker locker(&mutex);
    head = 0;
    tail = 0;
    atomicCount.storeRelease(0);
}
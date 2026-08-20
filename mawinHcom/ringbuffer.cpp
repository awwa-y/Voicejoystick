#include "ringbuffer.h"
#include <string.h>

RingBuffer::RingBuffer(int size, QObject *parent)
    : QObject(parent), capacity(size), head(0), tail(0), count(0)
{
    buffer = new char[capacity];
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

int RingBuffer::write(const char *data, int len, WritePolicy policy)
{
    QMutexLocker locker(&mutex);
    if (len <= 0)
        return 0;

    int written = 0;
    int remaining = len;

    while (remaining > 0) {
        int free = capacity - count.load();

        if (free < remaining) {   // 空闲空间不足（不一定 <=0）
            switch (policy) {
            case OverwriteOld: {
                remaining = qMin(remaining, capacity);
                int need = remaining - free;
                if (need > 0) {
                    int discard = qMin(need, count.load());
                    tail = (tail + discard) % capacity;
                    count.fetch_sub(discard);
                }
                free = remaining;
                break;
            }
            case Block:
                return written;
            case DiscardNew:
                return written;
            default:
                return written;
            }
        }

        int writeSize = qMin(remaining, free);
        int writeToEnd = qMin(writeSize, capacity - head);

        memcpy(buffer + head, data + written, writeToEnd);
        head = (head + writeToEnd) % capacity;

        if (writeSize > writeToEnd) {
            int writeFromStart = writeSize - writeToEnd;
            memcpy(buffer, data + written + writeToEnd, writeFromStart);
            head = writeFromStart;
        }

        written += writeSize;
        remaining -= writeSize;
        count.fetch_add(writeSize);
    }

    emit newDataAvailable();
    return written;
}


int RingBuffer::write(const QByteArray &data, WritePolicy policy)
{
    return write(data.constData(), data.size(), policy);
}

int RingBuffer::read(char *dest, int maxLen)
{
    QMutexLocker locker(&mutex);
    return readLocked(dest, maxLen);
}

QByteArray RingBuffer::readAll()
{
    QMutexLocker locker(&mutex);

    if (count.load() == 0) {
        return QByteArray();
    }

    QByteArray result;
    result.resize(count.load());
    int actualRead = readLocked(result.data(), result.size());
    if (actualRead != result.size()) {
        result.resize(actualRead);
    }
    return result;
}

bool RingBuffer::hasData() const
{
    return count.load() > 0;
}

int RingBuffer::size() const
{
    return count.load();
}

int RingBuffer::freeSpace() const
{
    return capacity - count.load();
}

bool RingBuffer::isEmpty() const
{
    return count.load() == 0;
}

bool RingBuffer::isFull() const
{
    return count.load() >= capacity;
}

void RingBuffer::clear()
{
    QMutexLocker locker(&mutex);
    head = 0;
    tail = 0;
    count.store(0);
}

int RingBuffer::readLocked(char *dest, int maxLen)
{
    if (maxLen <= 0 || count.load() == 0) {
        return 0;
    }

    int readSize = qMin(maxLen, count.load());
    int readFromEnd = qMin(readSize, capacity - tail);

    memcpy(dest, buffer + tail, readFromEnd);
    tail = (tail + readFromEnd) % capacity;

    if (readSize > readFromEnd) {
        int readFromStart = readSize - readFromEnd;
        memcpy(dest + readFromEnd, buffer, readFromStart);
        tail = readFromStart;
    }

    count.fetch_sub(readSize);
    return readSize;
}
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
    
    if (len <= 0) {
        return 0;
    }
    
    int written = 0;
    int remaining = len;
    
    while (remaining > 0) {
        int free = freeSpace();
        
        if (free <= 0) {
            switch (policy) {
            case OverwriteOld: {
                // 覆盖旧数据，移动 tail
                int overwrite = remaining;
                tail = (tail + overwrite) % capacity;
                count.fetch_sub(overwrite);
                free = remaining;
                break;
            }
            case Block:
                // 阻塞（简单实现，实际可能需要条件变量）
                return written;

            case DiscardNew:
                // 丢弃新数据
                return written;

            default:
                // 默认情况，防止编译器警告
                return written;
            }
        }
        
        int writeSize = qMin(remaining, free);
        int writeToEnd = qMin(writeSize, capacity - head);
        
        // 写入数据
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
    
    if (maxLen <= 0 || count.load() == 0) {
        return 0;
    }
    
    int readSize = qMin(maxLen, count.load());
    int readFromEnd = qMin(readSize, capacity - tail);
    
    // 读取数据
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

QByteArray RingBuffer::readAll()
{
    QMutexLocker locker(&mutex);
    
    if (count.load() == 0) {
        return QByteArray();
    }
    
    QByteArray result;
    result.resize(count.load());
    
    read(result.data(), result.size());
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
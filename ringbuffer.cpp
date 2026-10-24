#include "ringbuffer.h"
#include <cstring>

// 构造：分配定长数组，索引归零
RingBuffer::RingBuffer(int size, QObject *parent)
    : QObject(parent), capacity(size), head(0), tail(0), count(0)
{
    buffer = new char[capacity];
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

// 写入核心。head 走到缓冲末尾要绕回 0，一条数据可能分成
// "尾部一段 + 头部一段"两次 memcpy，所以先算写到末尾能放多少，
// 放不下的再从 buffer[0] 接着放
// OverwriteOld：空间不够时先移动 tail 丢掉足够的旧数据腾地方
int RingBuffer::write(const char *data, int len, WritePolicy policy)
{
    QMutexLocker locker(&mutex);

    if (len <= 0) {
        return 0;
    }

    if (policy == OverwriteOld) {
        // 空间不够就把最老的数据丢掉，保证新数据能写进去
        int free = capacity - count.load();
        if (len > free) {
            int need = len - free;
            tail = (tail + need) % capacity;
            count.fetch_sub(need);
        }
    } else {
        // Block / DiscardNew：空间不足时写多少算多少，不覆盖
        len = qMin(len, capacity - count.load());
        if (len <= 0) {
            return 0;
        }
    }

    // 第一段：写到缓冲末尾
    int firstPart = qMin(len, capacity - head);
    memcpy(buffer + head, data, firstPart);

    // 第二段：绕回头部接着写
    int secondPart = len - firstPart;
    if (secondPart > 0) {
        memcpy(buffer, data + firstPart, secondPart);
    }

    head = (head + len) % capacity;
    count.fetch_add(len);

    emit newDataAvailable();
    return len;
}

// QByteArray 版写入，转调裸指针版本
int RingBuffer::write(const QByteArray &data, WritePolicy policy)
{
    return write(data.constData(), data.size(), policy);
}

// 读到调用方缓冲区
int RingBuffer::read(char *dest, int maxLen)
{
    QMutexLocker locker(&mutex);
    return readLocked(dest, maxLen);
}

// 取走全部数据。先按 count 预分配，读出多少算多少
// （读的过程中可能有并发写，所以实际读出可能少于预分配，需收缩）
QByteArray RingBuffer::readAll()
{
    QMutexLocker locker(&mutex);

    int currentCount = count.load();
    if (currentCount <= 0) {
        return QByteArray();
    }

    QByteArray result(currentCount, Qt::Uninitialized);
    int bytesRead = readLocked(result.data(), currentCount);

    if (bytesRead < currentCount) {
        result.resize(bytesRead);   // 并发写导致实际可读变少时收缩
    }

    return result;
}

// ===== 查询接口只读 count（atomic），不需要持锁 =====

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

// 清空：持锁归零索引。数据不用抹，count 判空就够了
void RingBuffer::clear()
{
    QMutexLocker locker(&mutex);
    head = 0;
    tail = 0;
    count.store(0);
}

// 已持锁版本的读，和 write 对称：读指针绕回头部时分两段 memcpy
// 先读到缓冲末尾，不够再从 buffer[0] 接着读
int RingBuffer::readLocked(char *dest, int maxLen)
{
    int currentCount = count.load();
    int toRead = qMin(maxLen, currentCount);
    if (toRead <= 0) {
        return 0;
    }

    // 第一段：从 tail 读到缓冲末尾
    int firstPart = qMin(toRead, capacity - tail);
    memcpy(dest, buffer + tail, firstPart);

    // 第二段：绕回头部接着读
    int secondPart = toRead - firstPart;
    if (secondPart > 0) {
        memcpy(dest + firstPart, buffer, secondPart);
    }

    tail = (tail + toRead) % capacity;
    count.fetch_sub(toRead);

    return toRead;
}

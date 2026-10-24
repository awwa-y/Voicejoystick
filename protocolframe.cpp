#include "protocolframe.h"
#include <QDebug>

// 构造：初始状态是等帧头，刚启动还没收到任何字节
ProtocolFrame::ProtocolFrame()
    : QObject(nullptr),
    currentState(ParseState::WaitForMagic)
{
}

// 组包：按帧序依次 append，校验和覆盖帧头之后到数据段末尾
QByteArray ProtocolFrame::pack(quint8 cmdType, quint8 deviceType, const QByteArray &data)
{
    if (data.size() > 255) {
        qWarning() << "ProtocolFrame::pack: data size exceeds 255, returning empty frame.";
        return QByteArray();
    }
    // data.size() 返回 qsizetype（64位），协议 dataLen 只占 1 字节，
    // 超长时上面的守卫直接拒绝，避免 append 时静默截断
    QByteArray frame;

    frame.append(static_cast<char>(MAGIC));
    frame.append(static_cast<char>(cmdType));
    frame.append(static_cast<char>(deviceType));
    frame.append(static_cast<char>(static_cast<quint8>(data.size())));
    frame.append(data);

    quint8 checksum = calculateChecksum(frame);
    frame.append(static_cast<char>(checksum));
    frame.append(static_cast<char>(TAIL));

    return frame;
}

// 校验和：跳过帧头（索引0），从索引1累加到末尾
// frameWithoutTail 是从 0xAA 到 data 末尾的部分，不含校验和和帧尾
quint8 ProtocolFrame::calculateChecksum(const QByteArray &frameWithoutTail)
{
    quint8 sum = 0;
    for (int i = 1; i < frameWithoutTail.size(); ++i) {
        sum += static_cast<quint8>(frameWithoutTail.at(i));
    }
    return sum;
}

// 状态机核心：每喂一个字节走一步
// 两个要点：
// 1. WaitForMagic 状态下不是 0xAA 的字节直接丢，流错位后能自动重新同步
// 2. 校验失败或帧尾不对时整帧丢弃，回到 WaitForMagic 重新找帧头。
//    帧头已经被消费掉了没法回退，只能这样
void ProtocolFrame::feed(quint8 byte)
{
    switch (currentState) {
    case ParseState::WaitForMagic:
        if (byte == MAGIC) {
            currentState = ParseState::WaitForCmdType;
        }
        // 不是 0xAA 就无视，继续等。垃圾字节在流式通信里是常态，
        // 每个都报错会刷屏
        break;

    case ParseState::WaitForCmdType:
        tempCmd = byte;
        currentState = ParseState::WaitForDeviceType;
        break;

    case ParseState::WaitForDeviceType:
        tempDevice = byte;
        currentState = ParseState::WaitForDataLen;
        break;

    case ParseState::WaitForDataLen:
        expectedLen = byte;
        tempData.clear();
        if (expectedLen == 0) {
            // 长度为 0，没有数据段，直接跳去收校验和
            currentState = ParseState::WaitForChecksum;
        } else {
            currentState = ParseState::WaitForData;
        }
        break;

    case ParseState::WaitForData:
        tempData.append(static_cast<char>(byte));
        if (tempData.size() == expectedLen) {
            currentState = ParseState::WaitForChecksum;
        }
        break;

    case ParseState::WaitForChecksum:{
        receivedChecksum = byte;

        // 把已收的部分重新拼出来算一遍校验
        // （帧头字节已经消费了，只能手工补回去）
        QByteArray frameWithoutTail;
        frameWithoutTail.append(static_cast<char>(MAGIC));
        frameWithoutTail.append(static_cast<char>(tempCmd));
        frameWithoutTail.append(static_cast<char>(tempDevice));
        frameWithoutTail.append(static_cast<char>(expectedLen));
        frameWithoutTail.append(tempData);

        quint8 calculated = calculateChecksum(frameWithoutTail);

        if (calculated == receivedChecksum) {
            currentState = ParseState::WaitForFrameTail;
        } else {
            emit errorReported("Checksum mismatch");
            currentState = ParseState::WaitForMagic;
        }
        break;
    }
    case ParseState::WaitForFrameTail:{
        if (byte == TAIL) {
            emit frameReceived(tempCmd, tempDevice, tempData);
            currentState = ParseState::WaitForMagic;
        } else {
            emit errorReported("Frame tail mismatch");
            currentState = ParseState::WaitForMagic;
        }
        break;
    }
    }

}

// 批量喂入：串口 readyRead 给的是一整块，逐字节走状态机切分
void ProtocolFrame::feed(const QByteArray &bytes){
    for (char byte : bytes) {
        feed(static_cast<quint8>(byte));
    }
}

// 状态机复位：丢弃攒了半截的帧，所有上下文清零，回到等帧头
// 串口错误重连后调用，避免拿旧上下文继续拼帧
void ProtocolFrame::reset()
{
    currentState = ParseState::WaitForMagic;
    tempData.clear();
    tempCmd = 0;
    tempDevice = 0;
    expectedLen = 0;
    receivedChecksum = 0;
}

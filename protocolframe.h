#ifndef PROTOCOLFRAME_H
#define PROTOCOLFRAME_H
#include <QObject>
#include <QByteArray>

// 协议层：负责 0xAA 七段式帧的组包和解析
// 帧格式: 0xAA | cmdType | deviceType | dataLen | data[N] | checksum | 0x55
// 之前组包解析逻辑散落在 MainWindow 和 CommandProcessor 里，改协议要改多处，
// 收到这里之后全项目只有这个类知道帧长什么样
//
// 解析为什么用状态机：串口是字节流，没有消息边界。一帧可能分几次到达（半包），
// 多帧也可能一次到达（粘包），只能逐字节喂给状态机攒帧
class ProtocolFrame : public QObject{
    Q_OBJECT
public:
    ProtocolFrame();

    // 组包：命令+设备+数据 拼成完整帧
    // data 超过 255 字节返回空（协议 dataLen 只有 1 字节，放不下）
    static QByteArray pack(quint8 cmdType,quint8 deviceType,const QByteArray &data);

    // 喂入单个字节，状态机走一步。不是 0xAA 开头的垃圾字节会被丢弃
    void feed(quint8 byte);

    // 批量喂入，内部逐字节转调上面的版本
    void feed(const QByteArray &bytes);

    // 状态机复位：丢弃攒了半截的帧，回到等帧头
    // 串口错误重连后调用，避免拿旧上下文继续拼帧
    void reset();
signals:
    // 解析出完整一帧且校验通过时发出，参数和 pack 的输入对应
    void frameReceived(quint8 cmdType, quint8 deviceType, const QByteArray &data);

    // 校验失败或帧尾不对时发出，当前帧丢弃
    void errorReported(const QString&);
private:
    static constexpr quint8 MAGIC = 0xAA;  // 帧头
    static constexpr quint8 TAIL = 0x55;   // 帧尾

    // 校验和：从索引1累加到末尾，uint8 溢出取模
    // 全项目唯一的校验实现，发送(pack)和解析(feed)共用
    static quint8 calculateChecksum(const QByteArray &frameWithoutTail);

    // 状态机的七个阶段，对应帧的七段
    enum class ParseState {
        WaitForMagic,       // 等 0xAA
        WaitForCmdType,     // 收命令类型 1 字节
        WaitForDeviceType,  // 收设备类型 1 字节
        WaitForDataLen,     // 收数据长度 1 字节
        WaitForData,        // 收 dataLen 个数据字节
        WaitForChecksum,    // 收校验和 1 字节并比对
        WaitForFrameTail    // 收帧尾 0x55，到这里一帧完整
    };

    ParseState currentState;   // 当前状态
    quint8 tempCmd;            // 正在拼的帧的命令类型
    quint8 tempDevice;         // 正在拼的帧的设备类型
    quint8 expectedLen;        // 本帧声明的数据长度
    QByteArray tempData;       // 收到的数据段
    quint8 receivedChecksum;   // 收到的校验和
};

#endif // PROTOCOLFRAME_H

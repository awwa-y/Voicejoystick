#ifndef GATEWAYPROTOCOL_H
#define GATEWAYPROTOCOL_H

#include <QtGlobal>
#include <QByteArray>

// 与 SomeIpGateway 的 25 字节协议头保持一致的线上格式（大端）。
// 上位机作为"网关的串口桥提供者"时，用这套协议和 Linux 上的 epoll 网关通信。
namespace gw {

constexpr quint32 kMagic      = 0xABCDEF01;
constexpr int     kHeaderSize = 25;

enum MsgType : quint8 {
    REQUEST   = 0x00,
    RESPONSE  = 0x02,
    NOTIFY    = 0x04,
    REGISTER  = 0x08,
    HEARTBEAT = 0x10,
};

enum ErrorCode : quint8 {
    OK                  = 0x00,
    NOT_READY           = 0x01,
    NOT_FOUND           = 0x02,
    TIMEOUT             = 0x03,
    INVALID_PAYLOAD     = 0x04,
    SERVICE_UNAVAILABLE = 0x05,
};

// 串口桥服务：把上位机的串口设备以服务形式注册到网关
constexpr quint32 SVC_SERIAL_BRIDGE = 0x0000F001;

// 线上头（逻辑结构，序列化时逐字节大端写入，避免结构体对齐问题）
struct Header {
    quint32 magic      = kMagic;
    quint32 serviceId  = 0;
    quint32 methodId   = 0;
    quint32 length     = 0;
    quint32 clientId   = 0;
    quint8  msgType    = 0;
    quint8  errorCode  = 0;
    quint8  version    = 0;
    quint8  reserved0  = 0;
    quint8  reserved1  = 0;
};

// 把 Header + payload 序列化成线上字节（大端）
inline QByteArray encodeFrame(const Header &h, const QByteArray &payload = QByteArray())
{
    QByteArray out;
    out.resize(kHeaderSize);
    auto put32 = [&out](int off, quint32 v) {
        out[off]     = char((v >> 24) & 0xFF);
        out[off + 1] = char((v >> 16) & 0xFF);
        out[off + 2] = char((v >> 8) & 0xFF);
        out[off + 3] = char(v & 0xFF);
    };
    put32(0,  h.magic);
    put32(4,  h.serviceId);
    put32(8,  h.methodId);
    put32(12, h.length);
    put32(16, h.clientId);
    out[20] = char(h.msgType);
    out[21] = char(h.errorCode);
    out[22] = char(h.version);
    out[23] = char(h.reserved0);
    out[24] = char(h.reserved1);
    out.append(payload);
    return out;
}

inline quint32 get32(const QByteArray &b, int off)
{
    return (quint32(quint8(b[off])) << 24) |
           (quint32(quint8(b[off + 1])) << 16) |
           (quint32(quint8(b[off + 2])) << 8) |
           quint32(quint8(b[off + 3]));
}

inline Header decodeHeader(const QByteArray &b, int off = 0)
{
    Header h;
    h.magic     = get32(b, off);
    h.serviceId = get32(b, off + 4);
    h.methodId  = get32(b, off + 8);
    h.length    = get32(b, off + 12);
    h.clientId  = get32(b, off + 16);
    h.msgType   = quint8(b[off + 20]);
    h.errorCode = quint8(b[off + 21]);
    h.version   = quint8(b[off + 22]);
    return h;
}

} // namespace gw

#endif // GATEWAYPROTOCOL_H

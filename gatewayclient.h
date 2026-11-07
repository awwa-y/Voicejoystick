#ifndef GATEWAYCLIENT_H
#define GATEWAYCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include "gatewayprotocol.h"

// 网关客户端，角色 = "串口桥提供者"（边缘代理）：
//   连接 Linux 上的 SomeIpGateway -> REGISTER "串口桥"服务 -> 周期心跳；
//   远端经网关下发的 REQUEST 载荷就是 AA..55 串口帧，由上层转给串口设备后回 RESPONSE。
// 这样上位机把本地串口设备接入了 TCP 网关，形成 远端 ↔ 网关 ↔ 上位机 ↔ STM32 的链路。
class GatewayClient : public QObject
{
    Q_OBJECT
public:
    explicit GatewayClient(QObject *parent = nullptr);
    ~GatewayClient();

    void connectToGateway(const QString &host, quint16 port);
    void disconnectFromGateway();
    bool isConnected() const;

    // 向网关回 RESPONSE（响应远端的 REQUEST）
    void respond(quint32 clientId, quint8 errorCode, const QByteArray &payload = QByteArray());

signals:
    void gatewayConnected();
    void gatewayDisconnected();
    void gatewayError(const QString &msg);
    // 远端经网关下发的帧（payload 是 AA..55 串口帧）
    void remoteFrame(quint32 clientId, quint32 methodId, const QByteArray &payload);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void sendHeartbeat();

private:
    void sendRegister();
    void drainFrames();

    QTcpSocket *m_socket;
    QByteArray  m_recvBuf;   // TCP 接收缓冲（半包/粘包）
    QTimer     *m_hbTimer;   // 心跳定时器
};

#endif // GATEWAYCLIENT_H

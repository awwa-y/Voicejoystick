#ifndef GATEWAYCLIENT_H
#define GATEWAYCLIENT_H

#include <QObject>
#include <QTcpSocket>

// 网关客户端:把串口收到的设备数据帧转发给 Linux epoll 网关,接收网关广播
// 角色 = "边缘代理":串口设备(STM32)经上位机接入 TCP 网络(工业 DTU 原理)
class GatewayClient : public QObject
{
    Q_OBJECT
public:
    explicit GatewayClient(QObject *parent = nullptr);
    ~GatewayClient();

    void connectToGateway(const QString &host, quint16 port);
    void disconnectFromGateway();
    bool isConnected() const;

    // 转发一帧串口数据到网关(帧自带 AA..55 边界)
    void forwardDeviceData(const QByteArray &frame);

signals:
    void gatewayConnected();
    void gatewayDisconnected();
    void gatewayError(const QString &msg);
    void gatewayMessage(const QByteArray &frame);   // 网关下发的完整帧

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QTcpSocket *m_socket;
    QByteArray m_recvBuf;      // TCP 接收缓冲(处理粘包,按帧边界切分)

    // 从接收缓冲扫描完整帧(AA...55),逐帧上抛
    void extractFrames();
};

#endif // GATEWAYCLIENT_H

#include "gatewayclient.h"
#include <QDebug>

// 构造:socket 不指定 parent,手动管理生命周期
GatewayClient::GatewayClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &GatewayClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &GatewayClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &GatewayClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &GatewayClient::onErrorOccurred);
}

GatewayClient::~GatewayClient()
{
    // socket 随 this 销毁,断开即可
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
}

// 连接网关
void GatewayClient::connectToGateway(const QString &host, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        return;
    m_recvBuf.clear();
    m_socket->connectToHost(host, port);
}

// 主动断开
void GatewayClient::disconnectFromGateway()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

bool GatewayClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

// 转发串口帧:原始帧直接发(帧自带 AA..55 边界,网关侧按边界切分)
void GatewayClient::forwardDeviceData(const QByteArray &frame)
{
    if (frame.isEmpty())
        return;
    if (!isConnected()) {
        return;   // 未连接时静默丢弃(本地串口功能不受网关影响)
    }
    m_socket->write(frame);
}

// 连接成功
void GatewayClient::onConnected()
{
    emit gatewayConnected();
}

// 连接断开(主动或掉线)
void GatewayClient::onDisconnected()
{
    emit gatewayDisconnected();
}

// 网关数据到达:先入缓冲再按帧边界切分(防粘包/半包)
void GatewayClient::onReadyRead()
{
    m_recvBuf.append(m_socket->readAll());
    extractFrames();
}

// 套接字错误
void GatewayClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit gatewayError(m_socket->errorString());
}

// 帧边界扫描:找 AA ... 55 的完整帧,逐帧 emit
// 帧格式:AA cmdType deviceType dataLen data checksum 55(不定长,dataLen 指示)
void GatewayClient::extractFrames()
{
    while (true) {
        // 1. 找帧头 AA
        int head = m_recvBuf.indexOf(char(0xAA));
        if (head < 0) { m_recvBuf.clear(); return; }
        if (head > 0)
            m_recvBuf.remove(0, head);          // 丢弃帧头前的垃圾字节

        // 2. 最小帧长:AA+cmd+device+len+checksum+55 = 6 字节
        if (m_recvBuf.size() < 6)
            return;                              // 半包,等更多数据

        // 3. 按 dataLen 定位帧尾
        int dataLen = static_cast<quint8>(m_recvBuf.at(3));
        int frameLen = 6 + dataLen;              // 头4 + data + checksum + 尾1
        if (m_recvBuf.size() < frameLen)
            return;                              // 半包

        // 4. 校验帧尾
        if (static_cast<quint8>(m_recvBuf.at(frameLen - 1)) != 0x55) {
            m_recvBuf.remove(0, 1);              // 假帧头,跳过继续扫
            continue;
        }

        // 5. 完整帧上抛
        emit gatewayMessage(m_recvBuf.left(frameLen));
        m_recvBuf.remove(0, frameLen);
    }
}

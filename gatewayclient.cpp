#include "gatewayclient.h"
#include "log.h"
#include <QDebug>
#include <QNetworkProxy>

GatewayClient::GatewayClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_hbTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &GatewayClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &GatewayClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &GatewayClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &GatewayClient::onErrorOccurred);
    connect(m_hbTimer, &QTimer::timeout, this, &GatewayClient::sendHeartbeat);
    m_hbTimer->setInterval(3000);   // 3s 一次心跳，小于网关的超时阈值

    // 网关通常是局域网直连，强制不走系统代理（避免 Clash 等代理导致
    // ProxyTypeNotSupportedError："The proxy type is invalid for this operation"）
    m_socket->setProxy(QNetworkProxy::NoProxy);
}

GatewayClient::~GatewayClient()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
}

bool GatewayClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void GatewayClient::connectToGateway(const QString &host, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        return;
    m_recvBuf.clear();
    m_socket->connectToHost(host, port);
}

void GatewayClient::disconnectFromGateway()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

void GatewayClient::onConnected()
{
    sendRegister();          // 先把"串口桥"服务注册到网关
    m_hbTimer->start();      // 再开始周期心跳，避免被网关超时剔除
    emit gatewayConnected();
}

void GatewayClient::onDisconnected()
{
    m_hbTimer->stop();
    emit gatewayDisconnected();
}

void GatewayClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit gatewayError(m_socket->errorString());
}

// 注册串口桥服务：REGISTER 的 payload 是 4 字节大端 service_id
void GatewayClient::sendRegister()
{
    gw::Header h;
    h.serviceId = gw::SVC_SERIAL_BRIDGE;
    h.msgType   = gw::REGISTER;
    h.length    = 4;
    QByteArray payload(4, '\0');
    payload[0] = char((gw::SVC_SERIAL_BRIDGE >> 24) & 0xFF);
    payload[1] = char((gw::SVC_SERIAL_BRIDGE >> 16) & 0xFF);
    payload[2] = char((gw::SVC_SERIAL_BRIDGE >> 8) & 0xFF);
    payload[3] = char(gw::SVC_SERIAL_BRIDGE & 0xFF);
    m_socket->write(gw::encodeFrame(h, payload));
}

void GatewayClient::sendHeartbeat()
{
    if (!isConnected()) return;
    gw::Header h;
    h.serviceId = gw::SVC_SERIAL_BRIDGE;
    h.msgType   = gw::HEARTBEAT;
    m_socket->write(gw::encodeFrame(h));
}

void GatewayClient::respond(quint32 clientId, quint8 errorCode, const QByteArray &payload)
{
    if (!isConnected()) return;
    gw::Header h;
    h.serviceId = gw::SVC_SERIAL_BRIDGE;
    h.clientId  = clientId;
    h.msgType   = gw::RESPONSE;
    h.errorCode = errorCode;
    h.length    = quint32(payload.size());
    m_socket->write(gw::encodeFrame(h, payload));
}

void GatewayClient::onReadyRead()
{
    m_recvBuf.append(m_socket->readAll());
    drainFrames();
}

// 按"25 字节头 + length 字节载荷"切帧，处理半包/粘包/错位重同步
void GatewayClient::drainFrames()
{
    while (m_recvBuf.size() >= gw::kHeaderSize) {
        gw::Header h = gw::decodeHeader(m_recvBuf, 0);
        if (h.magic != gw::kMagic) {
            m_recvBuf.remove(0, 1);   // 协议错位，丢一个字节重新找帧头
            continue;
        }
        if (h.length > (1u << 20)) {  // 异常大包，直接丢弃
            Logger::instance()->warning(QString("网关帧长度异常: %1，清空缓冲").arg(h.length));
            m_recvBuf.clear();
            return;
        }
        const int total = gw::kHeaderSize + int(h.length);
        if (m_recvBuf.size() < total) break;   // 半包，等更多数据

        QByteArray payload = m_recvBuf.mid(gw::kHeaderSize, int(h.length));
        m_recvBuf.remove(0, total);

        if (h.msgType == gw::REQUEST) {
            Logger::instance()->info(QString("网关下发 REQUEST service=%1 method=%2 client=%3 %4B")
                                         .arg(h.serviceId).arg(h.methodId)
                                         .arg(h.clientId).arg(payload.size()));
            emit remoteFrame(h.clientId, h.methodId, payload);
        }
        // RESPONSE / NOTIFY / HEARTBEAT：串口桥暂不需要处理
    }
}

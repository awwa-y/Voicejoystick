#include <QCoreApplication>
#include <QDebug>
#include "protocolframe.h"

// 测试辅助类：接收信号并记录结果
class TestReceiver : public QObject {
    Q_OBJECT
public:
    bool success = false;
    quint8 recvCmd = 0;
    quint8 recvDev = 0;
    QByteArray recvData;
    QString errorMsg;

public slots:
    void onFrameReceived(quint8 cmd, quint8 dev, const QByteArray &data) {
        recvCmd = cmd;
        recvDev = dev;
        recvData = data;
        success = true;
    }
    void onError(const QString &msg) {
        errorMsg = msg;
        success = false;
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);  // 需要事件循环才能接收信号，但也可以不用，为了安全加上

    ProtocolFrame parser;
    TestReceiver receiver;

    // 连接信号到测试槽
    QObject::connect(&parser, &ProtocolFrame::frameReceived,
                     &receiver, &TestReceiver::onFrameReceived);
    QObject::connect(&parser, &ProtocolFrame::errorReported,
                     &receiver, &TestReceiver::onError);

    // ----- 测试用例1：正常数据 -----
    quint8 testCmd = 0x12;
    quint8 testDev = 0x34;
    QByteArray testData = "Hello STM32!";

    QByteArray frame = ProtocolFrame::pack(testCmd, testDev, testData);
    qDebug() << "Packed frame (hex):" << frame.toHex();

    // 逐字节喂给状态机
    for (char byte : frame) {
        parser.feed(static_cast<quint8>(byte));
    }

    // 检查结果
    if (receiver.success) {
        qDebug() << "Test PASS: cmd=" << receiver.recvCmd
                 << ", dev=" << receiver.recvDev
                 << ", data=" << receiver.recvData;
    } else {
        qDebug() << "Test FAIL: " << receiver.errorMsg;
        return 1;
    }

    // ----- 测试用例2：空数据 -----
    receiver.success = false;
    QByteArray frame2 = ProtocolFrame::pack(0x01, 0x02, QByteArray());
    for (char byte : frame2) {
        parser.feed(static_cast<quint8>(byte));
    }
    if (receiver.success && receiver.recvData.isEmpty()) {
        qDebug() << "Test2 PASS: empty data decoded correctly";
    } else {
        qDebug() << "Test2 FAIL";
        return 1;
    }

    // ----- 测试用例3：错误校验和（篡改一个字节）-----
    receiver.success = false;
    QByteArray frame3 = ProtocolFrame::pack(0x05, 0x06, "abc");
    // 把第一个字节（0xAA）改成0xAB，破坏帧头（会导致WaitForMagic忽略它，收不到完整帧）
    // 但更好的破坏是改数据长度或数据，让校验和不匹配。我们直接改数据段最后一个字节。
    // 注意：frame3[索引] 0=AA,1=cmd,2=dev,3=len(3),4='a',5='b',6='c',7=校验和,8=55
    // 把 'c' 改成 'd'
    frame3[6] = 'd';   // 原本是 'c'
    for (char byte : frame3) {
        parser.feed(static_cast<quint8>(byte));
    }
    if (!receiver.success && receiver.errorMsg.contains("Checksum")) {
        qDebug() << "Test3 PASS: checksum error detected";
    } else {
        qDebug() << "Test3 FAIL: expected checksum error, got success or other error";
        return 1;
    }

    qDebug() << "All tests passed!";
    return 0;
}

// 必须包含 moc 生成的代码（因为 TestReceiver 有 Q_OBJECT）

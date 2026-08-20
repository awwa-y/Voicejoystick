#ifndef PROTOCOLFRAME_H
#define PROTOCOLFRAME_H
#include <QObject>
#include <QByteArray>

class ProtocolFrame : public QObject{
    Q_OBJECT
public:
    ProtocolFrame();
    static QByteArray pack(uint8_t cmdType,uint8_t deviceType,const QByteArray &data);
signals:
    void frameReceived(quint8 cmdType, quint8 deviceType, const QByteArray &data);
    void errorReported(const QString&);
private:
    static constexpr quint8 MAGIC = 0xAA;
    static constexpr quint8 TAIL = 0x55;
    static uint8_t calculateChecksum(const QByteArray &frameWithoutTail);

    enum class ParseState {
        WaitForMagic,
        WaitForCmdType,
        WaitForDeviceType,
        WaitForDataLen,
        WaitForData,
        WaitForChecksum,
        WaitForFrameTail
    };

    ParseState currentState;
    quint8 tempCmd;




};



#endif // PROTOCOLFRAME_H

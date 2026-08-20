#include "protocolframe.h"


ProtocolFrame::ProtocolFrame()
{
}

QByteArray ProtocolFrame::pack(uint8_t cmdType, uint8_t deviceType, const QByteArray &data)
{
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

uint8_t ProtocolFrame::calculateChecksum(const QByteArray &frameWithoutTail)
{
    quint8 sum = 0;
    for (int i = 1; i < frameWithoutTail.size(); ++i) {
        sum += static_cast<quint8>(frameWithoutTail.at(i));
    }
    return sum;
}

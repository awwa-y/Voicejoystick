#include "commandprocessor.h"

CommandProcessor::CommandProcessor(QWidget *parent) {

}
QByteArray CommandProcessor::createControlFrame(int x, int y,int speed)
{
    uint8_t x_byte = static_cast<uint8_t>(x + 100);
    uint8_t y_byte = static_cast<uint8_t>(y + 100);
    uint8_t speed_byte = static_cast<uint8_t>(speed);

    QByteArray data;
    data.append(static_cast<char>(x_byte));
    data.append(static_cast<char>(y_byte));
    data.append(static_cast<char>(speed_byte));

    QByteArray frame;
    frame.append(0xAA);
    frame.append(CMD_CONTROL);
    frame.append(static_cast<char>(data.size())); // 3
    frame.append(data);

    // static constexpr uint8_t CMD_CONTROL = 0x01;
    // static constexpr uint8_t CMD_EMERGENCY_STOP = 0x03;
    // 计算校验和之前打印 frame 内容（不含校验和 帧尾）
    qDebug() << "Before checksum:" << frame.toHex(' ').toUpper();

    uint8_t checksum = calculateChecksum(frame);
    qDebug() << "Calculated checksum:" << QString::number(checksum, 16).toUpper();

    frame.append(static_cast<char>(checksum));
    frame.append(0x55);

    qDebug() << "Final frame:" << frame.toHex(' ').toUpper();
    return frame;
}
uint8_t CommandProcessor::calculateChecksum(const QByteArray &data)
{
    uint8_t sum = 0;

    for (int i = 1; i < data.size(); ++i) {
        sum += static_cast<uint8_t>(data[i]);
    }
    return sum;
}

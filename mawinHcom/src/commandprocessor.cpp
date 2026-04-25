#include "commandprocessor.h"
#include <QDebug>

CommandProcessor::CommandProcessor(QWidget *parent) : QWidget(parent)
{
}

QByteArray CommandProcessor::createCommand(uint8_t cmdType, uint8_t deviceType, const QByteArray &data)
{
    QByteArray frame;
    frame.append(0xAA);                    // 帧头
    frame.append(static_cast<char>(cmdType));        // 命令类型
    frame.append(static_cast<char>(deviceType));     // 设备类型
    frame.append(static_cast<char>(data.size()));    // 数据长度
    frame.append(data);                    // 数据

    uint8_t checksum = calculateChecksum(frame);
    frame.append(static_cast<char>(checksum));       // 校验和
    frame.append(0x55);                    // 帧尾

    qDebug() << "Command frame:" << frame.toHex(' ').toUpper();
    return frame;
}

QByteArray CommandProcessor::createControlFrame(int x, int y, int speed)
{
    uint8_t x_byte = static_cast<uint8_t>(x + 100);
    uint8_t y_byte = static_cast<uint8_t>(y + 100);
    uint8_t speed_byte = static_cast<uint8_t>(speed);

    QByteArray data;
    data.append(static_cast<char>(x_byte));
    data.append(static_cast<char>(y_byte));
    data.append(static_cast<char>(speed_byte));

    return createCommand(CMD_CONTROL, DEVICE_MOTOR, data);
}

QByteArray CommandProcessor::createMotorSpeedCommand(int speed)
{
    uint8_t speed_byte = static_cast<uint8_t>(speed);
    QByteArray data;
    data.append(static_cast<char>(speed_byte));
    return createCommand(CMD_MOTOR_SPEED, DEVICE_MOTOR, data);
}

QByteArray CommandProcessor::createSensorReadCommand()
{
    QByteArray data;
    return createCommand(CMD_SENSOR_READ, DEVICE_SENSOR, data);
}

QByteArray CommandProcessor::createServoAngleCommand(int angle)
{
    uint8_t angle_byte = static_cast<uint8_t>(angle);
    QByteArray data;
    data.append(static_cast<char>(angle_byte));
    return createCommand(CMD_SERVO_ANGLE, DEVICE_SERVO, data);
}

QByteArray CommandProcessor::createEmergencyStopCommand()
{
    QByteArray data;
    return createCommand(CMD_EMERGENCY_STOP, DEVICE_MOTOR, data);
}

uint8_t CommandProcessor::calculateChecksum(const QByteArray &data)
{
    uint8_t sum = 0;
    for (int i = 1; i < data.size(); ++i) {
        sum += static_cast<uint8_t>(data[i]);
    }
    return sum;
}
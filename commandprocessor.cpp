#include "commandprocessor.h"
#include "protocolframe.h"

// 历史遗留的构造，static 工具类不需要实例化
CommandProcessor::CommandProcessor(QWidget *parent)
{
}

// 通用组包：组帧逻辑收敛到 ProtocolFrame::pack，这里只做转发
// 全项目只有 ProtocolFrame 一处知道帧长什么样，改协议不用到处找
QByteArray CommandProcessor::createCommand(uint8_t cmdType, uint8_t deviceType, const QByteArray &data)
{
    return ProtocolFrame::pack(cmdType, deviceType, data);
}

// 摇杆方向控制帧
// x/y 范围 -100..100（有符号），协议字节无符号，
// 统一 +100 平移到 0..200 再编码，STM32 端减 100 还原
QByteArray CommandProcessor::createControlFrame(int x, int y, int speed)
{
    QByteArray data;
    data.append(static_cast<char>(x + 100));
    data.append(static_cast<char>(y + 100));
    data.append(static_cast<char>(speed));

    return createCommand(CMD_CONTROL, DEVICE_MOTOR, data);
}

// 电机调速帧
QByteArray CommandProcessor::createMotorSpeedCommand(int speed)
{
    QByteArray data;
    data.append(static_cast<char>(speed));
    return createCommand(CMD_MOTOR_SPEED, DEVICE_MOTOR, data);
}

// 舵机角度帧
QByteArray CommandProcessor::createServoAngleCommand(int angle)
{
    QByteArray data;
    data.append(static_cast<char>(angle));
    return createCommand(CMD_SERVO_ANGLE, DEVICE_SERVO, data);
}

// 传感器读取帧，数据段为空
QByteArray CommandProcessor::createSensorReadCommand()
{
    return createCommand(CMD_SENSOR_READ, DEVICE_SENSOR, QByteArray());
}

// 紧急停止帧，数据段为空
QByteArray CommandProcessor::createEmergencyStopCommand()
{
    return createCommand(CMD_EMERGENCY_STOP, DEVICE_MOTOR, QByteArray());
}

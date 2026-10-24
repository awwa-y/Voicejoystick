#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include <QByteArray>
#include <cstdint>
#include <QWidget>

// 指令构造器：把业务语义翻译成 0xAA 七段式协议帧
// 每个业务指令（控制/调速/转角/读传感器/急停）各有一个便捷函数，
// 上层不用记命令码。全部 static，无状态工具类
class CommandProcessor
{
public:

     CommandProcessor(QWidget *parent = nullptr);
    ~CommandProcessor();

     // 摇杆方向控制帧：x/y 偏移(-100..100) + 速度，数据段 3 字节
     static QByteArray createControlFrame(int x, int y, int speed);
     // 电机调速帧，速度值 1 字节
     static QByteArray createMotorSpeedCommand(int speed);
     // 舵机角度帧，角度(0..180) 1 字节
     static QByteArray createServoAngleCommand(int angle);
     // 传感器读取帧，空数据段
     static QByteArray createSensorReadCommand();
     // 紧急停止帧，空数据段，设备侧最高优先级执行
     static QByteArray createEmergencyStopCommand();

     static constexpr uint8_t CMD_CONTROL = 0x01;        // 控制指令
     static constexpr uint8_t CMD_MOTOR_SPEED = 0x02;    // 电机速度指令
     static constexpr uint8_t CMD_EMERGENCY_STOP = 0x06; // 紧急停止指令
     static constexpr uint8_t CMD_SERVO_ANGLE = 0x04;    // 舵机角度指令
     static constexpr uint8_t CMD_SENSOR_READ = 0x05;    // 传感器读取指令
     // 设备类型
     static constexpr uint8_t DEVICE_MOTOR = 0x01;       // 电机
     static constexpr uint8_t DEVICE_SERVO = 0x02;       // 舵机
     static constexpr uint8_t DEVICE_SENSOR = 0x03;      // 传感器

     // 通用组包入口：内部转调 ProtocolFrame::pack，组帧逻辑只在 ProtocolFrame 一处
     static QByteArray createCommand(uint8_t cmdType, uint8_t deviceType, const QByteArray &data);
};

#endif

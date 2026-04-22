#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include <QByteArray>
#include <cstdint>
#include <QWidget>
class CommandProcessor:public QWidget
{
     Q_OBJECT
public:

     CommandProcessor(QWidget *parent = nullptr);

     static QByteArray createControlFrame(int x, int y, int speed);
     static QByteArray createMotorSpeedCommand(int speed);
     static QByteArray createServoAngleCommand(int angle);
     static QByteArray createSensorReadCommand();

     static constexpr uint8_t CMD_CONTROL = 0x01;
     static constexpr uint8_t CMD_MOTOR_SPEED = 0x02;
     static constexpr uint8_t CMD_EMERGENCY_STOP = 0x03;
     static constexpr uint8_t CMD_SERVO_ANGLE = 0x04;
     static constexpr uint8_t CMD_SENSOR_READ = 0x05;
     // 设备类型
     static constexpr uint8_t DEVICE_MOTOR = 0x01;
     static constexpr uint8_t DEVICE_SERVO = 0x02;
     static constexpr uint8_t DEVICE_SENSOR = 0x03;

     static QByteArray createCommand(uint8_t cmdType, uint8_t deviceType, const QByteArray &data);
     static QByteArray createEmergencyStopCommand();
 private:
     static uint8_t calculateChecksum(const QByteArray &data);
};

#endif // COMMANDPROCESSOR_H

#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include <QByteArray>
#include <cstdint>
#include <QWidget>
class CommandProcessor:public QWidget
{
     Q_OBJECT
public:
     CommandProcessor(QWidget *parent=nullptr);
    static QByteArray createControlFrame(int x, int y,int speed);

    static constexpr uint8_t CMD_CONTROL = 0x01;
    static constexpr uint8_t CMD_EMERGENCY_STOP = 0x03;
private:
    static uint8_t calculateChecksum(const QByteArray &data);

};

#endif // COMMANDPROCESSOR_H

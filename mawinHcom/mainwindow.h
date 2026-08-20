#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "devicemanager.h"
#include "joystickmanager.h"
#include <QTimer>
#include <QSerialPortInfo>
#include <QDebug>
#include <QStringList>
#include <QThread>
#include "VoskWorker.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

#include <QMessageBox>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QStringList getAvailablePorts();
    void Qslwork_init();
    void speedchange();

    int getbaud();
public:
    bool m_hasPending;
    void updateDeviceTabStatus(DeviceType type, bool added);
    DeviceType getSelectedDeviceType();
    QString getDeviceNameByType(DeviceType type);
    void onMotorAngleSlider_valueChanged(int value);
    void onReadSensorClicked();

public slots:
    void onLogMessageReceived(const QString &message);
private slots:
    void onMotorConnectClicked();
    void onServoConnectClicked();
    void onSensorConnectClicked();

    void onRefreshPortsClicked();
    void onForwardButtonClicked();
    void onBackwardButtonClicked();
    void onSpeedUpButtonClicked();
    void onSpeedDownButtonClicked();
    void onDataReceived(int deviceId, const QByteArray &data);
    void onSerialError(int deviceId, const QString &error);
    void onSerialOpened(int deviceId);
    void onSerialClosed();
    void changenewdata(int newx,int newy);
    void sendPendingData();
    void speedUp();
    void speedDown();

    void onAddDeviceClicked();
    void onRemoveDeviceClicked();
    void updateDeviceListCombox();

    void onDeviceComboBoxChanged(int index);
    void onDeviceStatusChanged(int deviceId, const QString &status);
    void onDeviceDataReceived(int deviceId, const QByteArray &data);
    void onSendCommandClicked();

    // void onReadSensorClicked();
    // void onControlMotorClicked();

    void onVoiceCommand(const QString &cmd);   // 识别到指令
    void onVoiceStatus(const QString &msg);    // 语音引擎状态（启动/停止）
    void onVoiceError(const QString &err);     // 错误信息
    void toggleVoiceRecognition(bool enabled); // 按钮切换语音开关
    // void initVoiceUI();
private:
    Ui::MainWindow *ui;
    JoystickManager *m_joystick;
    QTimer *m_sendTimer;
    bool m_isConnected;
    QString portName;
    int baud;
    int m_pendingX, m_pendingY;
    bool m_pendingA, m_pendingB;
    int m_speedValue;
    int m_lastX;
    int m_lastY;

    bool m_motorConnected;
    bool m_servoConnected;
    bool m_sensorConnected;

    // 语音工作线程
    QThread      m_voskThread;
    VoskWorker   *m_voskWorker = nullptr;

    ///设备变量
    DeviceManager *deviceManager = nullptr;
    // 设备类型到设备ID的映射
    QMap<DeviceType, int> devicetypeIdmap;
signals:
    void openSerialRequest(QString portName, int baudRate, int deviceId);
    void closeSerialRequest(int deviceId);
    void sendDataRequest(int deviceId, const QByteArray &data);

    void startVoiceRecognition();
    void stopVoiceRecognition();
};

#endif
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "joystickmanager.h"
#include <QTimer>
#include <QSerialPortInfo>
#include <QDebug>
#include <QStringList>
#include "serialwork.h"
#include <QThread>
#include "commandprocessor.h"
#include "joystickmanager.h"
#include "VoskWorker.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include "VoskWorker.h"
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
private slots:
    void onConnectButtonClicked();
    void onRefreshPortsClicked();
    void onForwardButtonClicked();
    void onBackwardButtonClicked();
    void onSpeedUpButtonClicked();
    void onSpeedDownButtonClicked();
    void onDataReceived(const QByteArray &data);
    void onSerialError(const QString &error);
    void onSerialOpened();
    void onSerialClosed();
    void changenewdata(int newx,int newy);
    void sendPendingData();
    void speedUp();
    void speedDown();

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
    QThread m_serialThread;
    SerialWork *m_serialWorker;
    int m_pendingX, m_pendingY;
    bool m_pendingA, m_pendingB;
    int m_speedValue;
    int m_lastX;
    int m_lastY;

    QPushButton *m_voiceToggleButton = nullptr;
    QLabel      *m_voiceStatusLabel = nullptr;
    QTextEdit   *m_voiceResultEdit = nullptr;

    // 语音工作线程
    QThread      m_voskThread;
    VoskWorker   *m_voskWorker = nullptr;
signals:
    void openSerialRequest(QString portName,int baud);
    void closeSerialRequest();
    void sendDataRequest(const QByteArray &data);

    void startVoiceRecognition();
    void stopVoiceRecognition();
};

#endif // MAINWINDOW_H

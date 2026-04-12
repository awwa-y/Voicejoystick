#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "VoskWorker.h"
#include <QDebug>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serialWorker(new SerialWork)
    , m_isConnected(false)
    , m_speedValue(0)
{
    ui->setupUi(this);
    m_joystick = ui->widget;
    m_sendTimer = new QTimer(this);
    Qslwork_init();
    speedchange();
    connect(ui->connectbutton, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);

    // 串口线程
    m_serialWorker->moveToThread(&m_serialThread);
    connect(&m_serialThread, &QThread::finished, m_serialWorker, &QObject::deleteLater);
    connect(this, &MainWindow::openSerialRequest, m_serialWorker, &SerialWork::openSerial);
    connect(this, &MainWindow::closeSerialRequest, m_serialWorker, &SerialWork::closeSerial);
    connect(this, &MainWindow::sendDataRequest, m_serialWorker, &SerialWork::sendData);
    connect(ui->freshbutton, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    connect(m_serialWorker, &SerialWork::dataReceived, this, &MainWindow::onDataReceived);
    connect(m_serialWorker, &SerialWork::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_serialWorker, &SerialWork::serialOpened, this, &MainWindow::onSerialOpened);
    connect(m_serialWorker, &SerialWork::serialClosed, this, &MainWindow::onSerialClosed);
    connect(m_joystick, &JoystickManager::joystickChanged, this, &MainWindow::changenewdata);
    connect(m_sendTimer, &QTimer::timeout, this, &MainWindow::sendPendingData);
    connect(this, &MainWindow::sendDataRequest, m_serialWorker, &SerialWork::sendData);

    // 速度控制
    connect(ui->fastbutton, &QPushButton::clicked, this, &MainWindow::speedUp);
    connect(ui->lowbutton, &QPushButton::clicked, this, &MainWindow::speedDown);

    m_sendTimer->start(40);
    m_serialThread.start();

    //语音控制
    m_voskWorker = new VoskWorker();
    m_voskWorker->moveToThread(&m_voskThread);

    connect(&m_voskThread, &QThread::finished, m_voskWorker, &QObject::deleteLater);
    connect(this, &MainWindow::startVoiceRecognition, m_voskWorker, &VoskWorker::startRecognition);
    connect(this, &MainWindow::stopVoiceRecognition, m_voskWorker, &VoskWorker::stopRecognition);
    connect(m_voskWorker, &VoskWorker::commandRecognized, this, &MainWindow::onVoiceCommand);
    connect(m_voskWorker, &VoskWorker::statusMessage, this, &MainWindow::onVoiceStatus);
    connect(m_voskWorker, &VoskWorker::errorOccurred, this, &MainWindow::onVoiceError);
    connect(m_voskWorker, &VoskWorker::joystickMove, m_joystick, &JoystickManager::setPosition);

    m_voskThread.start();

    ui->voiceToggleButton->setCheckable(true);
    connect(ui->voiceToggleButton, &QPushButton::toggled, this, &MainWindow::toggleVoiceRecognition);

}

MainWindow::~MainWindow()
{
    emit stopVoiceRecognition();
    m_voskThread.quit();
    m_voskThread.wait();

    m_serialThread.quit();
    m_serialThread.wait();
    delete ui;
}


QStringList MainWindow::getAvailablePorts()
{
    QStringList ports;
    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : serialPortInfos) {
        ports << portInfo.portName();
    }
    ui->comCombox->addItems(ports);
    baud = ui->bateCombox->currentText().toInt();
    qDebug() << baud;
    return ports;
}

int MainWindow::getbaud()
{
    baud = ui->bateCombox->currentText().toInt();
    return baud;
}

void MainWindow::Qslwork_init()
{
    QStringList currentcomlist = getAvailablePorts();
    if (!currentcomlist.isEmpty()) {
        portName = currentcomlist[0];
        qDebug() << "默认串口:" << portName;
    } else {
        portName = "";
        qDebug() << "未检测到串口设备";
        ui->connectbutton->setEnabled(false);
    }
    QStringList baudRates = {"4800","9600", "19200", "38400", "57600", "115200", "230400"};
    ui->bateCombox->addItems(baudRates);
}

void MainWindow::onConnectButtonClicked()
{
    if (m_isConnected) {
        emit closeSerialRequest();
    } else {
        QString port = ui->comCombox->currentText();
        if (port.isEmpty() || port == "无可用串口") {
            qDebug() << "请选择有效串口";
            return;
        }
        bool ok;
        int baud = ui->bateCombox->currentText().toInt(&ok);
        if (!ok) {
            qDebug() << "无效波特率";
            return;
        }
        emit openSerialRequest(port, baud);
    }
    ui->connectbutton->setText("连接中...");
    ui->connectbutton->setEnabled(false);
}

void MainWindow::onRefreshPortsClicked()
{
    qDebug() << "刷新";
    QStringList currentcomlist = getAvailablePorts();
    if (!currentcomlist.isEmpty()) {
        portName = currentcomlist[0];
        qDebug() << "刷新为" << currentcomlist[0];
        ui->connectbutton->setEnabled(true);
    } else {
        portName = "";
        qDebug() << "未检测到串口设备";
        qDebug() << "刷新不出来";
        ui->connectbutton->setEnabled(false);
    }
}

void MainWindow::onDataReceived(const QByteArray &data)
{
    // 根据需要实现
    Q_UNUSED(data);
}

void MainWindow::onSerialError(const QString &error)
{
    qDebug() << "串口错误:" << error;
}

void MainWindow::onSerialOpened()
{
    ui->connectbutton->setText("断开");
    ui->comCombox->setEnabled(false);
    ui->bateCombox->setEnabled(false);
    ui->connectbutton->setEnabled(true);
    m_isConnected = true;
}

void MainWindow::onSerialClosed()
{
    ui->connectbutton->setText("连接");
    ui->comCombox->setEnabled(true);
    ui->bateCombox->setEnabled(true);
    ui->connectbutton->setEnabled(true);
    m_isConnected = false;
}

void MainWindow::changenewdata(int newx, int newy)
{
    m_pendingX = newx;
    m_pendingY = newy;
    m_hasPending = true;
}

void MainWindow::sendPendingData()
{
    if (!m_hasPending) return;

    qDebug() << "Pending X:" << m_pendingX << " Y:" << m_pendingY << " Speed:" << m_speedValue;

    QByteArray frame = CommandProcessor::createControlFrame(m_pendingX, m_pendingY, m_speedValue);
    qDebug() << "Frame:" << frame.toHex(' ').toUpper();

    emit sendDataRequest(frame);
    m_hasPending = false;
}

void MainWindow::speedUp()
{
    m_speedValue += 10;
    speedchange();
}

void MainWindow::speedDown()
{
    if (m_speedValue >= 10) {
        m_speedValue -= 10;
        speedchange();
    }
}

void MainWindow::speedchange()
{
    QString text = QString::number(m_speedValue);
    ui->speedValueLB->setText(" " + text);
    sendPendingData();   // 立即发送
}
void MainWindow::onForwardButtonClicked()
{
    qDebug() << "Forward button clicked";
}

void MainWindow::onBackwardButtonClicked()
{
    qDebug() << "Backward button clicked";
}

void MainWindow::onSpeedUpButtonClicked()
{
    speedUp();
}

void MainWindow::onSpeedDownButtonClicked()
{
    speedDown();
}
void MainWindow::toggleVoiceRecognition(bool checked)
{
    if (checked) {
        emit startVoiceRecognition();
        ui->voiceToggleButton->setText("关闭语音");
        ui->voiceStatusLabel->setText("语音状态：正在监听...");
    } else {
        emit stopVoiceRecognition();
        ui->voiceToggleButton->setText("开启语音");
        ui->voiceStatusLabel->setText("语音状态：已停止");
    }
}

void MainWindow::onVoiceCommand(const QString &cmd)
{
    ui->voiceResultEdit->append(QString("识别到指令：%1").arg(cmd));

    if (cmd == "加速") {
        speedUp();
        ui->voiceResultEdit->append("-> 执行加速");
    } else if (cmd == "减速") {
        speedDown();
        ui->voiceResultEdit->append("-> 执行减速");
    } else if (cmd == "左转") {
        ui->voiceResultEdit->append("-> 执行左转（摇杆已移动）");
    } else if (cmd == "右转") {
        ui->voiceResultEdit->append("-> 执行右转（摇杆已移动）");
    }
}

void MainWindow::onVoiceStatus(const QString &msg)
{
    ui->voiceStatusLabel->setText(QString("语音状态：%1").arg(msg));
}

void MainWindow::onVoiceError(const QString &err)
{
    ui->voiceResultEdit->append(QString("错误：%1").arg(err));
    ui->voiceStatusLabel->setText("语音状态：出错");
    ui->voiceToggleButton->setChecked(false);
}
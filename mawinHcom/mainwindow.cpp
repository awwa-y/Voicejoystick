#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "VoskWorker.h"
#include <QDebug>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include "commandprocessor.h"
#include "log.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isConnected(false)
    , m_speedValue(0)
{
    ui->setupUi(this);
    m_joystick = ui->widget;
    m_sendTimer = new QTimer(this);

    Qslwork_init();
    speedchange();

    ui->tabWidget->setTabText(0, "电机控制");
    ui->tabWidget->setTabText(1, "舵机控制");
    ui->tabWidget->setTabText(2, "温湿度传感器");
    m_motorConnected = false;
    m_servoConnected = false;
    m_sensorConnected = false;
    connect(ui->motorConnectButton, &QPushButton::clicked, this, &MainWindow::onMotorConnectClicked);
    connect(ui->servoConnectButton, &QPushButton::clicked, this, &MainWindow::onServoConnectClicked);
    connect(ui->sensorConnectButton, &QPushButton::clicked, this, &MainWindow::onSensorConnectClicked);
    connect(ui->removedeviceButton, &QPushButton::clicked, this, &MainWindow::onRemoveDeviceClicked);

    //设备连接
    deviceManager =new DeviceManager(this);
    connect(ui->adddevice,&QPushButton::clicked,this,&MainWindow::onAddDeviceClicked);

    // manager管理串口线程

    connect(this, &MainWindow::sendDataRequest, deviceManager, &DeviceManager::sendData);
    connect(this, &MainWindow::openSerialRequest, deviceManager, &DeviceManager::openSerialRequest);
    connect(this, &MainWindow::closeSerialRequest, deviceManager, &DeviceManager::closeSerialRequest);
    connect(ui->freshbutton, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    connect(deviceManager, &DeviceManager::deviceDataReceived, this, &MainWindow::onDataReceived);
    connect(deviceManager, &DeviceManager::deviceError, this, &MainWindow::onSerialError);
    connect(deviceManager, &DeviceManager::deviceConnected, this, [=](int deviceId){
        onSerialOpened(deviceId);
    });
    connect(deviceManager, &DeviceManager::deviceDisconnected, this, [=](int deviceId){
        // 设备断开时的处理
        DeviceType type = deviceManager->getDeviceType(deviceId);
        switch (type) {
        case DEVICE_TYPE_MOTOR:
            m_motorConnected = false;
            if (ui->motorConnectButton) ui->motorConnectButton->setText("连接");
            break;
        case DEVICE_TYPE_SERVO:
            m_servoConnected = false;
            if (ui->servoConnectButton) ui->servoConnectButton->setText("连接");
            break;
        case DEVICE_TYPE_HUMIDITY_SENSOR:
            m_sensorConnected = false;
            if (ui->sensorConnectButton) ui->sensorConnectButton->setText("连接");
            break;
        default:
            break;
        }
        updateDeviceTabStatus(type, true);
        updateDeviceListCombox();
    });
    connect(m_joystick, &JoystickManager::joystickChanged,
            this, &MainWindow::changenewdata);
    connect(m_sendTimer, &QTimer::timeout,
            this,&MainWindow::sendPendingData);
    if (deviceManager) {
        connect(deviceManager, &DeviceManager::deviceStatusChanged,
                this, &MainWindow::onDeviceStatusChanged);
    }

    // 速度控制
    connect(ui->fastbutton, &QPushButton::clicked, this, &MainWindow::speedUp);
    connect(ui->lowbutton, &QPushButton::clicked, this, &MainWindow::speedDown);

    m_sendTimer->start(40);
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


    //命令控制
    connect(ui->servoAngleSlider, &QSlider::valueChanged, this, &MainWindow::onMotorAngleSlider_valueChanged);
    connect(ui->readSensorButton, &QPushButton::clicked, this, &MainWindow::onReadSensorClicked);

    //日志
    // 初始化Logger并连接信号
    Logger::instance()->init("mawinHcom.log");
    connect(Logger::instance(), &Logger::logMessage, this, &MainWindow::onLogMessageReceived);
}

MainWindow::~MainWindow()
{
    emit stopVoiceRecognition();
    m_voskThread.quit();
    m_voskThread.wait();
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
    ui->comCombox->clear();
    ui->comCombox->addItem("COM1");
    ui->comCombox->addItem("COM2");
    ui->comCombox->addItem("COM3");
    ui->comCombox->addItem("COM4");
    ui->comCombox->addItem("COM5");
    ui->comCombox->addItem("COM6");
    ui->comCombox->addItem("COM7");
    ui->comCombox->addItem("COM8");
    ui->comCombox->setEditable(true);  // 允许手动输入

    QStringList baudRates = {"4800", "9600", "19200", "38400", "57600", "115200", "230400"};
    QStringList devicename = {"舵机", "电机", "传感器"};
    QStringList currentcomlist = getAvailablePorts();
    if (!currentcomlist.isEmpty()) {
        portName = currentcomlist[0];
        qDebug() << "默认串口:" << portName;
    } else {
        portName = "";
        qDebug() << "未检测到串口设备";
    }
    ui->bateCombox->addItems(baudRates);
    ui->devicecombox->addItems(devicename);

}
//电机
void MainWindow::onMotorConnectClicked()
{
    if (m_motorConnected) {
        // 断开电机
        if (devicetypeIdmap.contains(DEVICE_TYPE_MOTOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_MOTOR];
            emit closeSerialRequest(deviceId);
        }
        m_motorConnected = false;
        ui->motorConnectButton->setText("连接");
        updateDeviceTabStatus(DEVICE_TYPE_MOTOR, true);
    } else {
        // 连接电机
        if (!devicetypeIdmap.contains(DEVICE_TYPE_MOTOR)) {
            QMessageBox::warning(this, "警告", "请先添加电机设备");
            return;
        }
        int deviceId = devicetypeIdmap[DEVICE_TYPE_MOTOR];
        emit openSerialRequest(
            deviceManager->getDevicePortName(deviceId),
            deviceManager->getDeviceBaudRate(deviceId),
            deviceId);
    }
}

// 舵机连接按钮
void MainWindow::onServoConnectClicked()
{
    if (m_servoConnected) {
        // 断开舵机
        if (devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_SERVO];
            emit closeSerialRequest(deviceId);
        }
        m_servoConnected = false;
        ui->servoConnectButton->setText("连接");
        updateDeviceTabStatus(DEVICE_TYPE_SERVO, true);
    } else {
        // 连接舵机
        if (!devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
            QMessageBox::warning(this, "警告", "请先添加舵机设备");
            return;
        }
        int deviceId = devicetypeIdmap[DEVICE_TYPE_SERVO];
        emit openSerialRequest(
            deviceManager->getDevicePortName(deviceId),
            deviceManager->getDeviceBaudRate(deviceId),
            deviceId);
    }
}

// 传感器连接按钮
void MainWindow::onSensorConnectClicked()
{
    if (m_sensorConnected) {
        // 断开传感器
        if (devicetypeIdmap.contains(DEVICE_TYPE_HUMIDITY_SENSOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_HUMIDITY_SENSOR];
            emit closeSerialRequest(deviceId);
        }
        m_sensorConnected = false;
        ui->sensorConnectButton->setText("连接");
        updateDeviceTabStatus(DEVICE_TYPE_HUMIDITY_SENSOR, true);
    } else {
        // 连接传感器
        if (!devicetypeIdmap.contains(DEVICE_TYPE_HUMIDITY_SENSOR)) {
            QMessageBox::warning(this, "警告", "请先添加传感器设备");
            return;
        }
        int deviceId = devicetypeIdmap[DEVICE_TYPE_HUMIDITY_SENSOR];
        emit openSerialRequest(
            deviceManager->getDevicePortName(deviceId),
            deviceManager->getDeviceBaudRate(deviceId),
            deviceId);
    }
}

void MainWindow::onRefreshPortsClicked()
{
    qDebug() << "刷新";
    QStringList currentcomlist = getAvailablePorts();
    if (!currentcomlist.isEmpty()) {
        portName = currentcomlist[0];
        qDebug() << "刷新为" << currentcomlist[0];
    } else {
        portName = "";
        qDebug() << "未检测到串口设备";
        qDebug() << "刷新不出来";
    }
}

void MainWindow::onDataReceived(int deviceId, const QByteArray &data)
{
    // 根据需要实现
    Q_UNUSED(data)
}

void MainWindow::onSerialError(int deviceId, const QString &error)
{
    qDebug() << "串口错误:" << error;

    // 获取设备信息
    QString port = deviceManager->getDevicePortName(deviceId);
    DeviceType type = deviceManager->getDeviceType(deviceId);
    QString deviceName = getDeviceNameByType(type);

    // 弹出错误对话框
    QMessageBox::critical(this, "串口连接失败",
                          QString("设备: %1\n串口: %2\n错误信息: %3").arg(deviceName).arg(port).arg(error));

    // 重置连接状态
    switch (type) {
    case DEVICE_TYPE_MOTOR:
        m_motorConnected = false;
        if (ui->motorConnectButton) ui->motorConnectButton->setText("连接");
        break;
    case DEVICE_TYPE_SERVO:
        m_servoConnected = false;
        if (ui->servoConnectButton) ui->servoConnectButton->setText("连接");
        break;
    case DEVICE_TYPE_HUMIDITY_SENSOR:
        m_sensorConnected = false;
        if (ui->sensorConnectButton) ui->sensorConnectButton->setText("连接");
        break;
    default:
        break;
    }

    // 更新状态标签
    updateDeviceTabStatus(type, true);
}

void MainWindow::onSerialOpened(int deviceId)
{
    // if (deviceManager) {
    //     deviceManager->setDeviceConnected(deviceId, true);
    // }

    qDebug() << "设备" << deviceId << "连接成功";

    DeviceType type = deviceManager->getDeviceType(deviceId);

    switch (type) {
    case DEVICE_TYPE_MOTOR:
        m_motorConnected = true;
        if (ui->motorConnectButton) ui->motorConnectButton->setText("断开");
        break;
    case DEVICE_TYPE_SERVO:
        m_servoConnected = true;
        if (ui->servoConnectButton) ui->servoConnectButton->setText("断开");
        break;
    case DEVICE_TYPE_HUMIDITY_SENSOR:
        m_sensorConnected = true;
        if (ui->sensorConnectButton) ui->sensorConnectButton->setText("断开");
        break;
    default:
        break;
    }

    updateDeviceTabStatus(type, true);
}

void MainWindow::onSerialClosed()
{

}

void MainWindow::changenewdata(int newx, int newy)
{
    m_pendingX = newx;
    m_pendingY = newy;
    m_hasPending = true;
    if (devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
        Logger::instance()->info(QString("摇杆数据更新: X=%1, Y=%2").arg(newx).arg(newy));
    }
}

void MainWindow::sendPendingData()
{
    if (!deviceManager) return;

    int currentDevice = -1;

    if (devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
        int servoId = devicetypeIdmap[DEVICE_TYPE_SERVO];
        if (deviceManager->isDeviceConnected(servoId)) {
            currentDevice = servoId;
        }
    }

    if (currentDevice == -1) {
        return;
    }

    if (!m_hasPending) return;

    qDebug() << "Pending X:" << m_pendingX << " Y:" << m_pendingY << " Speed:" << m_speedValue;

    int servoAngle = 90 + m_pendingX;
    if (servoAngle < 0) servoAngle = 0;
    if (servoAngle > 180) servoAngle = 180;

    QByteArray frame = CommandProcessor::createServoAngleCommand(servoAngle);
    qDebug() << "舵机角度:" << servoAngle << ", 帧内容:" << frame.toHex(' ').toUpper();

    emit sendDataRequest(currentDevice, frame);
    Logger::instance()->info(QString("发送舵机角度: %1°").arg(servoAngle));
    m_hasPending = false;
}

void MainWindow::speedUp()
{
    if (m_speedValue < 100) {
        m_speedValue += 10;
        speedchange();

        if (devicetypeIdmap.contains(DEVICE_TYPE_MOTOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_MOTOR];
            QByteArray command = CommandProcessor::createMotorSpeedCommand(m_speedValue);
            emit sendDataRequest(deviceId, command);
            Logger::instance()->info(QString("发送电机速度命令: %1").arg(m_speedValue));
        }
    }
}

void MainWindow::speedDown()
{
    if (m_speedValue >= 10) {
        m_speedValue -= 10;
        speedchange();

        if (devicetypeIdmap.contains(DEVICE_TYPE_MOTOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_MOTOR];
            QByteArray command = CommandProcessor::createMotorSpeedCommand(m_speedValue);
            emit sendDataRequest(deviceId, command);
            Logger::instance()->info(QString("发送电机速度命令: %1").arg(m_speedValue));
        }
    }
}

void MainWindow::onAddDeviceClicked()
{
    QString devicename;
    DeviceType type = getSelectedDeviceType();

    // 检查是否选择了串口
    QString portName = ui->comCombox->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择串口");
        return;
    }

    //检查COM口是否已被占用
    for (auto existingType : devicetypeIdmap.keys()) {
        int existingId = devicetypeIdmap[existingType];
        if (deviceManager->getDevicePortName(existingId) == portName) {
            QMessageBox::warning(this, "警告",
                                 QString("串口 %1 已被 %2 占用").arg(portName, getDeviceNameByType(existingType)));
            return;
        }
    }

    // 检查波特率
    bool ok;
    int baudRate = ui->bateCombox->currentText().toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, "警告", "无效的波特率");
        return;
    }

    int deviceId = deviceManager->addDevice(
        portName,
        baudRate,
        devicename,
        type
        );

    devicetypeIdmap[type] = deviceId;
    QMessageBox::information(this, "成功",
                             QString("设备添加成功！ID: %1, 串口: %2, 波特率: %3")
                                 .arg(deviceId).arg(portName).arg(baudRate));
    updateDeviceTabStatus(type, true);
    updateDeviceListCombox();
}

void MainWindow::onRemoveDeviceClicked()
{
    int index = ui->deviceListCombox->currentIndex();
    if (index == -1) {
        QMessageBox::information(this, "提示", "请先选择要删除的设备");
        return;
    }

    // 获取deviceId
    int deviceId = ui->deviceListCombox->itemData(index).toInt();

    // 根据deviceId找到对应的DeviceType
    DeviceType typeToRemove = DEVICE_TYPE_UNKNOWN;
    for (auto it = devicetypeIdmap.constBegin(); it != devicetypeIdmap.constEnd(); ++it) {
        if (it.value() == deviceId) {
            typeToRemove = it.key();
            break;
        }
    }

    if (typeToRemove == DEVICE_TYPE_UNKNOWN) {
        QMessageBox::warning(this, "警告", "未找到该设备");
        return;
    }

    // 先断开连接
    emit closeSerialRequest(deviceId);

    // 重置对应连接状态
    switch (typeToRemove) {
    case DEVICE_TYPE_MOTOR:
        m_motorConnected = false;
        break;
    case DEVICE_TYPE_SERVO:
        m_servoConnected = false;
        break;
    case DEVICE_TYPE_HUMIDITY_SENSOR:
        m_sensorConnected = false;
        break;
    default:
        break;
    }

    // 从设备管理器中移除
    deviceManager->removeDevice(deviceId);
    devicetypeIdmap.remove(typeToRemove);

    // 更新UI
    updateDeviceTabStatus(typeToRemove, false);

    // 更新设备列表combo
    updateDeviceListCombox();

    QMessageBox::information(this, "成功", "设备已删除");

}

void MainWindow::onDeviceComboBoxChanged(int index)
{

}

void MainWindow::onDeviceStatusChanged(int deviceId, const QString &status)
{
    if (!deviceManager) return;

    DeviceType type = deviceManager->getDeviceType(deviceId);
    QString deviceName;

    switch (type) {
    case DEVICE_TYPE_MOTOR:
        deviceName = "电机";
        break;
    case DEVICE_TYPE_SERVO:
        deviceName = "舵机";
        break;
    case DEVICE_TYPE_HUMIDITY_SENSOR:
        deviceName = "传感器";
        break;
    default:
        deviceName = "设备";
    }

    Logger::instance()->info(QString("%1 (ID:%2) 状态: %3").arg(deviceName).arg(deviceId).arg(status));
}

void MainWindow::onDeviceDataReceived(int deviceId, const QByteArray &data)
{

    if (!deviceManager) return;

    DeviceType type = deviceManager->getDeviceType(deviceId);

    // 记录日志
    Logger::instance()->info(QString("收到设备 %1 数据: %2").arg(deviceId).arg(data.toHex(' ')));

    switch (type) {
    case DEVICE_TYPE_HUMIDITY_SENSOR:
    {
        // 解析温湿度数据
        if (data.size() >= 8) {
            float temperature = *reinterpret_cast<const float*>(data.constData());
            float humidity = *reinterpret_cast<const float*>(data.constData() + 4);

            ui->temperatureLabel->setText(QString("温度: %1°C").arg(temperature, 0, 'f', 1));
            ui->humidityLabel->setText(QString("湿度: %1%").arg(humidity, 0, 'f', 1));
        }
    }
    break;
    case DEVICE_TYPE_MOTOR:
    {
        // 处理电机反馈数据
        if (data.size() >= 2) {
            int speed = static_cast<unsigned char>(data[0]);
            int status = static_cast<unsigned char>(data[1]);
            Logger::instance()->info(QString("电机状态 - 速度: %1, 状态: %2").arg(speed).arg(status));
        }
    }
    break;
    case DEVICE_TYPE_SERVO:
    {
        // 处理舵机反馈数据
        if (data.size() >= 2) {
            int angle = static_cast<unsigned char>(data[0]);
            int status = static_cast<unsigned char>(data[1]);
            Logger::instance()->info(QString("舵机状态 - 角度: %1°, 状态: %2").arg(angle).arg(status));
        }
    }
    case DEVICE_TYPE_UNKNOWN:

    break;
    }
}

void MainWindow::onSendCommandClicked()
{

}

void MainWindow::speedchange()
{
    QString text = QString::number(m_speedValue);
    ui->speedValueLB->setText(" " + text);
    if (deviceManager) {
        sendPendingData();   // 立即发送
    }
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
    else if (cmd.contains("读取") || cmd.contains("传感器")) {
        onReadSensorClicked();
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
void MainWindow::updateDeviceTabStatus(DeviceType type, bool added)
{
    switch (type) {
    case DEVICE_TYPE_MOTOR:
        ui->tabWidget->setTabText(0, added ? "电机控制 (已添加)" : "电机控制 (未添加)");
        // ui->motorControlWidget->setEnabled(added && m_motorConnected);

        if (added && devicetypeIdmap.contains(DEVICE_TYPE_MOTOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_MOTOR];
            if (m_motorConnected) {
                QString port = deviceManager->getDevicePortName(deviceId);
                int baud = deviceManager->getDeviceBaudRate(deviceId);
                ui->motorStatusLabel->setText(QString("已连接: %1, 波特率: %2").arg(port).arg(baud));
            } else {
                ui->motorStatusLabel->setText("未连接");
            }
        } else {
            ui->motorStatusLabel->setText("未添加设备");
        }
        break;

    case DEVICE_TYPE_SERVO:
        ui->tabWidget->setTabText(1, added ? "舵机控制 (已添加)" : "舵机控制 (未添加)");
        // ui->servoControlWidget->setEnabled(added && m_servoConnected);

        if (added && devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_SERVO];
            if (m_servoConnected) {
                QString port = deviceManager->getDevicePortName(deviceId);
                int baud = deviceManager->getDeviceBaudRate(deviceId);
                ui->servoStatusLabel->setText(QString("已连接: %1, 波特率: %2").arg(port).arg(baud));
            } else {
                ui->servoStatusLabel->setText("未连接");
            }
        } else {
            ui->servoStatusLabel->setText("未添加设备");
        }
        break;

    case DEVICE_TYPE_HUMIDITY_SENSOR:
        ui->tabWidget->setTabText(2, added ? "温湿度传感器 (已添加)" : "温湿度传感器 (未添加)");
        // ui->sensorControlWidget->setEnabled(added && m_sensorConnected);

        if (added && devicetypeIdmap.contains(DEVICE_TYPE_HUMIDITY_SENSOR)) {
            int deviceId = devicetypeIdmap[DEVICE_TYPE_HUMIDITY_SENSOR];
            if (m_sensorConnected) {
                QString port = deviceManager->getDevicePortName(deviceId);
                int baud = deviceManager->getDeviceBaudRate(deviceId);
                ui->sensorStatusLabel->setText(QString("已连接: %1, 波特率: %2").arg(port).arg(baud));
            } else {
                ui->sensorStatusLabel->setText("未连接");
            }
        } else {
            ui->sensorStatusLabel->setText("未添加设备");
        }
        break;

    case DEVICE_TYPE_UNKNOWN:
        qDebug()<<"未知设备";
        break;
    }
}
DeviceType MainWindow::getSelectedDeviceType(){
    QString devicename;
    DeviceType deviceType = DEVICE_TYPE_UNKNOWN;
    if(ui->devicecombox->currentText()=="舵机"){
        devicename="CMD_TYPE_SERVO_CONTROL";
        deviceType=DEVICE_TYPE_SERVO;//舵机
    }
    else if(ui->devicecombox->currentText()=="电机"){
        devicename="CMD_TYPE_MOTOR_CONTROL";
        deviceType = DEVICE_TYPE_MOTOR;   // 电机类型
    }else if(ui->devicecombox->currentText()=="传感器"){
        devicename="CMD_TYPE_SENSOR";
        deviceType = DEVICE_TYPE_HUMIDITY_SENSOR;  // 传感器类型
    }
    if (devicetypeIdmap.contains(deviceType)) {
        QMessageBox::information(this, "提示", "该类型设备已添加");

    }
    return deviceType;
}
QString MainWindow::getDeviceNameByType(DeviceType type)
{
    switch (type) {
    case DEVICE_TYPE_MOTOR:
        return "CMD_TYPE_MOTOR_CONTROL";
    case DEVICE_TYPE_SERVO:
        return "CMD_TYPE_SERVO_CONTROL";
    case DEVICE_TYPE_HUMIDITY_SENSOR:
        return "CMD_TYPE_SENSOR";
    default:
        return "CMD_TYPE_UNKNOWN";
    }
}
//命令控制
void MainWindow::onMotorAngleSlider_valueChanged(int value)
{
    m_speedValue = value;
    speedchange();

    if (devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
        int deviceId = devicetypeIdmap[DEVICE_TYPE_SERVO];
        QByteArray command = CommandProcessor::createServoAngleCommand(value);
        
        qDebug() << "发送舵机命令 - 设备ID:" << deviceId 
                 << ", 角度:" << value 
                 << ", 帧内容:" << command.toHex(' ').toUpper();
        
        emit sendDataRequest(deviceId, command);
        Logger::instance()->info(QString("发送舵机角度命令: %1").arg(value));
    }
}
void MainWindow::onReadSensorClicked()
{
    if (devicetypeIdmap.contains(DEVICE_TYPE_HUMIDITY_SENSOR)) {
        int deviceId = devicetypeIdmap[DEVICE_TYPE_HUMIDITY_SENSOR];
        QByteArray command = CommandProcessor::createSensorReadCommand();
        emit sendDataRequest(deviceId, command);
        Logger::instance()->instance()->info("发送传感器读取命令");
    }
}

void MainWindow::onLogMessageReceived(const QString &message)
{
    if (ui->logger) {
        ui->logger->append(message);
    }
}
void MainWindow::updateDeviceListCombox()
{
    ui->deviceListCombox->clear();

    for (auto it = devicetypeIdmap.constBegin(); it != devicetypeIdmap.constEnd(); ++it) {
        DeviceType type = it.key();
        int deviceId = it.value();
        QString port = deviceManager->getDevicePortName(deviceId);
        QString deviceName = getDeviceNameByType(type);
        QString itemText = QString("%1 - %2 (ID:%3)").arg(port, deviceName, QString::number(deviceId));
        ui->deviceListCombox->addItem(itemText,deviceId);
    }
}
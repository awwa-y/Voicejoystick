#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "VoskWorker.h"
#include <QDebug>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include "commandprocessor.h"
#include "log.h"
#include <cstring>

// 按文字给状态标签着色(已连接/监听=绿,未连接/出错=橙,其余=灰)
static void colorizeStatus(QLabel *label)
{
    const QString text = label->text();
    if (text.startsWith("已连接") || text.contains("正在监听"))
        label->setStyleSheet("color:#4caf50;font-weight:bold;background:transparent;");
    else if (text.contains("未连接") || text.contains("出错"))
        label->setStyleSheet("color:#f4511e;font-weight:bold;background:transparent;");
    else
        label->setStyleSheet("color:#9a9a9a;background:transparent;");
}

// 构造：建 UI、初始化串口逻辑、连好所有信号槽（设备/摇杆/语音/日志）
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isConnected(false)
    , m_speedValue(0)
{
    ui->setupUi(this);
    applyDarkTheme();
    buildModernLayout();
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
    connect(deviceManager, &DeviceManager::deviceFrameReceived, this, &MainWindow::onFrameReceived);
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

    //日志：init 只在 main.cpp 做一次（单例重复 init 会清空日志文件），这里只接信号
    connect(Logger::instance(), &Logger::logMessage, this, &MainWindow::onLogMessageReceived);

    // 网关客户端(边缘代理):串口帧转发 epoll 网关
    m_gateway = new GatewayClient(this);
    connect(m_gateway, &GatewayClient::gatewayConnected, this, [this]() {
        m_gatewayConnected = true;
        if (m_gatewayButton) m_gatewayButton->setText("断开网关");
        Logger::instance()->info("网关已连接");
    });
    connect(m_gateway, &GatewayClient::gatewayDisconnected, this, [this]() {
        m_gatewayConnected = false;
        if (m_gatewayButton) m_gatewayButton->setText("连接网关");
        Logger::instance()->info("网关已断开");
    });
    connect(m_gateway, &GatewayClient::gatewayError, this, [this](const QString &msg) {
        Logger::instance()->info(QString("网关错误: %1").arg(msg));
    });
    connect(m_gateway, &GatewayClient::gatewayMessage, this, [this](const QByteArray &frame) {
        Logger::instance()->info(QString("网关下发 %1 字节").arg(frame.size()));
    });
}

// 析构：先停语音线程（quit+wait 等它退出），再删 UI
MainWindow::~MainWindow()
{
    emit stopVoiceRecognition();
    m_voskThread.quit();
    m_voskThread.wait();
    delete ui;
}


// 枚举系统可用串口名（给下拉框）
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

// 读 UI 上选的波特率，解析失败给默认 115200
int MainWindow::getbaud()
{
    baud = ui->bateCombox->currentText().toInt();
    return baud;
}

// 串口初始化：COM 口自动识别系统端口(纯下拉选择)，波特率默认 115200
void MainWindow::Qslwork_init()
{
    ui->comCombox->clear();

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
    ui->bateCombox->setCurrentText("115200");  // STM32 通信约定波特率
    ui->devicecombox->addItems(devicename);

}
//电机
// 电机连接/断开按钮：没连就按 UI 选的串口添加设备，已连就移除
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
// 舵机连接/断开按钮，逻辑同电机
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
// 传感器连接/断开按钮，逻辑同电机
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

// 刷新按钮：重新枚举串口填进下拉框
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

// 收到设备原始字节流：记 hex 日志。协议解析在 DeviceManager 侧，整帧走 onFrameReceived
void MainWindow::onDataReceived(int deviceId, const QByteArray &data)
{
    Logger::instance()->info(QString("收到设备%1原始数据: %2")
                                 .arg(deviceId).arg(data.toHex(' ').toUpper()));
}

// 串口错误：弹窗提示并重置该设备的连接状态
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

// 串口打开成功：置连接状态，按钮变"断开"
void MainWindow::onSerialOpened(int deviceId)
{
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

// 串口关闭（目前空实现）
void MainWindow::onSerialClosed()
{

}

// 摇杆位置变化：只存进 pending，真正发送由 40ms 定时器做
void MainWindow::changenewdata(int newx, int newy)
{
    m_pendingX = newx;
    m_pendingY = newy;
    m_hasPending = true;
    if (devicetypeIdmap.contains(DEVICE_TYPE_SERVO)) {
        Logger::instance()->info(QString("摇杆数据更新: X=%1, Y=%2").arg(newx).arg(newy));
    }
}

// 定时发送：有 pending 数据就组控制帧发给电机（40ms 周期，平滑流量）
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

// 加速按钮：速度 +10 并发调速帧
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

// 减速按钮：速度 -10 并发调速帧
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

// 添加设备按钮：按 UI 选择创建设备并打开串口
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

// 移除设备按钮：断开并删掉设备
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

// 切换设备下拉框：更新活动设备
void MainWindow::onDeviceComboBoxChanged(int index)
{

}

// 设备状态变化：更新对应页签的显示
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

// 收到完整协议帧：按设备类型更新 UI
// data 是帧内数据段（不是裸字节流），浮点用 memcpy 取，避免未对齐的 reinterpret_cast
void MainWindow::onFrameReceived(int deviceId, quint8 cmdType, quint8 deviceType, const QByteArray &data)
{
    Q_UNUSED(cmdType)
    Q_UNUSED(deviceType)

    // 边缘代理:设备上行帧同步转发网关(未连接时 forwardDeviceData 内部静默跳过)
    if (m_gateway) {
        QByteArray raw = data;   // 数据段还原成完整帧(头4+data+校验+尾1)
        QByteArray frame;
        frame.resize(4 + raw.size() + 2);
        frame[0] = char(0xAA);
        frame[1] = char(cmdType);
        frame[2] = char(deviceType);
        frame[3] = char(raw.size());
        frame.replace(4, raw.size(), raw);
        frame[frame.size() - 2] = 0x00;   // 校验位(简化:原帧校验已在解析层消费)
        frame[frame.size() - 1] = char(0x55);
        m_gateway->forwardDeviceData(frame);
    }

    if (!deviceManager) return;

    DeviceType type = deviceManager->getDeviceType(deviceId);

    switch (type) {
    case DEVICE_TYPE_HUMIDITY_SENSOR:
    {
        // 数据段：前 4 字节温度 float，后 4 字节湿度 float
        if (data.size() >= 8) {
            float temperature = 0.0f, humidity = 0.0f;
            memcpy(&temperature, data.constData(), sizeof(float));
            memcpy(&humidity, data.constData() + 4, sizeof(float));

            ui->temperatureLabel->setText(QString("温度: %1°C").arg(temperature, 0, 'f', 1));
            ui->humidityLabel->setText(QString("湿度: %1%").arg(humidity, 0, 'f', 1));
        }
    }
    break;
    case DEVICE_TYPE_MOTOR:
    {
        if (data.size() >= 2) {
            int speed = static_cast<unsigned char>(data[0]);
            int status = static_cast<unsigned char>(data[1]);
            Logger::instance()->info(QString("电机状态 - 速度: %1, 状态: %2").arg(speed).arg(status));
        }
    }
    break;
    case DEVICE_TYPE_SERVO:
    {
        if (data.size() >= 2) {
            int angle = static_cast<unsigned char>(data[0]);
            int status = static_cast<unsigned char>(data[1]);
            Logger::instance()->info(QString("舵机状态 - 角度: %1°, 状态: %2").arg(angle).arg(status));
        }
    }
    break;
    default:
        break;
    }
}

// 手动发送命令按钮（调试用）
void MainWindow::onSendCommandClicked()
{

}

// 速度变化：更新显示并发调速帧
void MainWindow::speedchange()
{
    QString text = QString::number(m_speedValue);
    ui->speedValueLB->setText(" " + text);
    if (deviceManager) {
        sendPendingData();   // 立即发送
    }
}
// 前进按钮：摇杆打到正前方
void MainWindow::onForwardButtonClicked()
{
    qDebug() << "Forward button clicked";
}

// 后退按钮
void MainWindow::onBackwardButtonClicked()
{
    qDebug() << "Backward button clicked";
}

// 加速按钮（转发到 speedUp）
void MainWindow::onSpeedUpButtonClicked()
{
    speedUp();
}

// 减速按钮（转发到 speedDown）
void MainWindow::onSpeedDownButtonClicked()
{
    speedDown();
}
// 语音开关：开就发 startVoiceRecognition（队列连接到语音线程）
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
    colorizeStatus(ui->voiceStatusLabel);
}

// 网关连接/断开:解析"IP:端口"格式,已连接则断开
void MainWindow::onGatewayConnectClicked()
{
    if (m_gatewayConnected) {
        m_gateway->disconnectFromGateway();
        return;
    }
    QString text = m_gatewayEdit->text().trimmed();
    QStringList parts = text.split(':');
    if (parts.size() != 2) {
        Logger::instance()->info("网关地址格式错误,应为 IP:端口,如 192.168.1.100:9000");
        return;
    }
    bool ok = false;
    quint16 port = parts[1].toUShort(&ok);
    if (!ok || port == 0) {
        Logger::instance()->info("网关端口无效");
        return;
    }
    Logger::instance()->info(QString("正在连接网关 %1:%2 ...").arg(parts[0]).arg(port));
    m_gateway->connectToGateway(parts[0], port);
}

// 识别到语音指令：加速/减速发调速帧，左转/右转驱动摇杆
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

// 语音引擎状态显示
void MainWindow::onVoiceStatus(const QString &msg)
{
    ui->voiceStatusLabel->setText(QString("语音状态：%1").arg(msg));
    colorizeStatus(ui->voiceStatusLabel);
}

// 语音错误显示
void MainWindow::onVoiceError(const QString &err)
{
    ui->voiceResultEdit->append(QString("错误：%1").arg(err));
    ui->voiceStatusLabel->setText("语音状态：出错");
    ui->voiceToggleButton->setChecked(false);
    colorizeStatus(ui->voiceStatusLabel);
}
// 更新设备页签的可用状态（设备加进来/移除后调用）
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
    // 状态文字可能变了,刷新三个设备标签着色
    colorizeStatus(ui->motorStatusLabel);
    colorizeStatus(ui->servoStatusLabel);
    colorizeStatus(ui->sensorStatusLabel);
}
// 读 UI 当前选中的设备类型（下拉框）
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
// 设备类型转显示名
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
// 舵机角度滑条变化：组角度帧发给舵机(不碰速度,速度只归电机控制)
void MainWindow::onMotorAngleSlider_valueChanged(int value)
{
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
// 读传感器按钮：发传感器读取帧
void MainWindow::onReadSensorClicked()
{
    if (devicetypeIdmap.contains(DEVICE_TYPE_HUMIDITY_SENSOR)) {
        int deviceId = devicetypeIdmap[DEVICE_TYPE_HUMIDITY_SENSOR];
        QByteArray command = CommandProcessor::createSensorReadCommand();
        emit sendDataRequest(deviceId, command);
        Logger::instance()->info("发送传感器读取命令");
    }
}

// 日志显示到 UI 文本框
void MainWindow::onLogMessageReceived(const QString &message)
{
    if (ui->logger) {
        ui->logger->append(message);
    }
}
// 刷新设备下拉框（从 DeviceManager 拉当前列表）
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

// 全局深色主题:统一背景/字体/控件样式,只动样式不动布局
void MainWindow::applyDarkTheme()
{
    setStyleSheet(R"(
        /* 全局背景与字体 */
        QMainWindow, QWidget {
            background-color: #1e1e1e;
            color: #d8d8d8;
            font-size: 10pt;
        }
        QLabel {
            background: transparent;
            color: #d8d8d8;
        }
        /* 卡片容器:直角面板分区,细丝印线(经典组态软件风) */
        QFrame#card {
            background-color: #262626;
            border: 1px solid #3a3a3a;
            border-radius: 0px;
        }
        QLabel#cardTitle {
            background: transparent;
            color: #8a8a8a;
            font-size: 9pt;
            font-weight: normal;
            border: none;
            border-bottom: 1px solid #3a3a3a;
            padding: 0 0 4px 0;
        }
        /* 摇杆速度/位置读数:等宽数字 */
        #speedValueLB {
            background: transparent;
            color: #4caf50;
            font-family: "Consolas";
            font-size: 13pt;
            font-weight: bold;
        }
        #posReadout {
            background: transparent;
            color: #4caf50;
            font-family: "Consolas";
            font-size: 10pt;
        }
        /* 按钮:直角平面 */
        QPushButton {
            background-color: #2f2f2f;
            border: 1px solid #444444;
            border-radius: 0px;
            padding: 5px 10px;
            color: #d0d0d0;
        }
        QPushButton:hover {
            background-color: #383838;
            border: 1px solid #555555;
        }
        QPushButton:pressed {
            background-color: #242424;
            border: 1px solid #2a2a2a;
        }
        /* checked 态(语音开关):开启后变绿 */
        QPushButton:checked {
            background-color: #294d2c;
            border: 1px solid #3d8b40;
            color: #d8ecd8;
        }
        QPushButton:checked:hover {
            background-color: #2f5a33;
        }
        /* 启动:主操作按钮,哑光绿 */
        #beginbotton {
            background-color: #294d2c;
            border: 1px solid #3d8b40;
            color: #d8ecd8;
            font-weight: bold;
        }
        #beginbotton:hover {
            background-color: #2f5a33;
            border: 1px solid #4a9c4e;
        }
        #beginbotton:pressed {
            background-color: #214023;
        }
        /* 移除设备:危险操作,红色 */
        #removedeviceButton {
            background-color: #5a2323;
            border: 1px solid #b03a3a;
            color: #e8c8c8;
        }
        #removedeviceButton:hover {
            background-color: #6a2a2a;
            border: 1px solid #c04545;
        }
        #removedeviceButton:pressed {
            background-color: #4a1d1d;
        }
        /* 下拉框 */
        QComboBox {
            background-color: #262626;
            border: 1px solid #3a3a3a;
            border-radius: 0px;
            padding: 3px 8px;
            color: #d0d0d0;
        }
        QComboBox::drop-down { border: none; width: 22px; }
        QComboBox QAbstractItemView {
            background-color: #242424;
            border: 1px solid #3a3a3a;
            selection-background-color: #294d2c;
            selection-color: #ffffff;
            color: #d0d0d0;
        }
        /* 日志/语音结果文本框 */
        QTextEdit {
            background-color: #1a1a1a;
            border: 1px solid #333333;
            border-radius: 0px;
            color: #b8b8b8;
            font-family: "Consolas";
            font-size: 9pt;
        }
        /* 电机/舵机/传感器标签页 */
        QTabWidget::pane {
            border: 1px solid #3a3a3a;
            border-radius: 0px;
            background: transparent;
            top: -1px;
        }
        QTabBar::tab {
            background-color: #262626;
            color: #909090;
            padding: 4px 12px;
            border: 1px solid #3a3a3a;
            border-bottom: none;
            border-top-left-radius: 0px;
            border-top-right-radius: 0px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            color: #4caf50;
            background-color: #2b2b2b;
            font-weight: bold;
            border-bottom: 2px solid #3d8b40;
        }
        /* 舵机角度滑条:方形滑块 */
        QSlider::groove:horizontal {
            height: 6px;
            background: #2a2a2a;
            border: 1px solid #333333;
            border-radius: 0px;
        }
        QSlider::sub-page:horizontal {
            background: #3d8b40;
            border-radius: 0px;
        }
        QSlider::handle:horizontal {
            width: 12px;
            height: 16px;
            margin: -6px 0;
            background: #c8c8c8;
            border: 1px solid #888888;
            border-radius: 0px;
        }
        /* 滚动条 */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #4a4a4a;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: #5a5a5a; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #4a4a4a;
            border-radius: 4px;
            min-width: 30px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )");
}

// 卡片容器:深色面板 + 小标题 + 内容布局(标题轻量化,不占竖向空间)
static QFrame *makeCard(const QString &title, QLayout *inner)
{
    auto *card = new QFrame;
    card->setObjectName("card");
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(10, 6, 10, 8);
    lay->setSpacing(6);
    auto *t = new QLabel(title, card);
    t->setObjectName("cardTitle");
    lay->addWidget(t);
    lay->addLayout(inner);
    return card;
}

// 卡片式布局重排:废弃 Designer 的绝对定位,把控件 reparent 进分组卡片;
// 控件名与信号槽连接全部不变,业务代码零改动
void MainWindow::buildModernLayout()
{
    setWindowTitle("多设备串口控制上位机");
    resize(1180, 760);

    // 标签文字规范化(去掉原来的前导空格)
    ui->devicelabel->setText("设备类型:");
    ui->comlabel->setText("串口端口:");
    ui->botelabel->setText("波特率:");
    ui->voiceStatusLabel->setAlignment(Qt::AlignCenter);

    // 状态标签初始着色(后续在各更新点调 colorizeStatus 刷新)
    colorizeStatus(ui->motorStatusLabel);
    colorizeStatus(ui->servoStatusLabel);
    colorizeStatus(ui->sensorStatusLabel);
    colorizeStatus(ui->voiceStatusLabel);

    // 三个 tab 页内部重排(原来也是绝对定位)
    auto *motorLay = new QVBoxLayout(ui->motorControlWidget);
    motorLay->setContentsMargins(12, 10, 12, 12);
    motorLay->setSpacing(10);
    motorLay->addWidget(ui->motorStatusLabel, 0, Qt::AlignHCenter);
    motorLay->addStretch();
    motorLay->addWidget(ui->motorConnectButton, 0, Qt::AlignHCenter);
    ui->motorConnectButton->setFixedWidth(140);

    auto *servoLay = new QVBoxLayout(ui->servoControlWidget);
    servoLay->setContentsMargins(12, 10, 12, 12);
    servoLay->setSpacing(10);
    servoLay->addWidget(ui->servoStatusLabel, 0, Qt::AlignHCenter);
    servoLay->addStretch();
    servoLay->addWidget(ui->servoConnectButton, 0, Qt::AlignHCenter);
    ui->servoConnectButton->setFixedWidth(140);

    auto *sensorLay = new QVBoxLayout(ui->sensorControlWidget);
    sensorLay->setContentsMargins(12, 10, 12, 12);
    sensorLay->setSpacing(10);
    auto *valRow = new QHBoxLayout;
    valRow->setSpacing(20);
    valRow->addWidget(ui->humidityLabel);
    valRow->addWidget(ui->temperatureLabel);
    sensorLay->addLayout(valRow);
    sensorLay->addStretch();
    auto *senBtnRow = new QHBoxLayout;
    senBtnRow->setSpacing(10);
    senBtnRow->addWidget(ui->readSensorButton);
    senBtnRow->addWidget(ui->sensorConnectButton);
    sensorLay->addLayout(senBtnRow);
    sensorLay->addWidget(ui->sensorStatusLabel, 0, Qt::AlignHCenter);

    // 主布局:三栏(间距收紧,贴近工业软件密度)
    auto *root = new QHBoxLayout(ui->centralwidget);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    // 左栏:手动控制 + 摇杆
    auto *left = new QVBoxLayout;
    left->setSpacing(10);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addWidget(ui->fastbutton);
    btnRow->addWidget(ui->lowbutton);
    btnRow->addWidget(ui->beginbotton);
    auto *manualCard = makeCard("手动控制", btnRow);
    manualCard->setFixedWidth(360);
    left->addWidget(manualCard);

    // 摇杆卡:不用大标题,首行即实时坐标读数(省出竖向空间)
    auto *joyCard = new QFrame;
    joyCard->setObjectName("card");
    auto *joyLay = new QVBoxLayout(joyCard);
    joyLay->setContentsMargins(10, 6, 10, 8);
    joyLay->setSpacing(6);
    auto *posRow = new QHBoxLayout;
    auto *posXLB = new QLabel("X: 0");
    auto *posYLB = new QLabel("Y: 0");
    for (auto *lb : {posXLB, posYLB}) lb->setObjectName("posReadout");
    posRow->addStretch();
    posRow->addWidget(posXLB);
    posRow->addSpacing(16);
    posRow->addWidget(posYLB);
    posRow->addStretch();
    joyLay->addLayout(posRow);
    connect(ui->widget, &JoystickManager::joystickChanged, this,
            [posXLB, posYLB](int x, int y) {
                posXLB->setText(QString("X: %1").arg(x));
                posYLB->setText(QString("Y: %1").arg(y));
            });
    ui->widget->setMinimumSize(331, 331);
    joyLay->addWidget(ui->widget, 0, Qt::AlignHCenter);
    auto *speedRow = new QHBoxLayout;
    speedRow->setSpacing(8);
    speedRow->addWidget(ui->speedlabel);
    speedRow->addWidget(ui->speedValueLB);
    speedRow->addStretch();
    joyLay->addLayout(speedRow);
    joyCard->setFixedWidth(360);
    left->addWidget(joyCard, 1);
    left->addStretch();
    root->addLayout(left);

    // 中栏:设备控制 + 设备列表
    auto *center = new QVBoxLayout;
    center->setSpacing(10);

    // 设备控制:tab 高度比自然高度加约三成,留白更舒展
    auto *tabInner = new QVBoxLayout;
    tabInner->setSpacing(10);
    ui->tabWidget->setMinimumHeight(280);
    tabInner->addWidget(ui->tabWidget);
    auto *sliderRow = new QHBoxLayout;
    sliderRow->setSpacing(10);
    auto *sliderLabel = new QLabel("舵机角度:");
    sliderRow->addWidget(sliderLabel);
    sliderRow->addWidget(ui->servoAngleSlider, 1);
    tabInner->addLayout(sliderRow);
    center->addWidget(makeCard("设备控制", tabInner));

    auto *listInner = new QVBoxLayout;
    listInner->setSpacing(8);
    listInner->addWidget(ui->deviceListCombox);
    listInner->addWidget(ui->removedeviceButton);
    auto *listCard = makeCard("设备列表", listInner);
    listCard->setFixedHeight(120);
    center->addWidget(listCard);

    // 运行日志放中栏底部,占满剩余空间(日志区宽一点更实用)
    auto *logInner = new QVBoxLayout;
    logInner->setSpacing(8);
    logInner->addWidget(ui->logger);
    center->addWidget(makeCard("运行日志", logInner), 1);
    root->addLayout(center, 1);

    // 右栏:设备连接 + 语音(拉伸填满)
    auto *right = new QVBoxLayout;
    right->setSpacing(10);

    auto *connGrid = new QGridLayout;
    connGrid->setSpacing(8);
    // 三个标签统一宽度+右对齐,与下拉框列对齐
    for (auto *lb : {ui->devicelabel, ui->comlabel, ui->botelabel}) {
        lb->setFixedWidth(76);
        lb->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    connGrid->addWidget(ui->devicelabel, 0, 0);
    connGrid->addWidget(ui->devicecombox, 0, 1);
    connGrid->addWidget(ui->comlabel, 1, 0);
    connGrid->addWidget(ui->comCombox, 1, 1);
    connGrid->addWidget(ui->botelabel, 2, 0);
    connGrid->addWidget(ui->bateCombox, 2, 1);
    auto *connBtnRow = new QHBoxLayout;
    connBtnRow->setSpacing(8);
    connBtnRow->addWidget(ui->adddevice, 1);
    connBtnRow->addWidget(ui->freshbutton, 1);
    connGrid->addLayout(connBtnRow, 3, 0, 1, 2);
    // 网关(边缘代理):地址输入 + 连接按钮,串口帧经 TCP 转发 epoll 网关
    auto *gatewayLabel = new QLabel("网关地址:");
    gatewayLabel->setFixedWidth(76);
    gatewayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_gatewayEdit = new QLineEdit("192.168.1.100:9000");
    m_gatewayButton = new QPushButton("连接网关");
    connect(m_gatewayButton, &QPushButton::clicked, this, &MainWindow::onGatewayConnectClicked);
    connGrid->addWidget(gatewayLabel, 4, 0);
    connGrid->addWidget(m_gatewayEdit, 4, 1);
    connGrid->addWidget(m_gatewayButton, 5, 0, 1, 2);
    auto *connCard = makeCard("设备连接", connGrid);
    connCard->setFixedWidth(320);
    right->addWidget(connCard);

    auto *voiceInner = new QVBoxLayout;
    voiceInner->setSpacing(8);
    voiceInner->addWidget(ui->voiceToggleButton);
    voiceInner->addWidget(ui->voiceStatusLabel);
    voiceInner->addWidget(ui->voiceResultEdit, 1);
    auto *voiceCard = makeCard("语音控制", voiceInner);
    voiceCard->setFixedWidth(320);
    right->addWidget(voiceCard, 1);
    root->addLayout(right);

    // 遗弃的 Designer 容器壳与孤儿标签隐藏
    ui->horizontalLayoutWidget->hide();
    ui->verticalLayoutWidget->hide();
    ui->horizontalLayoutWidget_2->hide();
    ui->verticalLayoutWidget_2->hide();
    ui->loglabel->hide();
}
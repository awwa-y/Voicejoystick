#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "uitheme.h"
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
#include <QSignalBlocker>
#include <QMenu>
#include <QAction>
#include <cstring>

// 构造：建 UI、初始化串口逻辑、连好所有信号槽（设备/摇杆/语音/日志）
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isConnected(false)
    , m_speedValue(0)
{
    ui->setupUi(this);
    UiTheme::applyDarkTheme(this);
    UiTheme::GatewayWidgets gatewayWidgets;
    UiTheme::buildModernLayout(ui, this, gatewayWidgets);
    m_gatewayEdit = gatewayWidgets.edit;
    m_gatewayButton = gatewayWidgets.button;
    connect(m_gatewayButton, &QPushButton::clicked, this, &MainWindow::onGatewayConnectClicked);
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
    connect(ui->motorConnectButton, &QPushButton::clicked, this, [this]{ toggleDevice(DEVICE_TYPE_MOTOR); });
    connect(ui->servoConnectButton, &QPushButton::clicked, this, [this]{ toggleDevice(DEVICE_TYPE_SERVO); });
    connect(ui->sensorConnectButton, &QPushButton::clicked, this, [this]{ toggleDevice(DEVICE_TYPE_HUMIDITY_SENSOR); });
    connect(ui->removedeviceButton, &QPushButton::clicked, this, &MainWindow::onRemoveDeviceClicked);
    connect(ui->deviceListCombox, &QComboBox::currentIndexChanged, this, &MainWindow::onDeviceComboBoxChanged);

    //设备连接
    deviceManager =new DeviceManager(this);
    connect(ui->adddevice,&QPushButton::clicked,this,&MainWindow::onAddDeviceClicked);

    // 调试菜单：模拟设备模式（无硬件也能跑通发送路径、把帧打进日志，方便演示/面试）
    QMenu *debugMenu = menuBar()->addMenu("调试");
    QAction *simAct = debugMenu->addAction("模拟设备（无需硬件）");
    simAct->setCheckable(true);
    connect(simAct, &QAction::toggled, this, [this](bool on) {
        if (deviceManager) deviceManager->setSimulate(on);
        Logger::instance()->info(on ? "已开启模拟设备模式（不打开真实串口）"
                                    : "已关闭模拟设备模式");
    });

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
        // 设备断开：按真实状态刷新活动设备 + 列表
        DeviceType type = deviceManager->getDeviceType(deviceId);
        refreshActiveDevice(type);
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
// ---- P0: 三套连接逻辑收敛成一个入口 ----
// 连接/断开指定类型的设备：未连接则打开串口，已连接则关闭
void MainWindow::toggleDevice(DeviceType type)
{
    if (!devicetypeIdmap.contains(type)) {
        QString name;
        switch (type) {
        case DEVICE_TYPE_MOTOR:           name = "电机"; break;
        case DEVICE_TYPE_SERVO:           name = "舵机"; break;
        case DEVICE_TYPE_HUMIDITY_SENSOR: name = "传感器"; break;
        default:                          name = "设备"; break;
        }
        QMessageBox::warning(this, "警告", QString("请先添加%1设备").arg(name));
        return;
    }

    int deviceId = devicetypeIdmap[type];
    if (isTypeConnected(type)) {
        emit closeSerialRequest(deviceId);
        refreshActiveDevice(type);   // 异步关闭，状态随后由 deviceDisconnected 信号再刷一次
    } else {
        emit openSerialRequest(deviceManager->getDevicePortName(deviceId),
                               deviceManager->getDeviceBaudRate(deviceId),
                               deviceId);
    }
}

// 类型 → 对应连接按钮（不存在返回 nullptr）
QPushButton *MainWindow::connectButton(DeviceType type)
{
    switch (type) {
    case DEVICE_TYPE_MOTOR:           return ui->motorConnectButton;
    case DEVICE_TYPE_SERVO:           return ui->servoConnectButton;
    case DEVICE_TYPE_HUMIDITY_SENSOR: return ui->sensorConnectButton;
    default:                          return nullptr;
    }
}

// 类型 → 当前活动设备是否已连接（以 DeviceManager 为准，避免缓存漂移）
bool MainWindow::isTypeConnected(DeviceType type) const
{
    int id = devicetypeIdmap.value(type, -1);
    return id > 0 && deviceManager && deviceManager->isDeviceConnected(id);
}

// 从 DeviceManager 重算"活动设备"的连接状态，刷新页签/按钮
// 多实例时，页签只反映该类型当前活动设备的状态
void MainWindow::refreshActiveDevice(DeviceType type)
{
    int deviceId = devicetypeIdmap.value(type, -1);
    bool connected = (deviceId > 0 && deviceManager && deviceManager->isDeviceConnected(deviceId));

    switch (type) {
    case DEVICE_TYPE_MOTOR:           m_motorConnected  = connected; break;
    case DEVICE_TYPE_SERVO:           m_servoConnected  = connected; break;
    case DEVICE_TYPE_HUMIDITY_SENSOR: m_sensorConnected = connected; break;
    default: break;
    }

    if (QPushButton *btn = connectButton(type))
        btn->setText(connected ? "断开" : "连接");

    updateDeviceTabStatus(type, !m_devicesByType.value(type).isEmpty());
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

    // 按设备真实状态刷新（错误后 isConnected 已为 false）
    refreshActiveDevice(type);
}

// 串口打开成功：置连接状态，按钮变"断开"
void MainWindow::onSerialOpened(int deviceId)
{
    qDebug() << "设备" << deviceId << "连接成功";

    DeviceType type = deviceManager->getDeviceType(deviceId);

    refreshActiveDevice(type);
    updateDeviceListCombox();   // 列表里的 [已连接] 标记同步刷新
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

// 定时发送：摇杆有变化就按 40ms 周期下发。舵机走角度帧，电机走 X/Y+速度控制帧。
void MainWindow::sendPendingData()
{
    if (!deviceManager || !m_hasPending) return;

    // 舵机：X 轴映射成 0..180 角度
    int servoId = devicetypeIdmap.value(DEVICE_TYPE_SERVO, -1);
    if (servoId > 0 && deviceManager->isDeviceConnected(servoId)) {
        int servoAngle = qBound(0, 90 + m_pendingX, 180);
        emit sendDataRequest(servoId, CommandProcessor::createServoAngleCommand(servoAngle));
        Logger::instance()->info(QString("发送舵机角度: %1°").arg(servoAngle));
    }

    // 电机：X/Y 方向 + 速度，走控制帧（Y 轴终于用上了）
    int motorId = devicetypeIdmap.value(DEVICE_TYPE_MOTOR, -1);
    if (motorId > 0 && deviceManager->isDeviceConnected(motorId)) {
        emit sendDataRequest(motorId,
                             CommandProcessor::createControlFrame(m_pendingX, m_pendingY, m_speedValue));
        Logger::instance()->info(QString("发送电机控制: X=%1 Y=%2 Speed=%3")
                                     .arg(m_pendingX).arg(m_pendingY).arg(m_speedValue));
    }

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

    // 检查是否选择了串口（模拟模式下允许不选，自动用 SIM-N）
    QString portName = ui->comCombox->currentText();
    if (portName.isEmpty()) {
        if (deviceManager->isSimulate()) {
            portName = QString("SIM-%1").arg(deviceManager->getDeviceList().size() + 1);
        } else {
            QMessageBox::warning(this, "警告", "请先选择串口");
            return;
        }
    }

    // 串口占用检查：遍历所有已添加实例（不只活动设备）
    for (auto it = m_devicesByType.constBegin(); it != m_devicesByType.constEnd(); ++it) {
        for (int existingId : it.value()) {
            if (deviceManager->getDevicePortName(existingId) == portName) {
                QMessageBox::warning(this, "警告",
                                     QString("串口 %1 已被 %2 占用").arg(portName, getDeviceNameByType(it.key())));
                return;
            }
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

    m_devicesByType[type].append(deviceId);   // 同类型多实例都记下来
    devicetypeIdmap[type] = deviceId;         // 新设备设为该类型的活动设备
    QMessageBox::information(this, "成功",
                             QString("设备添加成功！ID: %1, 串口: %2, 波特率: %3")
                                 .arg(deviceId).arg(portName).arg(baudRate));
    refreshActiveDevice(type);
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

    // 获取deviceId 与 其类型（以 DeviceManager 为准，支持同类型多实例）
    int deviceId = ui->deviceListCombox->itemData(index).toInt();
    DeviceType typeToRemove = deviceManager->getDeviceType(deviceId);
    if (typeToRemove == DEVICE_TYPE_UNKNOWN) {
        QMessageBox::warning(this, "警告", "未找到该设备");
        return;
    }

    // 先断开连接
    emit closeSerialRequest(deviceId);

    // 从设备管理器中移除
    deviceManager->removeDevice(deviceId);

    // 从该类型的列表里摘掉；若删的是活动设备，退回到该类型剩下的第一台
    m_devicesByType[typeToRemove].removeAll(deviceId);
    if (devicetypeIdmap.value(typeToRemove, -1) == deviceId) {
        const QList<int> &ids = m_devicesByType[typeToRemove];
        if (ids.isEmpty()) devicetypeIdmap.remove(typeToRemove);
        else devicetypeIdmap[typeToRemove] = ids.first();
    }

    // 更新 UI（按剩下的活动设备重算）
    refreshActiveDevice(typeToRemove);
    updateDeviceListCombox();

    QMessageBox::information(this, "成功", "设备已删除");

}

// 切换设备下拉框：把选中设备设为它所属类型的活动设备
void MainWindow::onDeviceComboBoxChanged(int index)
{
    if (!deviceManager || index < 0) return;
    int deviceId = ui->deviceListCombox->itemData(index).toInt();
    DeviceType type = deviceManager->getDeviceType(deviceId);
    if (type == DEVICE_TYPE_UNKNOWN) return;
    devicetypeIdmap[type] = deviceId;
    refreshActiveDevice(type);
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
        // 用统一组包入口重建完整帧，校验和正确计算（不再填 0x00）
        QByteArray frame = CommandProcessor::createCommand(cmdType, deviceType, data);
        if (!frame.isEmpty()) {
            m_gateway->forwardDeviceData(frame);
        }
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
    UiTheme::colorizeStatus(ui->voiceStatusLabel);
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
    UiTheme::colorizeStatus(ui->voiceStatusLabel);
}

// 语音错误显示
void MainWindow::onVoiceError(const QString &err)
{
    ui->voiceResultEdit->append(QString("错误：%1").arg(err));
    ui->voiceStatusLabel->setText("语音状态：出错");
    ui->voiceToggleButton->setChecked(false);
    UiTheme::colorizeStatus(ui->voiceStatusLabel);
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
    UiTheme::colorizeStatus(ui->motorStatusLabel);
    UiTheme::colorizeStatus(ui->servoStatusLabel);
    UiTheme::colorizeStatus(ui->sensorStatusLabel);
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
    return deviceType;
}
// 设备类型转显示名
QString MainWindow::getDeviceNameByType(DeviceType type)
{
    switch (type) {
    case DEVICE_TYPE_MOTOR:           return "电机";
    case DEVICE_TYPE_SERVO:           return "舵机";
    case DEVICE_TYPE_HUMIDITY_SENSOR: return "传感器";
    default:                          return "设备";
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
// 刷新设备下拉框（列出所有已添加设备，支持同类型多实例）
void MainWindow::updateDeviceListCombox()
{
    QSignalBlocker blocker(ui->deviceListCombox);   // 重建列表时不触发 currentIndexChanged
    ui->deviceListCombox->clear();

    for (auto it = m_devicesByType.constBegin(); it != m_devicesByType.constEnd(); ++it) {
        DeviceType type = it.key();
        for (int deviceId : it.value()) {
            QString port = deviceManager->getDevicePortName(deviceId);
            QString deviceName = getDeviceNameByType(type);
            bool connected = deviceManager->isDeviceConnected(deviceId);
            QString itemText = QString("%1 - %2 (ID:%3)%4")
                                   .arg(port, deviceName, QString::number(deviceId),
                                        connected ? " [已连接]" : "");
            ui->deviceListCombox->addItem(itemText, deviceId);
        }
    }
}


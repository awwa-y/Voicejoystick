#include "uitheme.h"
#include "ui_mainwindow.h"
#include "joystickmanager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>

namespace UiTheme {

// 按文字给状态标签着色(已连接/监听=绿,未连接/出错=橙,其余=灰)
void colorizeStatus(QLabel *label)
{
    const QString text = label->text();
    if (text.startsWith("已连接") || text.contains("正在监听"))
        label->setStyleSheet("color:#4caf50;font-weight:bold;background:transparent;");
    else if (text.contains("未连接") || text.contains("出错"))
        label->setStyleSheet("color:#f4511e;font-weight:bold;background:transparent;");
    else
        label->setStyleSheet("color:#9a9a9a;background:transparent;");
}

// 全局深色主题:统一背景/字体/控件样式,只动样式不动布局
void applyDarkTheme(QWidget *window)
{
    window->setStyleSheet(R"(
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
QFrame *makeCard(const QString &title, QLayout *inner)
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
void buildModernLayout(Ui::MainWindow *ui, QWidget *window, GatewayWidgets &gw)
{
    window->setWindowTitle("多设备串口控制上位机");
    window->resize(1180, 760);

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
    QObject::connect(ui->widget, &JoystickManager::joystickChanged, window,
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
    gw.edit = new QLineEdit("192.168.1.100:9000");
    gw.button = new QPushButton("连接网关");
    connGrid->addWidget(gatewayLabel, 4, 0);
    connGrid->addWidget(gw.edit, 4, 1);
    connGrid->addWidget(gw.button, 5, 0, 1, 2);
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

} // namespace UiTheme

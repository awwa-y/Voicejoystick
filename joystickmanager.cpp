#include "joystickmanager.h"

// 构造：固定圆心，旋钮初始在中心
JoystickManager::JoystickManager(QWidget *parent) :
    QWidget(parent)
    , m_radius(120)
    , m_knobRadius(45)
    , m_isDragging(false)
    , m_xValue(0)
    , m_yValue(0)
    , m_aButton(false)
    , m_bButton(false)
    ,m_initialized(false)
    ,m_center(QPoint(165,165))// 固定圆心
    ,currentpox(0)
    ,currentpoy(0)
{
    m_currentPos = m_center;

    update();
}


JoystickManager::~JoystickManager(){


}

// 绘制：平面工业仪表风——纯色底盘、刻度环、十字准线、平面旋钮(无渐变/投影/高光)
// 每次重绘都按当前控件尺寸重新算中心，窗口缩放时摇杆跟着居中
void JoystickManager::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int centerX = width() / 2;
    int centerY = height() / 2;

    m_center = QPoint(centerX, centerY);

    // 底盘:纯色面板 + 1px 边框
    painter.setPen(QPen(QColor(0x3a, 0x3a, 0x3a), 1));
    painter.setBrush(QBrush(QColor(0x24, 0x24, 0x24)));
    painter.drawEllipse(m_center, m_radius, m_radius);

    // 外圈刻度:8个方向,上下左右主方向加长(仪表刻度感)
    painter.setPen(QPen(QColor(0x70, 0x70, 0x70), 1));
    for (int i = 0; i < 8; ++i) {
        double a = i * M_PI / 4;
        int len = (i % 2 == 0) ? 10 : 6;
        int r1 = m_radius + 4;
        int r2 = r1 + len;
        painter.drawLine(
            QPoint(centerX + int(r1 * std::cos(a)), centerY + int(r1 * std::sin(a))),
            QPoint(centerX + int(r2 * std::cos(a)), centerY + int(r2 * std::sin(a))));
    }

    // 内部虚线参考圆(半径一半处)
    QPen dash(QColor(0x44, 0x44, 0x44), 1, Qt::DashLine);
    painter.setPen(dash);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_center, m_radius / 2, m_radius / 2);

    // 十字准线(灰,与边缘留 gap)
    painter.setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
    int gap = 14;
    painter.drawLine(centerX - m_radius + gap, centerY, centerX - gap, centerY);
    painter.drawLine(centerX + gap, centerY, centerX + m_radius - gap, centerY);
    painter.drawLine(centerX, centerY - m_radius + gap, centerX, centerY - gap);
    painter.drawLine(centerX, centerY + gap, centerX, centerY + m_radius - gap);

    // 边界检测：旋钮离中心超过半径时，沿角度方向拉回圆周上(四舍五入,拉满正好±100)
    int dx = m_currentPos.x() - centerX;
    int dy = m_currentPos.y() - centerY;
    double distance = std::sqrt(dx * dx + dy * dy);
    if (distance > m_radius) {
        double angle = std::atan2(dy, dx);
        m_currentPos.setX(centerX + qRound(m_radius * std::cos(angle)));
        m_currentPos.setY(centerY + qRound(m_radius * std::sin(angle)));
    }

    // 中心到旋钮的指示线(细绿线,偏离中心时才画)
    if (distance > m_knobRadius) {
        painter.setPen(QPen(QColor(0x3d, 0x8b, 0x40), 2));
        painter.drawLine(m_center, m_currentPos);
    }

    // 中心原点
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x3d, 0x8b, 0x40));
    painter.drawEllipse(m_center, 3, 3);

    // 旋钮:平面圆,深灰填充+灰边(工业风,无渐变无投影)
    painter.setBrush(QBrush(QColor(0x3a, 0x3a, 0x3a)));
    painter.setPen(QPen(QColor(0x80, 0x80, 0x80), 1));
    painter.drawEllipse(m_currentPos, m_knobRadius, m_knobRadius);

    // 旋钮内环(细一圈的同心圆,仪表层次)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0x60, 0x60, 0x60), 1));
    painter.drawEllipse(m_currentPos, m_knobRadius - 8, m_knobRadius - 8);
}

// 鼠标按下：点到旋钮附近（半径+旋钮半径）才开始拖拽，
// 点外面不响应，避免误触发
void JoystickManager::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 使用控件中心
        int centerX = width() / 2;
        int centerY = height() / 2;

        int dx = event->pos().x() - centerX;
        int dy = event->pos().y() - centerY;
        double distance = std::sqrt(dx*dx + dy*dy);

        if (distance <= m_radius + m_knobRadius) {
            m_isDragging = true;
            updateJoystickPosition(event->pos());
        }
    }
}
// 外部按百分比设置位置：把 -100..100 换算成像素坐标，
// 走 updateJoystickPosition（边界钳制、信号发射都复用）
void JoystickManager::setPosition(int x, int y) {
    int centerX = width() / 2;          // 实时获取摇杆中心X坐标
    int centerY = height() / 2;         // 实时获取摇杆中心Y坐标

    // 将百分比值 (-100..100) 转换成像素坐标(Y 与数学惯例一致:上=正)
    int pixelX = centerX + (x * m_radius / 100);
    int pixelY = centerY - (y * m_radius / 100);

    // 复用现有的更新逻辑（边界检测、更新 m_xValue/m_yValue、重绘、发射 joystickChanged 信号）
    updateJoystickPosition(QPoint(pixelX, pixelY));
}

void JoystickManager::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        updateJoystickPosition(event->pos());
    }

}

// 松开：旋钮弹回中心，xy 归零。只重绘不发数据
// （归零动作由下一次真正拖动或其他通道触发时再上报）
void JoystickManager::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_isDragging) {
        m_isDragging = false;
        int centerX = width() / 2;
        int centerY = height() / 2;
        m_center = QPoint(centerX, centerY);
        m_currentPos = m_center;
        m_xValue = 0;
        m_yValue = 0;
        update();
        // 如果需要在归位时发送数据，可添加 emit 或调用 sendJoystickData
        sendJoystickData();
    }
}

void JoystickManager::keyPressEvent(QKeyEvent *event)
{

}

void JoystickManager::keyReleaseEvent(QKeyEvent *event)
{

}

// 位置更新核心：
// 1. 超出半径的按角度钳制回圆周
// 2. 像素偏移换算成 -100..100 百分比
// 3. 重绘并发信号
void JoystickManager::updateJoystickPosition(const QPoint &pos)
{
    // 使用固定的中心点，与构造函数中的初始化一致
    int centerX = m_center.x();
    int centerY = m_center.y();

    int dx = pos.x() - centerX;
    int dy = pos.y() - centerY;
    double distance = std::sqrt(dx * dx + dy * dy);

    if (!m_initialized) {
        m_currentPos = m_center;
        m_initialized = true;
    }
    if (distance > m_radius) {
        double angle = std::atan2(dy, dx);
        m_currentPos.setX(centerX + qRound(m_radius * std::cos(angle)));
        m_currentPos.setY(centerY + qRound(m_radius * std::sin(angle)));
    } else {
        m_currentPos = pos;
    }

    // 四舍五入取整,拉到边界正好 ±100(截断会变成 99)
    // Y 取反:屏幕坐标向下为正,对外统一为数学惯例(上=正,下=负)
    m_xValue = qRound((m_currentPos.x() - centerX) * 100.0 / m_radius);
    m_yValue = qRound((centerY - m_currentPos.y()) * 100.0 / m_radius);


    m_xValue = qBound(-100, m_xValue, 100);
    m_yValue = qBound(-100, m_yValue, 100);


    update();
    sendJoystickData();

}



// 发送当前位置
void JoystickManager::sendJoystickData()
{
    emit joystickChanged(m_xValue,m_yValue);
}

// 语音通道的位置设置（同 setPosition,Y 上=正）
void JoystickManager::setPositionvoice(int x, int y) {
    int centerX = width() / 2;
    int centerY = height() / 2;
    int pixelX = centerX + (x * m_radius / 100);
    int pixelY = centerY - (y * m_radius / 100);
    updateJoystickPosition(QPoint(pixelX, pixelY));
}
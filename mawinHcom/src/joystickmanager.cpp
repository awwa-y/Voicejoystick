#include "joystickmanager.h"

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

void JoystickManager::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // // 计算左侧区域的中心位置
    int centerX = width() / 2;
    int centerY = height() / 2;

    m_center = QPoint(centerX, centerY);

    QPen pen(QColor(100, 100, 100));
    pen.setWidth(2);
    painter.setPen(pen);

    QBrush brush(QColor(240, 240, 240));
    painter.setBrush(brush);

    // 绘制摇杆背景
    painter.drawEllipse(m_center, m_radius, m_radius);

    // 绘制中心十字线
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawLine(centerX - m_radius, centerY, centerX + m_radius, centerY);
    painter.drawLine(centerX, centerY - m_radius, centerX, centerY + m_radius);

    // 【算法实现部分】：确保旋钮位置在有效范围内（边界检测算法）
    int dx = m_currentPos.x() - centerX;
    int dy = m_currentPos.y() - centerY;
    double distance = std::sqrt(dx * dx + dy * dy);
    if (distance > m_radius) {
        double angle = std::atan2(dy, dx);
        m_currentPos.setX(centerX + static_cast<int>(m_radius * std::cos(angle)));
        m_currentPos.setY(centerY + static_cast<int>(m_radius * std::sin(angle)));
    }

    // 绘制摇杆旋钮
    QLinearGradient gradient(m_currentPos, m_currentPos + QPoint(m_knobRadius, m_knobRadius));
    gradient.setColorAt(0, QColor(70, 130, 180));
    gradient.setColorAt(1, QColor(50, 100, 150));

    painter.setBrush(QBrush(gradient));
    painter.setPen(QPen(QColor(30, 80, 130), 1));
    painter.drawEllipse(m_currentPos, m_knobRadius, m_knobRadius);

    // 绘制从中心到旋钮的线
    //painter.setPen(QPen(QColor(0, 0, 0), 1));
    //painter.drawLine(m_center, m_currentPos);
}

//重写事件
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
void JoystickManager::setPosition(int x, int y) {
    int centerX = width() / 2;          // 实时获取摇杆中心X坐标
    int centerY = height() / 2;         // 实时获取摇杆中心Y坐标

    // 将百分比值 (-100..100) 转换成像素坐标
    int pixelX = centerX + (x * m_radius / 100);
    int pixelY = centerY + (y * m_radius / 100);

    // 复用现有的更新逻辑（边界检测、更新 m_xValue/m_yValue、重绘、发射 joystickChanged 信号）
    updateJoystickPosition(QPoint(pixelX, pixelY));
}

void JoystickManager::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        updateJoystickPosition(event->pos());
    }

}

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
    }
}

void JoystickManager::keyPressEvent(QKeyEvent *event)
{

}

void JoystickManager::keyReleaseEvent(QKeyEvent *event)
{

}

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
        m_currentPos.setX(centerX + static_cast<int>(m_radius * std::cos(angle)));
        m_currentPos.setY(centerY + static_cast<int>(m_radius * std::sin(angle)));
    } else {
        m_currentPos = pos;
    }

    m_xValue = static_cast<int>((m_currentPos.x() - centerX) * 100.0 / m_radius);
    m_yValue = static_cast<int>((m_currentPos.y() - centerY) * 100.0 / m_radius);


    m_xValue = qBound(-100, m_xValue, 100);
    m_yValue = qBound(-100, m_yValue, 100);


    update();
    sendJoystickData();

}



void JoystickManager::sendJoystickData()
{
    emit joystickChanged(m_xValue,m_yValue);
}
void JoystickManager::setPositionvoice(int x, int y) {
    int centerX = width() / 2;
    int centerY = height() / 2;
    int pixelX = centerX + (x * m_radius / 100);
    int pixelY = centerY + (y * m_radius / 100);
    updateJoystickPosition(QPoint(pixelX, pixelY));
}
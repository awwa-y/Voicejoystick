#ifndef JOYSTICKMANAGER_H
#define JOYSTICKMANAGER_H
#include <QWidget>
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>

// 自绘摇杆控件：鼠标拖拽旋钮，输出 -100..100 的 x/y 偏移
// 拖动过程中持续发 joystickChanged 信号，上层拿去组控制帧发串口
class JoystickManager:public QWidget
{
    Q_OBJECT
public:
    JoystickManager();

public:
    explicit JoystickManager(QWidget *parent = nullptr);
    ~JoystickManager();
    void resetPosition();              // 旋钮归中

    void setPosition(int x, int y);    // 外部按百分比(-100..100)设置旋钮位置
    void setPositionvoice(int x, int y);  // 语音通道的位置设置（行为同上）
signals:
    void joystickChanged(int x, int y);   // 位置变化，x/y 都是 -100..100


protected:
    void paintEvent(QPaintEvent *event) override;         // 画底盘、十字线、旋钮
    void mousePressEvent(QMouseEvent *event) override;    // 按下：判断是否点中旋钮区域，开始拖拽
    void mouseMoveEvent(QMouseEvent *event) override;     // 拖动中：更新位置
    void mouseReleaseEvent(QMouseEvent *event) override;  // 松开：旋钮弹回中心
    void keyPressEvent(QKeyEvent *event) override;        // 键盘控制（未实现）
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void updateJoystickPosition(const QPoint &pos);  // 位置更新核心：边界钳制 + 换算百分比 + 重绘 + 发信号
    void sendJoystickData();                         // 发 joystickChanged
private:
    QPoint m_center;        // 摇杆中心（控件几何中心）
    QPoint m_currentPos;    // 旋钮当前位置
    int m_radius;           // 底盘半径
    int m_knobRadius;       // 旋钮半径
    bool m_isDragging;      // 是否正在拖拽
    int m_xValue;           // 当前 x 百分比 -100..100
    int m_yValue;           // 当前 y 百分比 -100..100
    bool m_aButton;
    bool m_bButton;
    bool m_initialized;     // 首次更新标志
    int currentpox;
    int currentpoy;
};

#endif

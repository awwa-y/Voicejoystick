#ifndef JOYSTICKMANAGER_H
#define JOYSTICKMANAGER_H
#include <QWidget>
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
class JoystickManager:public QWidget
{
public:
    JoystickManager();
    Q_OBJECT
public:
    explicit JoystickManager(QWidget *parent = nullptr);
    ~JoystickManager();
    void resetPosition(); // 可选

    void setPosition(int x, int y);
    void setPositionvoice(int x, int y);
signals:
    void joystickChanged(int x, int y);


protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void updateJoystickPosition(const QPoint &pos);
    void sendJoystickData();
private:
    QPoint m_center;
    QPoint m_currentPos;
    int m_radius;//半径
    int m_knobRadius;
    bool m_isDragging;
    int m_xValue;//传参
    int m_yValue;//传递参数
    bool m_aButton;
    bool m_bButton;
    bool m_initialized;
    int currentpox;
    int currentpoy;




};

#endif

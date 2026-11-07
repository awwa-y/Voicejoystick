#ifndef UITHEME_H
#define UITHEME_H

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>

class QLayout;

namespace Ui { class MainWindow; }

// 纯 UI 工具层：深色主题 + 卡片式布局。
// 把样式与布局从 MainWindow 里抽出来，让视图层不再混入业务逻辑（单一职责）。
namespace UiTheme {

// 按文字给状态标签着色（已连接/监听=绿，未连接/出错=橙，其余=灰）
void colorizeStatus(QLabel *label);

// 全局深色主题（QSS 统一背景/字体/控件样式，只动样式不动布局）
void applyDarkTheme(QWidget *window);

// 卡片容器：深色面板 + 小标题 + 内容布局
QFrame *makeCard(const QString &title, QLayout *inner);

// 布局重排时动态创建的网关控件（地址输入框 + 连接按钮），回传给 MainWindow 保存
struct GatewayWidgets {
    QLineEdit   *edit   = nullptr;
    QPushButton *button = nullptr;
};

// 卡片式布局重排：废弃 Designer 的绝对定位，把控件 reparent 进分组卡片；
// 控件名与信号槽连接全部不变，业务代码零改动
void buildModernLayout(Ui::MainWindow *ui, QWidget *window, GatewayWidgets &gw);

} // namespace UiTheme

#endif // UITHEME_H

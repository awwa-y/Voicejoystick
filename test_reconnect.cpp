#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "reconnectpolicy.h"
#include "serialwork.h"

// 断线重连的单元测试。没有硬件也能跑：
//   1) 退避序列纯逻辑，直接比对毫秒数
//   2) "首开失败不自动重连"这条契约用真实 QSerialPort 打开一个不存在的端口验证
//      （拔线产生的 ResourceError 没法在无硬件环境下复现，那部分靠实机验证）

static int g_failed = 0;

static void check(bool cond, const QString &what)
{
    if (cond) {
        qDebug().noquote() << "PASS:" << what;
    } else {
        qDebug().noquote() << "FAIL:" << what;
        ++g_failed;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // ---- 用例1：退避序列 200/400/800/1600/3200/5000 封顶 ----
    const int expected[] = {200, 400, 800, 1600, 3200, 5000, 5000, 5000};
    for (int attempt = 1; attempt <= 8; ++attempt) {
        check(ReconnectPolicy::delayForAttempt(attempt) == expected[attempt - 1],
              QString("第%1次重连等待 %2ms").arg(attempt).arg(expected[attempt - 1]));
    }
    check(ReconnectPolicy::delayForAttempt(0) == 0, "第0次（立即）等待 0ms");

    // ---- 用例2：有状态版本 + reset 后从头开始 ----
    ReconnectPolicy policy;
    check(policy.attempts() == 0, "新策略计数为 0");
    check(policy.nextDelayMs() == 200, "首次等待 200ms");
    check(policy.nextDelayMs() == 400, "第二次等待 400ms");
    check(policy.attempts() == 2, "计数递增到 2");
    for (int i = 2; i < 6; ++i) policy.nextDelayMs();   // 推到封顶
    check(policy.nextDelayMs() == 5000, "封顶后保持 5000ms");
    policy.reset();
    check(policy.attempts() == 0 && policy.nextDelayMs() == 200, "reset 后退避回到 200ms");

    // ---- 用例3：连接过才重连——首次打开失败不应该进入重连循环 ----
    SerialWork sw(42);
    int errorCount = 0;
    int reconnectCount = 0;
    int openedCount = 0;
    QObject::connect(&sw, &SerialWork::errorOccurred,
                     [&](int, const QString &) { ++errorCount; });
    QObject::connect(&sw, &SerialWork::reconnecting,
                     [&](int, int, int) { ++reconnectCount; });
    QObject::connect(&sw, &SerialWork::serialOpened,
                     [&](int) { ++openedCount; });

    sw.setAutoReconnect(true);
    sw.openSerial("COM_NOT_EXIST_9X", 115200, 42);   // 必然打不开

    // 跑 800ms：足够跨越退避的前两次（200/400），如果错误地重连就会被抓到
    QTimer::singleShot(800, &app, &QCoreApplication::quit);
    app.exec();

    check(errorCount >= 1, "打开失败会上报错误");
    check(openedCount == 0, "不存在的端口不会产生打开成功");
    check(reconnectCount == 0, "首次打开失败不进入自动重连（避免选错口无限重试）");

    // ---- 用例4：重连期间 sendData 静默丢弃，不刷错误 ----
    const int before = errorCount;
    sw.sendData(42, QByteArray::fromHex("AA0402015A0855"));
    check(errorCount == before, "保持连接意图时 sendData 静默丢弃（不产生错误日志）");

    // ---- 用例5：用户主动断开后，再发数据要明确报错 ----
    sw.closeSerial(42);   // 取消"保持连接"意图
    sw.sendData(42, QByteArray::fromHex("AA0402015A0855"));
    check(errorCount == before + 1, "用户断开后 sendData 明确上报错误");

    if (g_failed == 0) {
        qDebug() << "All reconnect tests passed!";
        return 0;
    }
    qDebug() << g_failed << "check(s) failed";
    return 1;
}

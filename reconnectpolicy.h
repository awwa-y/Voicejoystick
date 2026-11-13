#ifndef RECONNECTPOLICY_H
#define RECONNECTPOLICY_H

#include <QtGlobal>

// 重连退避策略：只算"该等多久"，不碰串口、不碰定时器。
// 抽出来是为了能单测——IO 出错路径在没硬件时复现不了，
// 但退避序列可以纯逻辑验证，避免"重连风暴"或"永远在等"这类问题。
//
// 序列：200ms 起，每次翻倍，封顶 5s
//   第 1 次 200 / 第 2 次 400 / 第 3 次 800 / 第 4 次 1600 / 第 5 次 3200 / 之后 5000
class ReconnectPolicy
{
public:
    static constexpr int kFirstDelayMs = 200;
    static constexpr int kMaxDelayMs   = 5000;
    static constexpr int kBackoffSteps = 6;   // 前 5 次翻倍（最高 200<<4 = 3200），第 6 次起封顶 5000

    // 复位：连接成功或用户手动重连时调用，退避从头开始
    void reset() { m_attempt = 0; }

    // 当前已排定的重连次数
    int attempts() const { return m_attempt; }

    // 排定下一次重连，返回本次需要等待的毫秒数
    int nextDelayMs()
    {
        ++m_attempt;
        return delayForAttempt(m_attempt);
    }

    // 纯函数版本：第 attempt 次（从 1 开始）该等多久；<=0 视为立即
    static int delayForAttempt(int attempt)
    {
        if (attempt <= 0) return 0;
        if (attempt >= kBackoffSteps) return kMaxDelayMs;
        return kFirstDelayMs << (attempt - 1);
    }

private:
    int m_attempt = 0;
};

#endif // RECONNECTPOLICY_H

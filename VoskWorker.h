#ifndef VOSKWORKER_H
#define VOSKWORKER_H

#include <QObject>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>

// 前向声明 Vosk 类型（避免在头文件中包含 vosk_api.h）
typedef struct VoskModel VoskModel;
typedef struct VoskRecognizer VoskRecognizer;

// 离线语音识别工作者：QAudioSource 采麦克风 -> Vosk 识别 -> 发指令信号
// 模型用轻量中文模型（约 40MB），语法约束限定四个指令词，减少误识别
class VoskWorker : public QObject
{
    Q_OBJECT
public:
    explicit VoskWorker(QObject *parent = nullptr);
    ~VoskWorker();

public slots:
    void startRecognition();   // 加载模型、创建识别器、开麦克风
    void stopRecognition();    // 停麦克风（模型留着，下次直接用）

signals:
    void commandRecognized(const QString &cmd);  // 识别出指令词
    void errorOccurred(const QString &error);
    void statusMessage(const QString &msg);
    void joystickMove(int x, int y);   // 直接请求摇杆移动到 (x, y)

private slots:
    void onAudioReadyRead();   // 麦克风有数据：喂给 Vosk，出结果就发信号

private:
    QAudioSource *m_audioSource = nullptr;     // 麦克风采集
    QIODevice *m_audioDevice = nullptr;        // 音频数据流
    VoskModel *m_model = nullptr;              // Vosk 模型
    VoskRecognizer *m_recognizer = nullptr;    // 识别器
    bool m_running = false;
};

#endif // VOSKWORKER_H
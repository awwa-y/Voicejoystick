#ifndef VOSKWORKER_H
#define VOSKWORKER_H

#include <QObject>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>

// 前向声明 Vosk 类型（避免在头文件中包含 vosk_api.h）
typedef struct VoskModel VoskModel;
typedef struct VoskRecognizer VoskRecognizer;

class VoskWorker : public QObject
{
    Q_OBJECT
public:
    explicit VoskWorker(QObject *parent = nullptr);
    ~VoskWorker();

public slots:
    void startRecognition();   // 开始识别
    void stopRecognition();    // 停止识别

signals:
    void commandRecognized(const QString &cmd);
    void errorOccurred(const QString &error);
    void statusMessage(const QString &msg);
    void joystickMove(int x, int y);   // 直接请求摇杆移动到 (x, y)

private slots:
    void onAudioReadyRead();

private:
    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioDevice = nullptr;
    VoskModel *m_model = nullptr;
    VoskRecognizer *m_recognizer = nullptr;
    bool m_running = false;
};

#endif // VOSKWORKER_H
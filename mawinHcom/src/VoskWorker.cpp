#include "VoskWorker.h"
#include "vosk_api.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QAudioDevice>

VoskWorker::VoskWorker(QObject *parent)
    : QObject(parent)
{
}

VoskWorker::~VoskWorker()
{
    stopRecognition();
    if (m_recognizer) {
        vosk_recognizer_free(m_recognizer);
        m_recognizer = nullptr;
    }
    if (m_model) {
        vosk_model_free(m_model);
        m_model = nullptr;
    }
}

void VoskWorker::startRecognition()
{
    if (m_running) {
        qDebug() << "识别已经在运行中";
        return;
    }

    // 1. 加载模型（请修改为你的实际路径）
    // 建议使用绝对路径或确保 exe 同级目录下有 model 文件夹
    QString modelPath = "model";
    m_model = vosk_model_new(modelPath.toUtf8().constData());
    if (!m_model) {
        emit errorOccurred("加载 Vosk 模型失败，路径：" + modelPath);
        return;
    }
    qDebug() << "Vosk 模型加载成功";

    // 2. 创建识别器（采样率 16000 Hz）
    m_recognizer = vosk_recognizer_new(m_model, 16000.0);
    if (!m_recognizer) {
        emit errorOccurred("创建 Vosk 识别器失败");
        vosk_model_free(m_model);
        m_model = nullptr;
        return;
    }

    // 3. 设置语法约束（四个指令）
    QString grammar = "[\"加速\", \"减速\", \"左转\", \"右转\"]";
    vosk_recognizer_set_grm(m_recognizer, grammar.toUtf8().constData());
    qDebug() << "语法约束已设置：" << grammar;

    // 4. 配置音频格式（16kHz, 16bit, 单声道）
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!format.isValid()) {
        emit errorOccurred("音频格式无效");
        return;
    }

    // 5. 获取默认音频输入设备（Qt6 方式）
    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        emit errorOccurred("没有找到麦克风设备");
        return;
    }
    qDebug() << "使用麦克风：" << inputDevice.description();

    // 6. 检查并调整音频格式（如果默认格式不被支持）
    if (!inputDevice.isFormatSupported(format)) {
        qWarning() << "默认格式不被支持，使用设备首选格式";
        format = inputDevice.preferredFormat();
    }

    // 7. 创建 QAudioSource 并启动
    m_audioSource = new QAudioSource(inputDevice, format, this);
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        emit errorOccurred("无法打开麦克风，请检查麦克风权限");
        delete m_audioSource;
        m_audioSource = nullptr;
        return;
    }

    // 8. 连接音频数据信号
    connect(m_audioDevice, &QIODevice::readyRead, this, &VoskWorker::onAudioReadyRead);
    m_running = true;
    emit statusMessage("语音识别已启动，请说话...");
    qDebug() << "Vosk 语音识别已启动";
}

void VoskWorker::stopRecognition()
{
    if (!m_running) return;
    m_running = false;

    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
    }
    m_audioDevice = nullptr;

    emit statusMessage("语音识别已停止");
    qDebug() << "Vosk 语音识别已停止";
}

void VoskWorker::onAudioReadyRead()
{
    if (!m_audioDevice || !m_recognizer) return;

    QByteArray data = m_audioDevice->readAll();
    if (data.isEmpty()) return;

    if (vosk_recognizer_accept_waveform(m_recognizer, data.data(), data.size())) {
        const char *jsonResult = vosk_recognizer_result(m_recognizer);
        QJsonDocument doc = QJsonDocument::fromJson(jsonResult);
        QString text = doc.object().value("text").toString();
        if (!text.isEmpty()) {
            qDebug() << "识别到指令:" << text;
            emit commandRecognized(text);   // 保留原有信号

            // 新增：根据指令直接发射摇杆位置
            if (text == "左转") {
                emit joystickMove(-100, 0);
            } else if (text == "右转") {
                emit joystickMove(100, 0);
            }
            // 加速/减速不涉及摇杆移动，所以不发射 joystickMove
        }
    }
}
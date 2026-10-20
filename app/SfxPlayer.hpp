#pragma once

#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QVector>

#include <array>
#include <memory>

class QAudioSink;

// One persistent audio stream, mixed in software.
//
// The previous design (a QSoundEffect per sound) restarted the platform
// audio output on every play(): measured on a physical device, every pellet
// eaten cost a 15-40 ms synchronous AudioTrack start AND a 40-60 ms hiccup
// in the frame pipeline (UI + render thread stalled together, verified with
// tick/render instrumentation and a mute build: 50 gaps per game with
// QSoundEffect vs ~5 with this mixer).
//
// So: one QAudioSink is opened once at startup and fed until the app exits.
// Sounds are pre-decoded to float PCM and mixed into that stream from the
// audio callback thread; triggering a sound from the game thread only
// publishes a voice under a mutex — microseconds, never blocking I/O.
class SfxMixer : public QIODevice {
public:
    explicit SfxMixer(QObject* parent = nullptr);

    // Decodes a 16-bit PCM WAV resource into mono float samples.
    bool loadWav(const QString& resource, QVector<float>* out, int* sampleRate);

    void setFormat(const QAudioFormat& format);
    bool isReady() const { return m_ready; }

    // Starts (or restarts) a voice on the next audio callback.
    void trigger(const QVector<float>& samples, int sampleRate);

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    struct Voice {
        const QVector<float>* samples = nullptr;
        double position = 0.0;
        double step = 1.0;
    };

    static constexpr int kVoiceCount = 4;

    QMutex m_mutex;  // guards m_voices
    std::array<Voice, kVoiceCount> m_voices{};
    QVector<float> m_scratch;  // audio-callback thread only
    QAudioFormat m_format;
    bool m_ready = false;
};

// Exposed to QML as the "sfx" context property; see SfxMixer for why this
// is not a QSoundEffect.
class SfxPlayer : public QObject {
    Q_OBJECT

public:
    explicit SfxPlayer(QObject* parent = nullptr);
    ~SfxPlayer() override;

    Q_INVOKABLE void playMunch();
    Q_INVOKABLE void playPower();

private:
    void trigger(const QVector<float>& samples, int sampleRate);

    SfxMixer m_mixer;
    std::unique_ptr<QAudioSink> m_sink;
    QVector<float> m_munch;
    QVector<float> m_power;
    int m_munchRate = 0;
    int m_powerRate = 0;
};

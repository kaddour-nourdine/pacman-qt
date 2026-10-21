#pragma once

#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QVector>

#include <array>
#include <atomic>
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
// Sounds are pre-decoded to float PCM and mixed into that stream on demand:
// triggering a sound from the game thread only publishes a voice under a
// mutex — microseconds, never blocking I/O.
class SfxMixer : public QIODevice {
public:
    explicit SfxMixer(QObject* parent = nullptr);

    // Decodes a 16-bit PCM WAV resource into mono float samples.
    bool loadWav(const QString& resource, QVector<float>* out, int* sampleRate);

    void setFormat(const QAudioFormat& format);
    bool isReady() const { return m_ready; }

    // Starts (or restarts) a voice on the next audio callback.
    void trigger(const QVector<float>& samples, int sampleRate);

    // Diagnostics. Written by the audio thread, read from the game thread:
    // they are the only way to tell "the mixer is running" from "the mixer
    // is running but the phone is producing no sound".
    int voicesStarted() const { return m_voicesStarted.load(std::memory_order_relaxed); }
    qint64 framesWritten() const { return m_framesWritten.load(std::memory_order_relaxed); }
    float takePeak() { return m_peak.exchange(0.0f, std::memory_order_relaxed); }
    float peak() const { return m_peak.load(std::memory_order_relaxed); }

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

    // How much audio this device can hand out right now.
    //
    // This override is what makes the mixer audible at all. Qt's audio
    // pipeline only pulls from a user QIODevice when it claims to have
    // data (pullFromQIODeviceToRingbuffer() bails out on
    // bytesAvailable() == 0), and a plain QIODevice reports 0 forever -
    // so the sink sat in IdleState and never read a single frame.
    // We are a generator: silence plus the queued voices are always ready.
    qint64 bytesAvailable() const override { return m_availableBytes; }

private:
    struct Voice {
        const QVector<float>* samples = nullptr;
        double position = 0.0;
        double step = 1.0;
    };

    static constexpr int kVoiceCount = 4;

    QMutex m_mutex;  // guards m_voices
    std::array<Voice, kVoiceCount> m_voices{};
    QVector<float> m_scratch;  // only touched by the sink's own thread
    QAudioFormat m_format;
    bool m_ready = false;
    qint64 m_availableBytes = 0;  // what bytesAvailable() reports

    std::atomic<int> m_voicesStarted{0};
    std::atomic<qint64> m_framesWritten{0};
    std::atomic<float> m_peak{0.0f};
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

    // Health of the audio path, for the tests and the on-device log:
    // voicesStarted() counts the sounds that were queued, framesWritten()
    // counts the frames that actually left the mixer, peak() is the loudest
    // sample seen since the last status line.
    bool hasSink() const { return m_sink != nullptr; }
    int voicesStarted() const { return m_mixer.voicesStarted(); }
    qint64 framesWritten() const { return m_mixer.framesWritten(); }
    float peakLevel() const { return m_mixer.peak(); }

private:
    void trigger(const QVector<float>& samples, int sampleRate);
    void logStatus(const char* reason);

    SfxMixer m_mixer;
    std::unique_ptr<QAudioSink> m_sink;
    QVector<float> m_munch;
    QVector<float> m_power;
    int m_munchRate = 0;
    int m_powerRate = 0;
    QAudioFormat m_format;
    QString m_deviceName;
    qint64 m_lastStatusMs = 0;
};

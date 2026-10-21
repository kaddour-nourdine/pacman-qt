#include "SfxPlayer.hpp"

#include <QDateTime>
#include <QDebug>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QFile>
#include <QMediaDevices>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr qreal kSfxGain = 0.6;  // was the SoundEffect volume
constexpr int kBufferMs = 50;    // modest: keeps effect latency low
constexpr int kChunkMs = 200;    // how much audio the mixer claims to have ready
constexpr qint64 kStatusPeriodMs = 10000;  // health line cadence while playing
}  // namespace

SfxMixer::SfxMixer(QObject* parent) : QIODevice(parent) {
    open(QIODevice::ReadOnly);
}

bool SfxMixer::loadWav(const QString& resource, QVector<float>* out, int* sampleRate) {
    QFile file(resource);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "pacman: cannot open sound" << resource;
        return false;
    }
    const QByteArray data = file.readAll();
    const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
    const auto size = static_cast<quint32>(data.size());
    if (size < 44 || qFromLittleEndian<quint32>(bytes) != 0x46464952u) {  // "RIFF"
        qWarning() << "pacman: not a RIFF file" << resource;
        return false;
    }

    quint32 formatTag = 0, channels = 0, rate = 0, bits = 0;
    const uchar* pcm = nullptr;
    quint32 pcmBytes = 0;

    quint32 pos = 12;
    while (pos + 8 <= size) {
        const quint32 id = qFromLittleEndian<quint32>(bytes + pos);
        const quint32 chunkSize = qFromLittleEndian<quint32>(bytes + pos + 4);
        const uchar* chunk = bytes + pos + 8;
        if (pos + 8 + chunkSize > size) {
            break;
        }
        if (id == 0x20746D66u) {  // "fmt "
            formatTag = qFromLittleEndian<quint16>(chunk);
            channels = qFromLittleEndian<quint16>(chunk + 2);
            rate = qFromLittleEndian<quint32>(chunk + 4);
            bits = qFromLittleEndian<quint16>(chunk + 14);
        } else if (id == 0x61746164u) {  // "data"
            pcm = chunk;
            pcmBytes = chunkSize;
        }
        pos += 8 + chunkSize + (chunkSize & 1u);
    }

    if (pcm == nullptr || formatTag != 1 || bits != 16 || channels == 0 || rate == 0) {
        qWarning() << "pacman: unsupported WAV format in" << resource;
        return false;
    }

    const quint32 frames = pcmBytes / (2 * channels);
    out->resize(static_cast<int>(frames));
    for (quint32 i = 0; i < frames; ++i) {
        // Mono mixdown (our assets are mono; sums extras if that ever changes).
        qint32 acc = 0;
        for (quint32 c = 0; c < channels; ++c) {
            const uchar* sample = pcm + (i * channels + c) * 2;
            acc += static_cast<qint16>(qFromLittleEndian<quint16>(sample));
        }
        const qreal mixed = static_cast<qreal>(acc) / static_cast<qreal>(channels);
        (*out)[static_cast<int>(i)] = static_cast<float>(mixed / 32768.0);
    }
    *sampleRate = static_cast<int>(rate);
    return !out->isEmpty();
}

void SfxMixer::setFormat(const QAudioFormat& format) {
    m_format = format;
    m_ready = format.isValid() && format.sampleRate() > 0 && format.channelCount() > 0 &&
              (format.sampleFormat() == QAudioFormat::Int16 ||
               format.sampleFormat() == QAudioFormat::Int32 ||
               format.sampleFormat() == QAudioFormat::Float ||
               format.sampleFormat() == QAudioFormat::UInt8);
    if (!m_ready) {
        qWarning() << "pacman: unusable audio format" << format;
        m_availableBytes = 0;
        return;
    }

    // Claim a generous chunk of audio. The sink asks for at most this much
    // per pull and the ring buffer caps it anyway; keeping it a whole
    // number of frames means no partial frame is ever dropped.
    const qint64 frameBytes = format.bytesPerFrame();
    const qint64 chunk = format.bytesForDuration(kChunkMs * 1000);
    m_availableBytes = frameBytes > 0 ? (chunk / frameBytes) * frameBytes : 0;
}

void SfxMixer::trigger(const QVector<float>& samples, int sampleRate) {
    if (!m_ready || samples.isEmpty() || m_format.sampleRate() <= 0) {
        return;
    }
    QMutexLocker lock(&m_mutex);

    // Reuse an idle voice, else steal the one furthest along (oldest).
    Voice* chosen = nullptr;
    double bestPosition = -1.0;
    for (Voice& voice : m_voices) {
        if (voice.samples == nullptr) {
            chosen = &voice;
            break;
        }
        const double progress = voice.samples->isEmpty() ? 0.0
                                                        : voice.position / voice.samples->size();
        if (progress > bestPosition) {
            bestPosition = progress;
            chosen = &voice;
        }
    }
    if (chosen == nullptr) {
        return;
    }
    chosen->samples = &samples;
    chosen->position = 0.0;
    chosen->step = static_cast<double>(sampleRate) / static_cast<double>(m_format.sampleRate());
    m_voicesStarted.fetch_add(1, std::memory_order_relaxed);
}

qint64 SfxMixer::readData(char* data, qint64 maxSize) {
    const int frameBytes = m_format.bytesPerFrame();
    if (frameBytes <= 0 || !m_ready) {
        return 0;
    }
    const int frames = static_cast<int>(maxSize / frameBytes);
    if (frames <= 0) {
        return 0;
    }
    if (m_scratch.size() < frames) {
        m_scratch.resize(frames);  // only the sink's thread calls readData()
    }
    std::fill(m_scratch.begin(), m_scratch.begin() + frames, 0.0f);

    {
        QMutexLocker lock(&m_mutex);
        for (Voice& voice : m_voices) {
            if (voice.samples == nullptr) {
                continue;
            }
            const QVector<float>& clip = *voice.samples;
            const int clipFrames = clip.size();
            double position = voice.position;
            bool done = false;
            for (int i = 0; i < frames; ++i) {
                if (position >= clipFrames - 1) {
                    done = true;
                    break;
                }
                const int index = static_cast<int>(position);
                const float frac = static_cast<float>(position - index);
                const float interpolated = clip[index] + (clip[index + 1] - clip[index]) * frac;
                m_scratch[i] += interpolated * static_cast<float>(kSfxGain);
                position += voice.step;
            }
            if (done) {
                voice.samples = nullptr;
            } else {
                voice.position = position;
            }
        }
    }

    // Two overlapping sounds (a pellet eaten while the previous munch is
    // still ringing) can exceed full scale. Clamp once here: without it
    // the sum would distort in Float and wrap around in Int16.
    for (int i = 0; i < frames; ++i) {
        m_scratch[i] = std::clamp(m_scratch[i], -1.0f, 1.0f);
    }

    // Diagnostics: how much audio left the mixer and how loud it was.
    float peak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        peak = std::max(peak, std::fabs(m_scratch[i]));
    }
    float current = m_peak.load(std::memory_order_relaxed);
    while (peak > current &&
           !m_peak.compare_exchange_weak(current, peak, std::memory_order_relaxed)) {
    }
    m_framesWritten.fetch_add(frames, std::memory_order_relaxed);

    auto* out = reinterpret_cast<uchar*>(data);
    const int channels = m_format.channelCount();
    switch (m_format.sampleFormat()) {
    case QAudioFormat::Float: {
        auto* dst = reinterpret_cast<float*>(out);
        for (int f = 0; f < frames; ++f) {
            const float value = m_scratch[f];
            for (int c = 0; c < channels; ++c) {
                *dst++ = value;
            }
        }
        break;
    }
    case QAudioFormat::Int16: {
        auto* dst = reinterpret_cast<qint16*>(out);
        for (int f = 0; f < frames; ++f) {
            const float value = std::clamp(m_scratch[f], -1.0f, 1.0f);
            const auto sample = static_cast<qint16>(value * 32767.0f);
            for (int c = 0; c < channels; ++c) {
                *dst++ = sample;
            }
        }
        break;
    }
    case QAudioFormat::Int32: {
        auto* dst = reinterpret_cast<qint32*>(out);
        for (int f = 0; f < frames; ++f) {
            const float value = std::clamp(m_scratch[f], -1.0f, 1.0f);
            const auto sample = static_cast<qint32>(value * 2147483647.0f);
            for (int c = 0; c < channels; ++c) {
                *dst++ = sample;
            }
        }
        break;
    }
    case QAudioFormat::UInt8: {
        for (int f = 0; f < frames; ++f) {
            const float value = std::clamp(m_scratch[f], -1.0f, 1.0f);
            const auto sample = static_cast<uchar>((value * 0.5f + 0.5f) * 255.0f);
            for (int c = 0; c < channels; ++c) {
                *out++ = sample;
            }
        }
        break;
    }
    default:
        std::memset(data, 0, static_cast<size_t>(frames) * frameBytes);
        break;
    }
    return frames * frameBytes;
}

qint64 SfxMixer::writeData(const char*, qint64) {
    return -1;
}

SfxPlayer::SfxPlayer(QObject* parent) : QObject(parent) {
    if (!m_mixer.loadWav(QStringLiteral(":/assets/audio/munch.wav"), &m_munch, &m_munchRate) ||
        !m_mixer.loadWav(QStringLiteral(":/assets/audio/power.wav"), &m_power, &m_powerRate) ||
        !m_mixer.loadWav(QStringLiteral(":/assets/audio/ghost_eaten.wav"), &m_ghostEaten,
                         &m_ghostEatenRate) ||
        !m_mixer.loadWav(QStringLiteral(":/assets/audio/death.wav"), &m_death, &m_deathRate)) {
        return;
    }

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        qWarning() << "pacman: no audio output device, sounds disabled";
        return;
    }

    m_deviceName = device.description();
    const QAudioFormat format = device.preferredFormat();
    m_format = format;
    m_mixer.setFormat(format);
    if (!m_mixer.isReady()) {
        return;
    }

    // The one and only stream: opened before the first sound and kept open
    // for the lifetime of the app. Silence is as cheap as any other buffer
    // to the audio server, and no start/stop ever perturbs the pipeline.
    m_sink = std::make_unique<QAudioSink>(device, format);
    m_sink->setBufferSize(format.bytesForDuration(kBufferMs * 1000));
    m_sink->start(&m_mixer);
    if (m_sink->state() == QAudio::StoppedState && m_sink->error() != QAudio::NoError) {
        qWarning() << "pacman: audio sink failed to start" << m_sink->error();
        m_sink.reset();
        return;
    }

    // The sink changing state is the one interesting failure mode (the
    // platform can drop the output device at any time); log it whenever it
    // is not the healthy state.
    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state != QAudio::ActiveState) {
            logStatus("sink-state");
        }
    });

    qInfo() << "pacman-audio: device" << m_deviceName << "rate" << m_format.sampleRate()
            << "channels" << m_format.channelCount() << "sampleFormat"
            << static_cast<int>(m_format.sampleFormat()) << "buffer"
            << m_sink->bufferSize() << "state" << static_cast<int>(m_sink->state());
}

SfxPlayer::~SfxPlayer() {
    if (m_sink) {
        m_sink->stop();
    }
}

void SfxPlayer::trigger(const QVector<float>& samples, int sampleRate) {
    if (!m_enabled) {
        return;
    }
    m_mixer.trigger(samples, sampleRate);

    // Health line while the game is making noise: at most one every 10 s.
    // It answers the question the logs cannot: is the mixer actually
    // producing sound, or is it just not complaining?
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastStatusMs >= kStatusPeriodMs) {
        m_lastStatusMs = now;
        logStatus("playing");
    }
}

void SfxPlayer::logStatus(const char* reason) {
    if (!m_sink) {
        qWarning() << "pacman-audio:" << reason << "no sink";
        return;
    }
    const int triggers = m_mixer.voicesStarted();
    const qint64 frames = m_mixer.framesWritten();
    const float peak = m_mixer.takePeak();
    const double seconds = m_format.sampleRate() > 0
                                ? static_cast<double>(frames) / static_cast<double>(m_format.sampleRate())
                                : 0.0;
    qInfo() << "pacman-audio:" << reason << "triggers" << triggers << "sink"
            << static_cast<int>(m_sink->state()) << "error" << static_cast<int>(m_sink->error())
            << "frames" << frames << "seconds" << seconds << "peak" << peak;
}

void SfxPlayer::setEnabled(bool enabled) {
    m_enabled = enabled;
}

void SfxPlayer::playMunch() {
    trigger(m_munch, m_munchRate);
}

void SfxPlayer::playPower() {
    trigger(m_power, m_powerRate);
}

void SfxPlayer::playGhostEaten() {
    trigger(m_ghostEaten, m_ghostEatenRate);
}

void SfxPlayer::playDeath() {
    trigger(m_death, m_deathRate);
}

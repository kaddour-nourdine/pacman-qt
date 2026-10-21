#pragma once

#include <QObject>
#include <QSettings>

// Persistent user settings.
//
// The core never touches the file system (that is a hard rule of this
// project), so everything that has to survive a restart lives here, in the
// view layer, backed by QSettings. The high score is stored by
// GameController for the same reason.
//
// The object is owned by GameController and reaches QML as `game.settings`,
// so the QML side never has to know where the values are stored.
class AppSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged)
    Q_PROPERTY(bool slowMode READ slowMode WRITE setSlowMode NOTIFY slowModeChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);

    bool soundEnabled() const { return soundEnabled_; }
    void setSoundEnabled(bool enabled);

    // Slower game: the same levels, but with more time to decide where to
    // go. Handy on a touch screen, where a swipe costs a second.
    bool slowMode() const { return slowMode_; }
    void setSlowMode(bool enabled);

    Q_INVOKABLE void toggleSound();
    Q_INVOKABLE void toggleSlowMode();

signals:
    void soundEnabledChanged();
    void slowModeChanged();

private:
    static constexpr const char* kSoundKey = "soundEnabled";
    static constexpr const char* kSlowKey = "slowMode";

    bool soundEnabled_ = true;
    bool slowMode_ = false;
};

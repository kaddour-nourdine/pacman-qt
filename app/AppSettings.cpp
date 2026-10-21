#include "AppSettings.hpp"

AppSettings::AppSettings(QObject* parent) : QObject(parent) {
    QSettings settings;
    soundEnabled_ = settings.value(QLatin1String(kSoundKey), true).toBool();
    slowMode_ = settings.value(QLatin1String(kSlowKey), false).toBool();
}

void AppSettings::setSoundEnabled(bool enabled) {
    if (soundEnabled_ == enabled) {
        return;
    }
    soundEnabled_ = enabled;
    QSettings().setValue(QLatin1String(kSoundKey), enabled);
    emit soundEnabledChanged();
}

void AppSettings::setSlowMode(bool enabled) {
    if (slowMode_ == enabled) {
        return;
    }
    slowMode_ = enabled;
    QSettings().setValue(QLatin1String(kSlowKey), enabled);
    emit slowModeChanged();
}

void AppSettings::toggleSound() {
    setSoundEnabled(!soundEnabled_);
}

void AppSettings::toggleSlowMode() {
    setSlowMode(!slowMode_);
}

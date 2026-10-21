#include "ScreenInsets.hpp"

#include <QWindow>

ScreenInsets::ScreenInsets(QObject* parent) : QObject(parent) {}

void ScreenInsets::attach(QWindow* window) {
    if (!window) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    // The platform reports the safe area; it changes when the system bars
    // are shown or hidden, so follow the notification.
    connect(window, &QWindow::safeAreaMarginsChanged, this,
            [this](const QMargins& margins) { setMargins(margins); });
    setMargins(window->safeAreaMargins());
#else
    Q_UNUSED(window)
#endif
}

void ScreenInsets::setMargins(const QMargins& margins) {
    if (m_margins == margins) {
        return;
    }
    m_margins = margins;
    emit changed();
}

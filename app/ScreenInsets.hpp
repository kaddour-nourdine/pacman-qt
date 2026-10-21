#pragma once

#include <QMargins>
#include <QObject>

class QWindow;

// The safe area of the screen: the part that is not covered by a notch, a
// rounded corner or a system bar.
//
// On Android the platform reports it (Qt 6.9 and newer); on the desktop it
// is always empty. The values are in *device independent pixels*, the same
// unit QML uses, so they can be added to anchors directly.
class ScreenInsets : public QObject {
    Q_OBJECT

    Q_PROPERTY(int left READ left NOTIFY changed)
    Q_PROPERTY(int top READ top NOTIFY changed)
    Q_PROPERTY(int right READ right NOTIFY changed)
    Q_PROPERTY(int bottom READ bottom NOTIFY changed)

public:
    explicit ScreenInsets(QObject* parent = nullptr);

    // Follows the safe area of the given window. Safe to call with nullptr
    // (the insets then stay empty).
    void attach(QWindow* window);

    int left() const { return m_margins.left(); }
    int top() const { return m_margins.top(); }
    int right() const { return m_margins.right(); }
    int bottom() const { return m_margins.bottom(); }

signals:
    void changed();

private:
    void setMargins(const QMargins& margins);

    QMargins m_margins;
};

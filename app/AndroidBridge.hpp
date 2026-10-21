#pragma once

#include <QObject>

// Bridge from the Android activity (Java) to the game.
//
// Android does not deliver the back button to the Qt window: on API 33+ it
// arrives as an OnBackInvokedCallback, and on older versions as a key event
// that Qt's own input delegate consumes. The activity therefore forwards the
// press to this object through JNI (see AndroidBridge.cpp), and the QML side
// turns it into the pause menu.
//
// On the desktop the object exists but never receives anything.
class AndroidBridge : public QObject {
    Q_OBJECT

public:
    explicit AndroidBridge(QObject* parent = nullptr);
    ~AndroidBridge() override;

    // The one instance the JNI entry point talks to. The app creates exactly
    // one; it is set in the constructor.
    static AndroidBridge* instance() { return s_instance; }

public slots:
    // Called from the JNI entry point (on the Qt thread).
    void handleBackPressed();

signals:
    void backPressed();

private:
    static AndroidBridge* s_instance;
};

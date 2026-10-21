#include "AndroidBridge.hpp"

#include <QMetaObject>

#if defined(Q_OS_ANDROID)
#include <jni.h>
#endif

AndroidBridge* AndroidBridge::s_instance = nullptr;

AndroidBridge::AndroidBridge(QObject* parent) : QObject(parent) {
    s_instance = this;
}

AndroidBridge::~AndroidBridge() {
    // The JNI entry point looks the bridge up through the static pointer, so
    // it must not keep pointing at a destroyed object.
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void AndroidBridge::handleBackPressed() {
    emit backPressed();
}

#if defined(Q_OS_ANDROID)
// The entry point of org.pacman.qt.MainActivity.nativeBackPressed().
//
// The call arrives on the Android UI thread, which is the Qt main thread,
// but it is queued anyway: the game state must only be touched from the
// thread that owns it.
extern "C" JNIEXPORT void JNICALL
Java_org_pacman_qt_MainActivity_nativeBackPressed(JNIEnv*, jobject) {
    AndroidBridge* bridge = AndroidBridge::instance();
    if (bridge == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(bridge, "handleBackPressed", Qt::QueuedConnection);
}
#endif

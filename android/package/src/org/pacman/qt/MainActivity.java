package org.pacman.qt;

import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * The activity that hosts the game.
 *
 * It is a plain QtActivity with three additions:
 *
 *  - the game is played in landscape (the maze is much wider than tall),
 *  - the window is immersive, so the system bars do not eat the board,
 *  - the back button is forwarded to the game instead of closing the app.
 *
 * The back button needs care: Android delivers it to the activity, and Qt's
 * own input delegate swallows it (it registers an OnBackInvokedCallback on
 * API 33+ and consumes the key event on older versions). So the press is
 * intercepted here and handed to the C++ side through JNI; the game turns it
 * into the pause menu.
 */
public class MainActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // Landscape, both ways round: the player can turn the phone the way
        // that suits them.
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);

        // A phone lying on the table should not dim the screen mid-game.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        applyImmersiveMode();
    }

    @Override
    protected void onResume()
    {
        // Qt registers its own back gesture callback in onResume(), so ours
        // has to be registered afterwards to win (the dispatcher keeps one
        // callback per priority).
        super.onResume();
        BackHandler.register(this);
        applyImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveMode();
        }
    }

    /**
     * The back button never leaves the game: it opens the pause menu.
     *
     * On API 33+ the gesture arrives through the OnBackInvokedCallback
     * (see BackHandler); the key event below covers the hardware button and
     * the older platforms, where the framework would otherwise close the
     * activity.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event)
    {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                nativeBackPressed();
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    /**
     * Hides the status and navigation bars; they come back on their own
     * after a swipe, which is what IMMERSIVE_STICKY is for.
     */
    private void applyImmersiveMode()
    {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    /**
     * Called from C++ (see app/AndroidBridge.cpp).
     */
    private native void nativeBackPressed();

    /**
     * The predictive back gesture (API 33+). Kept in its own class so that
     * the API 33 types are only loaded on devices that have them.
     */
    private static final class BackHandler
    {
        static void register(final MainActivity activity)
        {
            if (Build.VERSION.SDK_INT < 33) {
                return;
            }
            try {
                activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                    new android.window.OnBackInvokedCallback() {
                        @Override
                        public void onBackInvoked()
                        {
                            activity.nativeBackPressed();
                        }
                    });
            } catch (IllegalStateException e) {
                // The dispatcher is only available while the activity is
                // running; the key event path covers the rest.
                android.util.Log.w("pacman", "cannot register back callback: " + e.getMessage());
            }
        }
    }
}

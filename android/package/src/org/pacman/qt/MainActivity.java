package org.pacman.qt;

import android.os.Build;
import android.os.Bundle;
import android.util.Log;
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
 * The back button needs care, because Android can deliver one press twice.
 * From API 33 on, an app that opts into the predictive back gesture gets
 * the press as an OnBackInvokedCallback — and the same press still arrives
 * as a plain key event. Forwarding both would open the pause menu and close
 * it again in the same breath, so the game would look like nothing had
 * happened (verified on a Tecno A059, Android 16).
 *
 * So exactly one path forwards the press: the callback when it could be
 * registered (API 33+, and then the gesture is covered as well), the key
 * event otherwise. The key event is swallowed either way, because the game
 * handles the back button and the activity must not finish.
 */
public class MainActivity extends QtActivity
{
    private static final String TAG = "pacman";

    // True when the predictive-back callback is in place. Then the key event
    // must not forward the press a second time.
    private boolean mBackCallbackRegistered = false;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        // A phone lying on the table should not dim the screen mid-game.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        applyImmersiveMode();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        mBackCallbackRegistered = BackHandler.register(this);
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
     * On API 33+ the press arrives through the OnBackInvokedCallback (see
     * BackHandler) and is forwarded from there; the key event below covers
     * the older platforms, where the framework would otherwise close the
     * activity.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event)
    {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (!mBackCallbackRegistered && event.getAction() == KeyEvent.ACTION_DOWN) {
                forwardBackPress();
            }
            // Swallowed in every case: the game handles the back button.
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
     * Hands the press to the game (see app/AndroidBridge.cpp).
     */
    private void forwardBackPress()
    {
        Log.i(TAG, "back press -> the game");
        nativeBackPressed();
    }

    /**
     * Called from C++ (see app/AndroidBridge.cpp).
     */
    private native void nativeBackPressed();

    /**
     * The predictive back gesture (API 33+). Kept in its own class so that
     * the API 33 types are only loaded on devices that have them.
     *
     * Returns true when the callback is in place.
     */
    private static final class BackHandler
    {
        static boolean register(final MainActivity activity)
        {
            if (Build.VERSION.SDK_INT < 33) {
                return false;
            }
            try {
                activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                    new android.window.OnBackInvokedCallback() {
                        @Override
                        public void onBackInvoked()
                        {
                            activity.forwardBackPress();
                        }
                    });
                return true;
            } catch (IllegalStateException e) {
                // The dispatcher is only available while the activity is
                // running; the key event path covers the rest.
                Log.w(TAG, "cannot register back callback: " + e.getMessage());
                return false;
            }
        }
    }
}

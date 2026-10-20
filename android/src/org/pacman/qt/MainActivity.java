package org.pacman.qt;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Window;
import android.view.WindowManager;
import org.qtproject.qt.android.bindings.QtActivity;

public class MainActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        // Enable immersive mode (full-screen)
        getWindow().getDecorView().setSystemUiVisibility(
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            android.view.View.SYSTEM_UI_FLAG_FULLSCREEN |
            android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
        
        // Keep screen on
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        
        // Lock to landscape
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        // Handle back button - map to pause
        if (keyCode == KeyEvent.KEYCODE_BACK)
        {
            // Send pause event to Qt
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        // Handle touch input for swipe gestures
        switch (event.getAction() & MotionEvent.ACTION_MASK)
        {
            case MotionEvent.ACTION_DOWN:
                // Start of touch - could be tap to start/pause
                break;
                
            case MotionEvent.ACTION_UP:
                // End of touch - check if it was a tap
                float dx = event.getX() - event.getHistoricalX(0);
                float dy = event.getY() - event.getHistoricalY(0);
                
                // If minimal movement, treat as tap
                if (Math.abs(dx) < 50 && Math.abs(dy) < 50)
                {
                    // Tap to start/pause
                    return true;
                }
                break;
                
            case MotionEvent.ACTION_MOVE:
                // Swipe detection
                if (event.getHistorySize() > 0)
                {
                    float startX = event.getHistoricalX(0);
                    float startY = event.getHistoricalY(0);
                    float endX = event.getX();
                    float endY = event.getY();
                    
                    float deltaX = endX - startX;
                    float deltaY = endY - startY;
                    
                    // Detect swipe direction
                    if (Math.abs(deltaX) > Math.abs(deltaY))
                    {
                        // Horizontal swipe
                        if (deltaX > 50)
                        {
                            // Swipe right
                            return true;
                        }
                        else if (deltaX < -50)
                        {
                            // Swipe left
                            return true;
                        }
                    }
                    else
                    {
                        // Vertical swipe
                        if (deltaY > 50)
                        {
                            // Swipe down
                            return true;
                        }
                        else if (deltaY < -50)
                        {
                            // Swipe up
                            return true;
                        }
                    }
                }
                break;
        }
        
        return super.onTouchEvent(event);
    }
    
    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        
        // Re-enable immersive mode when focus returns
        if (hasFocus)
        {
            getWindow().getDecorView().setSystemUiVisibility(
                android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                android.view.View.SYSTEM_UI_FLAG_FULLSCREEN |
                android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            );
        }
    }
}

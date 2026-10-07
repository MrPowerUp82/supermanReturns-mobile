package org.supermanreturns.mobile;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

public final class DiagnosticsActivity extends Activity implements SurfaceHolder.Callback {
    private final HandlerThread renderThread=new HandlerThread("sr-vulkan");
    private Handler worker;
    private SurfaceView surface;
    private ControllerView controls;
    private TextView status;
    private volatile int generation;
    private boolean resumed,ready;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state); getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        renderThread.start(); worker=new Handler(renderThread.getLooper());
        FrameLayout frame=new FrameLayout(this); setContentView(frame);
        surface=new SurfaceView(this); surface.getHolder().addCallback(this); frame.addView(surface);
        controls=new ControllerView(this); frame.addView(controls);
        status=new TextView(this); status.setTextColor(0xfff4f5f7); status.setTextSize(13); status.setGravity(Gravity.CENTER);
        status.setBackgroundColor(0x99081526); status.setText("DIAGNÓSTICO VULKAN · não é gameplay\nInicializando…");
        FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(-1,-2,Gravity.TOP); p.topMargin=12; frame.addView(status,p);
    }
    private void stop() {
        generation++; worker.removeCallbacksAndMessages(null); controls.clear(); worker.post(NativeBridge::closeSurface);
    }
    private void start() {
        stop(); if(!resumed || !ready) return;
        final int token=generation;
        worker.post(() -> {
            if(token!=generation) return;
            String opened=NativeBridge.openSurface(surface.getHolder().getSurface());
            if(opened.startsWith("ERRO:")) { show(token,opened); return; }
            Runnable draw=new Runnable() {
                long last;
                @Override public void run() {
                    if(token!=generation) return;
                    long before=android.os.SystemClock.uptimeMillis(); String result=NativeBridge.draw();
                    if(before-last>200 || result.startsWith("ERRO:")) {last=before;show(token,result+"\n"+NativeBridge.inputState());}
                    if(!result.startsWith("ERRO:")) worker.postDelayed(this,Math.max(1,33-(android.os.SystemClock.uptimeMillis()-before)));
                }
            };
            draw.run();
        });
    }
    private void show(int token,String text) { runOnUiThread(() -> { if(token==generation && !isDestroyed())status.setText("DIAGNÓSTICO · não é gameplay\n"+text); }); }
    @Override protected void onResume() {super.onResume();resumed=true;start();}
    @Override protected void onPause() {resumed=false;stop();super.onPause();}
    @Override protected void onDestroy() {stop();renderThread.quitSafely();super.onDestroy();}
    @Override public void surfaceCreated(SurfaceHolder holder) {}
    @Override public void surfaceChanged(SurfaceHolder holder,int format,int w,int h) {ready=w>0&&h>0;start();}
    @Override public void surfaceDestroyed(SurfaceHolder holder) {ready=false;stop();}
    @Override public void onWindowFocusChanged(boolean focus) {super.onWindowFocusChanged(focus);if(!focus && controls!=null)controls.clear();}
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if(event.isFromSource(InputDevice.SOURCE_GAMEPAD) || event.isFromSource(InputDevice.SOURCE_JOYSTICK))
            if(controls.padKey(event))return true;
        return super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if(event.isFromSource(InputDevice.SOURCE_JOYSTICK) && event.getActionMasked()==MotionEvent.ACTION_MOVE)return controls.padMotion(event);
        return super.dispatchGenericMotionEvent(event);
    }
}

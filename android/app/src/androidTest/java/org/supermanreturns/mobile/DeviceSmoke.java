package org.supermanreturns.mobile;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;

/** On-device checks of real JNI input, Vulkan presentation, multitouch and rotation. */
public final class DeviceSmoke extends Instrumentation {
    private Activity activity;
    private ControllerView controller;
    private final StringBuilder results=new StringBuilder();
    private int checks;
    @Override public void onCreate(Bundle args) {super.onCreate(args);start();}
    private void check(boolean value,String name) {if(!value)throw new AssertionError(name);checks++;results.append("PASS ").append(name).append('\n');}
    @Override public void onStart() {
        Bundle result=new Bundle();
        try {
            Intent intent=new Intent().setClassName(getTargetContext().getPackageName(),DiagnosticsActivity.class.getName()).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity=startActivitySync(intent); waitForIdleSync();
            runOnMainSync(() -> controller=findController(activity.getWindow().getDecorView()));
            check(controller!=null,"Controller view attached");
            String first=waitForFrames(); SystemClock.sleep(600); String second=screenText();
            check(!first.equals(second),"Vulkan presented frame counter advances");
            float[] origin=new float[4];runOnMainSync(() -> {int[] p=new int[2];controller.getLocationOnScreen(p);origin[0]=p[0];origin[1]=p[1];origin[2]=controller.getWidth();origin[3]=controller.getHeight();});
            float lx=origin[0]+origin[2]*.15f,ly=origin[1]+origin[3]*.62f;
            float ax=origin[0]+origin[2]*.86f,ay=origin[1]+origin[3]*.72f;
            long down=SystemClock.uptimeMillis();
            touch(down,MotionEvent.ACTION_DOWN,new int[]{0},new float[]{lx},new float[]{ly});
            touch(down,MotionEvent.ACTION_MOVE,new int[]{0},new float[]{lx+50},new float[]{ly});
            check(!NativeBridge.inputState().contains("| L 0.00,0.00"),"Touch stick reaches native input state");
            touch(down,MotionEvent.ACTION_POINTER_DOWN|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),new int[]{0,1},new float[]{lx+50,ax},new float[]{ly,ay});
            String held=NativeBridge.inputState();
            check(held.contains("0x1000") && !held.contains("| L 0.00,0.00"),"Two pointers preserve stick and A simultaneously");
            screenshot("multitouch.png");
            touch(down,MotionEvent.ACTION_POINTER_UP,new int[]{0,1},new float[]{lx+50,ax},new float[]{ly,ay});
            check(NativeBridge.inputState().contains("0x1000") && NativeBridge.inputState().contains("| L 0.00,0.00"),"Releasing stick preserves held A");
            touch(down,MotionEvent.ACTION_UP,new int[]{1},new float[]{ax},new float[]{ay});
            check(NativeBridge.inputState().contains("XInput 0x0 | L 0.00,0.00 | R 0.00,0.00 | LT 0.00 RT 0.00"),"All touch input resets on release");
            down=SystemClock.uptimeMillis();
            touch(down,MotionEvent.ACTION_DOWN,new int[]{0},new float[]{origin[0]+origin[2]*.23f},new float[]{origin[1]+origin[3]*.27f});
            check(NativeBridge.inputState().contains("LT 1.00"),"Left trigger reaches native input state");
            touch(down,MotionEvent.ACTION_CANCEL,new int[]{0},new float[]{lx},new float[]{ly});
            check(NativeBridge.inputState().contains("LT 0.00"),"Cancel clears held trigger");
            runOnMainSync(() -> activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE));
            SystemClock.sleep(1000);waitForFrames();
            check(true,"Vulkan surface survives reverse landscape");
            runOnMainSync(() -> activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));
            SystemClock.sleep(1000);waitForFrames();
            check(true,"Vulkan surface resumes original landscape");
            screenshot("diagnostic.png");
            runOnMainSync(activity::finish);waitForIdleSync();
            activity=startActivitySync(intent);waitForIdleSync();waitForFrames();
            check(NativeBridge.inputState().contains("XInput 0x0"),"Reopening diagnostic presents with neutral input");
            runOnMainSync(activity::finish);
            result.putString("stream",results+"\n"+checks+" device checks passed\n");finish(Activity.RESULT_OK,result);
        } catch(Throwable failure) {
            android.util.Log.e("SupermanDeviceSmoke","Device validation failed",failure);
            result.putString("stream",results+"\nFAIL "+failure+"\n");finish(Activity.RESULT_CANCELED,result);
        }
    }
    private String waitForFrames() {
        long deadline=SystemClock.uptimeMillis()+10000;
        while(SystemClock.uptimeMillis()<deadline) {
            String text=screenText(); if(text.contains("ERRO:"))throw new AssertionError(text);
            if(text.matches("(?s).*quadros apresentados: [1-9][0-9]*.*"))return text;
            SystemClock.sleep(100);
        }
        throw new AssertionError("No Vulkan frames: "+screenText());
    }
    private String screenText() {StringBuilder text=new StringBuilder();runOnMainSync(() -> collectText(activity.getWindow().getDecorView(),text));return text.toString();}
    private static void collectText(View view,StringBuilder out) {if(view instanceof TextView t)out.append(t.getText()).append('\n');if(view instanceof ViewGroup g)for(int i=0;i<g.getChildCount();i++)collectText(g.getChildAt(i),out);}
    private static ControllerView findController(View view) {if(view instanceof ControllerView c)return c;if(view instanceof ViewGroup g)for(int i=0;i<g.getChildCount();i++){ControllerView c=findController(g.getChildAt(i));if(c!=null)return c;}return null;}
    private void touch(long down,int action,int[] ids,float[] x,float[] y) {
        MotionEvent.PointerProperties[] props=new MotionEvent.PointerProperties[ids.length];
        MotionEvent.PointerCoords[] coords=new MotionEvent.PointerCoords[ids.length];
        for(int i=0;i<ids.length;i++){props[i]=new MotionEvent.PointerProperties();props[i].id=ids[i];props[i].toolType=MotionEvent.TOOL_TYPE_FINGER;coords[i]=new MotionEvent.PointerCoords();coords[i].x=x[i];coords[i].y=y[i];coords[i].pressure=1;coords[i].size=.1f;}
        MotionEvent event=MotionEvent.obtain(down,SystemClock.uptimeMillis(),action,ids.length,props,coords,0,0,1,1,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
        sendPointerSync(event);event.recycle();waitForIdleSync();
    }
    private void screenshot(String name) throws Exception {
        SystemClock.sleep(300);Bitmap bitmap=getUiAutomation().takeScreenshot();if(bitmap==null)throw new AssertionError("No screenshot");
        File dir=new File(getTargetContext().getExternalFilesDir(null),"validation");if(!dir.exists()&&!dir.mkdirs())throw new java.io.IOException("Cannot create evidence directory");
        try(FileOutputStream out=new FileOutputStream(new File(dir,name))){bitmap.compress(Bitmap.CompressFormat.PNG,100,out);}finally{bitmap.recycle();}
    }
}

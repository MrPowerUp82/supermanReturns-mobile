package org.supermanreturns.mobile;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayList;
import java.util.List;

/** Multitouch controls, merged with a physical gamepad using XInput button bits. */
final class ControllerView extends View {
    interface InputSink {void set(int buttons,float lx,float ly,float rx,float ry,float lt,float rt);}
    private final InputSink sink;
    private static final class Control {
        final String label; final float x,y,r; final int bit,axis;
        int pointer=-1; float dx,dy;
        Control(String label,float x,float y,float r,int bit,int axis) { this.label=label;this.x=x;this.y=y;this.r=r;this.bit=bit;this.axis=axis; }
    }
    private final List<Control> controls=new ArrayList<>();
    private final SparseArray<Control> held=new SparseArray<>();
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private int padButtons;
    private final float[] padAxes=new float[6];
    ControllerView(Context context) {this(context,NativeBridge::setInput);}
    ControllerView(Context context,InputSink sink) { super(context); this.sink=sink;setFocusable(true); setContentDescription("Controles Xbox 360 multitouch"); }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) {
        clear(); controls.clear(); float r=Math.min(h*.07f,w*.034f);
        add("L",.15f,.62f,r*1.8f,0,1); add("R",.67f,.62f,r*1.8f,0,2);
        add("A",.86f,.72f,r,0x1000,0); add("B",.93f,.57f,r,0x2000,0);
        add("X",.79f,.57f,r,0x4000,0); add("Y",.86f,.42f,r,0x8000,0);
        add("LB",.10f,.27f,r,0x100,0); add("LT",.23f,.27f,r,0,3);
        add("RB",.90f,.27f,r,0x200,0); add("RT",.77f,.27f,r,0,4);
        add("BACK",.44f,.30f,r,0x20,0); add("START",.56f,.30f,r,0x10,0);
        add("↑",.36f,.59f,r*.75f,1,0); add("↓",.36f,.83f,r*.75f,2,0);
        add("←",.30f,.71f,r*.75f,4,0); add("→",.42f,.71f,r*.75f,8,0);
        add("L3",.13f,.89f,r*.7f,0x40,0); add("R3",.67f,.89f,r*.7f,0x80,0);
    }
    private void add(String text,float x,float y,float r,int bit,int axis) { controls.add(new Control(text,x*getWidth(),y*getHeight(),r,bit,axis)); }
    @Override protected void onDraw(Canvas canvas) {
        for(Control c:controls) {
            paint.setStyle(Paint.Style.FILL); paint.setColor(c.pointer>=0?0xaae8b350:0x88334c66);
            canvas.drawCircle(c.x,c.y,c.r,paint); paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(2); paint.setColor(0xffa8b9cc);
            canvas.drawCircle(c.x,c.y,c.r,paint); paint.setStyle(Paint.Style.FILL);
            if(c.axis==1 || c.axis==2) { paint.setColor(0xffe8b350); canvas.drawCircle(c.x+c.dx*c.r*.65f,c.y+c.dy*c.r*.65f,c.r*.32f,paint); }
            paint.setColor(Color.WHITE); paint.setTextAlign(Paint.Align.CENTER); paint.setTextSize(Math.max(12,c.r*.5f));
            canvas.drawText(c.label,c.x,c.y+c.r*.18f,paint);
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action=event.getActionMasked(), index=event.getActionIndex(), id=event.getPointerId(index);
        if(action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            float x=event.getX(index), y=event.getY(index);
            for(Control c:controls) if(c.pointer<0 && Math.hypot(x-c.x,y-c.y)<=c.r*1.15f) {
                c.pointer=id; held.put(id,c); move(c,x,y); break;
            }
        } else if(action==MotionEvent.ACTION_MOVE) {
            for(int i=0;i<event.getPointerCount();i++) { Control c=held.get(event.getPointerId(i)); if(c!=null) move(c,event.getX(i),event.getY(i)); }
        } else if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
            Control c=held.get(id); if(c!=null) { c.pointer=-1; c.dx=c.dy=0; held.remove(id); }
            if(action==MotionEvent.ACTION_UP) performClick();
        } else if(action==MotionEvent.ACTION_CANCEL) { clear(); return true; }
        send(); invalidate(); return true;
    }
    @Override public boolean performClick() { super.performClick(); return true; }
    private void move(Control c,float x,float y) {
        c.dx=(x-c.x)/c.r; c.dy=(y-c.y)/c.r;
        float length=(float)Math.hypot(c.dx,c.dy); if(length>1) { c.dx/=length;c.dy/=length; }
    }
    void clear() { for(Control c:controls) {c.pointer=-1;c.dx=c.dy=0;} held.clear(); padButtons=0;java.util.Arrays.fill(padAxes,0);send();invalidate(); }
    private void send() {
        int buttons=padButtons; float lx=padAxes[0],ly=padAxes[1],rx=padAxes[2],ry=padAxes[3],lt=padAxes[4],rt=padAxes[5];
        for(Control c:controls) if(c.pointer>=0) {
            buttons|=c.bit;
            switch(c.axis) {
                case 1 -> {lx=c.dx;ly=-c.dy;} case 2 -> {rx=c.dx;ry=-c.dy;}
                case 3 -> lt=1; case 4 -> rt=1;
            }
        }
        sink.set(buttons,lx,ly,rx,ry,lt,rt);
    }
    boolean padKey(KeyEvent e) {
        int bit=switch(e.getKeyCode()) {
            case KeyEvent.KEYCODE_BUTTON_A -> 0x1000; case KeyEvent.KEYCODE_BUTTON_B -> 0x2000;
            case KeyEvent.KEYCODE_BUTTON_X -> 0x4000; case KeyEvent.KEYCODE_BUTTON_Y -> 0x8000;
            case KeyEvent.KEYCODE_BUTTON_L1 -> 0x100; case KeyEvent.KEYCODE_BUTTON_R1 -> 0x200;
            case KeyEvent.KEYCODE_BUTTON_START -> 0x10; case KeyEvent.KEYCODE_BUTTON_SELECT -> 0x20;
            case KeyEvent.KEYCODE_BUTTON_THUMBL -> 0x40; case KeyEvent.KEYCODE_BUTTON_THUMBR -> 0x80;
            case KeyEvent.KEYCODE_DPAD_UP -> 1; case KeyEvent.KEYCODE_DPAD_DOWN -> 2;
            case KeyEvent.KEYCODE_DPAD_LEFT -> 4; case KeyEvent.KEYCODE_DPAD_RIGHT -> 8; default -> 0;
        };
        if(bit==0) return false;
        if(e.getAction()==KeyEvent.ACTION_DOWN) padButtons|=bit;
        else if(e.getAction()==KeyEvent.ACTION_UP) padButtons&=~bit;
        send();return true;
    }
    boolean padMotion(MotionEvent e) {
        padAxes[0]=dead(e.getAxisValue(MotionEvent.AXIS_X));padAxes[1]=-dead(e.getAxisValue(MotionEvent.AXIS_Y));
        padAxes[2]=dead(e.getAxisValue(MotionEvent.AXIS_Z));padAxes[3]=-dead(e.getAxisValue(MotionEvent.AXIS_RZ));
        padAxes[4]=Math.max(0,Math.max(e.getAxisValue(MotionEvent.AXIS_LTRIGGER),e.getAxisValue(MotionEvent.AXIS_BRAKE)));
        padAxes[5]=Math.max(0,Math.max(e.getAxisValue(MotionEvent.AXIS_RTRIGGER),e.getAxisValue(MotionEvent.AXIS_GAS)));
        padButtons&=~15;
        float hx=e.getAxisValue(MotionEvent.AXIS_HAT_X),hy=e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        if(hx<-.5f)padButtons|=4;if(hx>.5f)padButtons|=8;if(hy<-.5f)padButtons|=1;if(hy>.5f)padButtons|=2;
        send();return true;
    }
    private float dead(float v) { return Math.abs(v)<.15f?0:Math.max(-1,Math.min(1,v)); }
}

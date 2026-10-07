package org.supermanreturns.mobile;

import android.os.Bundle;
import android.view.ViewGroup;
import org.libsdl.app.SDLActivity;

/** Real recompiled guest entry point. Kept in a separate process from the installer. */
public final class GameActivity extends SDLActivity {
    private ControllerView controls;
    @Override protected String[] getLibraries() { return new String[]{"c++_shared","rexruntime","superman_game"}; }
    @Override protected String[] getArguments() {
        String files=getFilesDir().getAbsolutePath();
        return new String[]{"--game_data_root="+files+"/game","--user_data_root="+files+"/userdata",
            "--cache_path="+files+"/cache","--log_file="+files+"/game.log","--log_level=info"};
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        String files=getFilesDir().getAbsolutePath();
        nativeSetenv("HOME",files);nativeSetenv("XDG_DATA_HOME",files);nativeSetenv("SR_ANDROID_FILES",files);
        controls=new ControllerView(this,GameActivity::setTouchState);
        mLayout.addView(controls,new ViewGroup.LayoutParams(-1,-1));
    }
    @Override protected void onPause() {if(controls!=null)controls.clear();setNativePaused(true);super.onPause();}
    @Override protected void onResume() {super.onResume();setNativePaused(false);}
    @Override protected void onDestroy() {if(controls!=null)controls.clear();super.onDestroy();}
    @Override public void onWindowFocusChanged(boolean focus) {super.onWindowFocusChanged(focus);if(!focus&&controls!=null)controls.clear();}
    private static native void setTouchState(int buttons,float lx,float ly,float rx,float ry,float lt,float rt);
    private static native void setNativePaused(boolean paused);
}

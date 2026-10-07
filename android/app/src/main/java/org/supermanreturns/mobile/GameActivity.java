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
            "--cache_path="+files+"/cache","--log_file="+files+"/game.log","--gpu_backend=vulkan","--log_level=info",
            "--draw_resolution_scale_x=1","--draw_resolution_scale_y=1","--resolution_scale=1",
            "--texture_cache_memory_limit_soft=256","--texture_cache_memory_limit_hard=512",
            "--texture_cache_memory_limit_render_to_texture=64","--texture_cache_memory_limit_soft_lifetime=5",
            "--vulkan_mobile_cache_clear_interval=60","--vulkan_render_target_height_limit=720"};
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        String files=getFilesDir().getAbsolutePath();
        nativeSetenv("HOME",files);nativeSetenv("XDG_DATA_HOME",files);nativeSetenv("SR_ANDROID_FILES",files);
        controls=new ControllerView(this,GameActivity::setTouchState);
        mLayout.addView(controls,new ViewGroup.LayoutParams(-1,-1));
    }
    @Override protected void onPause() {if(controls!=null)controls.clear();super.onPause();}
    @Override protected void onDestroy() {if(controls!=null)controls.clear();super.onDestroy();}
    @Override public void onWindowFocusChanged(boolean focus) {super.onWindowFocusChanged(focus);if(!focus&&controls!=null)controls.clear();}
    private static native void setTouchState(int buttons,float lx,float ly,float rx,float ry,float lt,float rt);
}

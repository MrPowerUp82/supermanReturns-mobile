package org.supermanreturns.mobile;

import android.view.Surface;

final class NativeBridge {
    static { System.loadLibrary("superman_mobile"); }
    static native String probe();
    static native String openSurface(long owner, Surface surface);
    static native String draw(long owner);
    static native void closeSurface(long owner);
    // The diagnostic uses the same bit layout as Xbox 360 XInput. No guest runs yet.
    static native void setInput(int buttons, float lx, float ly, float rx, float ry, float lt, float rt);
    static native String inputState();
    private NativeBridge() {}
}

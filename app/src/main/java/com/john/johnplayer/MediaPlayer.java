package com.john.testcplusplus;

import android.view.Surface;

public class MediaPlayer {

    public native String getPlayerVersion();

    public native void playVideo(String file, Surface surface);

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("mediacore");
    }
}

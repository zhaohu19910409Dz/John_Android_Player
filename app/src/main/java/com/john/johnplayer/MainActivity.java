package com.john.johnplayer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import android.Manifest;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;

public class MainActivity extends AppCompatActivity implements View.OnClickListener{
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private com.john.johnplayer.MediaPlayer player;
    private Button btPlay,btPause;


    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        ActivityCompat.requestPermissions(this,new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 1);
        ActivityCompat.requestPermissions(this,new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);

        player = new com.john.johnplayer.MediaPlayer();

        surfaceView = (SurfaceView)findViewById(R.id.surfaceView);
        surfaceHolder = surfaceView.getHolder();

        btPlay = (Button)findViewById(R.id.btPlay);
        btPlay.setOnClickListener(this);
        btPause = (Button)findViewById(R.id.btPause);
        btPause.setOnClickListener(this);

        //String p = Environment.getExternalStorageDirectory()+"/1.mp4";
        //player.playVideo(p,surfaceHolder.getSurface());

    }

    public void onClick(View v)
    {
        switch (v.getId())
        {
            case R.id.btPlay:
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        Log.i("MediaCore","App:btPlay");
                        String path = Environment.getExternalStorageDirectory()+"/1.mp4";
                        player.playVideo(path,surfaceHolder.getSurface());
                    }
                }).start();
                break;
            case R.id.btPause:
                Log.i("MediaCore","App:btPause");
                break;
        }
    }
}

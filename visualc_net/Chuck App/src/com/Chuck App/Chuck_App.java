package com.Chuck_App;

import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import java.io.*;
import android.content.res.AssetManager;
import android.util.Log;

import android.view.*;

public class Chuck_App extends SDLActivity
{
	public void copyFileOrDir(String path) {
    AssetManager assetManager = this.getAssets();
    String assets[] = null;
    try {
        assets = assetManager.list(path);
        if (assets.length == 0) {
            copyFile(path);
        } else {
            String fullPath = this.getFilesDir().getPath() + "/" + path;
            File dir = new File(fullPath);
            if (!dir.exists())
                dir.mkdir();
            for (int i = 0; i < assets.length; ++i) {
                copyFileOrDir(path + "/" + assets[i]);
            }
        }
    } catch (IOException ex) {
        Log.e("tag", "I/O Exception", ex);
    }
}

private void copyFile(String filename) {
    AssetManager assetManager = this.getAssets();

    InputStream in = null;
    OutputStream out = null;
    try {
        in = assetManager.open(filename);
        String newFileName = this.getFilesDir().getPath() + "/" + filename;
        out = new FileOutputStream(newFileName);

        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
        in.close();
        in = null;
        out.flush();
        out.close();
        out = null;
    } catch (Exception e) {
        Log.e("tag", e.getMessage());
    }

}

@Override
public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);
    if (hasFocus) {
        hideSystemUI();
    }
}

private void hideSystemUI() {
    // Enables regular immersive mode.
    // For "lean back" mode, remove SYSTEM_UI_FLAG_IMMERSIVE.
    // Or for "sticky immersive," replace it with SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    View decorView = getWindow().getDecorView();
    decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE
            // Set the content to appear under the system bars so that the
            // content doesn't resize when the system bars hide and show.
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            // Hide the nav bar and status bar
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN);
}

// Shows the system bars by removing all the flags
// except for the ones that make the content appear under the system bars.
private void showSystemUI() {
    View decorView = getWindow().getDecorView();
    decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
}


    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
		hideSystemUI();

    	copyFileOrDir("icons");
		copyFileOrDir("chuck");
		copyFileOrDir("dosbox.conf");

        super.onCreate(savedInstanceState);
    }
}
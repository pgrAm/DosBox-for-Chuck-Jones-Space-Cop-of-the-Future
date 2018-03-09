package com.Chuck_App;

import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import java.io.*;
import android.content.res.AssetManager;
import android.util.Log;

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

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
		copyFileOrDir("chuck");
		copyFileOrDir("dosbox.conf");

        super.onCreate(savedInstanceState);
    }
}
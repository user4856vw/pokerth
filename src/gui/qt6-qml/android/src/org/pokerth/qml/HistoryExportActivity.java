package org.pokerth.qml;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Copies a staged Kibitzer JSON archive into a document chosen by the user.
 *
 * A helper Activity is used so we can receive ACTION_CREATE_DOCUMENT's result
 * without subclassing QtActivity or passing potentially megabytes of JSON
 * through an Intent, the clipboard, or the Binder transaction buffer.
 * The staged source remains private to this app until the user chooses a
 * destination; this Activity is not exported.
 */
public final class HistoryExportActivity extends Activity {
    private static final String TAG = "PokerTH-HistoryExport";
    private static final String EXTRA_SOURCE = "sourcePath";
    private static final String STATE_PICKER_STARTED = "pickerStarted";
    private static final int REQUEST_CREATE_DOCUMENT = 2701;

    public static void start(Context context, String sourcePath) {
        Intent intent = new Intent(context, HistoryExportActivity.class);
        intent.putExtra(EXTRA_SOURCE, sourcePath);
        if (!(context instanceof Activity))
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (state != null && state.getBoolean(STATE_PICKER_STARTED, false))
            return; // Picker is already open; Android will deliver its result.

        Intent picker = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("application/json");
        picker.putExtra(Intent.EXTRA_TITLE, "pokerth-kibitzer-history.json");
        try {
            startActivityForResult(picker, REQUEST_CREATE_DOCUMENT);
        } catch (RuntimeException error) {
            Log.e(TAG, "Could not open Android Save file picker", error);
            Toast.makeText(this, "Cannot open file picker", Toast.LENGTH_LONG).show();
            finish();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        state.putBoolean(STATE_PICKER_STARTED, true);
        super.onSaveInstanceState(state);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_CREATE_DOCUMENT)
            return;

        if (resultCode == RESULT_OK && data != null && data.getData() != null) {
            try {
                File source = getPrivateStagedFile();
                Uri destination = data.getData();
                try (InputStream in = new FileInputStream(source);
                     OutputStream out = getContentResolver().openOutputStream(destination, "w")) {
                    if (out == null)
                        throw new java.io.IOException("Document provider returned null stream");
                    byte[] buffer = new byte[32768];
                    int count;
                    while ((count = in.read(buffer)) != -1)
                        out.write(buffer, 0, count);
                    out.flush();
                }
                Toast.makeText(this, "Kibitzer JSON saved", Toast.LENGTH_LONG).show();
            } catch (Exception error) {
                Log.e(TAG, "Could not export Kibitzer JSON", error);
                Toast.makeText(this, "Could not save Kibitzer JSON", Toast.LENGTH_LONG).show();
            }
        }
        finish();
    }

    private File getPrivateStagedFile() throws java.io.IOException {
        String path = getIntent().getStringExtra(EXTRA_SOURCE);
        if (path == null)
            throw new java.io.IOException("Missing export file");
        File source = new File(path).getCanonicalFile();
        String cache = getCacheDir().getCanonicalPath() + File.separator;
        if (!source.getPath().startsWith(cache))
            throw new SecurityException("Export source not inside private cache");
        return source;
    }

    @Override
    protected void onDestroy() {
        // Delete only when finishing (not when Android recreates the Activity).
        if (isFinishing()) {
            try {
                File source = getPrivateStagedFile();
                if (!source.delete() && source.exists())
                    Log.w(TAG, "Could not remove staged JSON file");
            } catch (Exception error) {
                Log.w(TAG, "Could not remove staged JSON file", error);
            }
        }
        super.onDestroy();
    }
}

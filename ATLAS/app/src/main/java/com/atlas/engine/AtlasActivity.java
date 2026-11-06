package com.atlas.engine;

import android.app.NativeActivity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import java.util.Locale;

public final class AtlasActivity extends NativeActivity {
    private TextView fpsView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        float density = getResources().getDisplayMetrics().density;
        int padding = Math.round(8 * density);
        int margin = Math.round(12 * density);
        fpsView = new TextView(this);
        fpsView.setTextColor(Color.WHITE);
        fpsView.setTextSize(14);
        fpsView.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        fpsView.setPadding(padding, padding, padding, padding);
        fpsView.setContentDescription("ATLAS FPS");
        fpsView.setText("FPS: --\nИнтервал: --");
        GradientDrawable background = new GradientDrawable();
        background.setColor(0xb3000000);
        background.setCornerRadius(6 * density);
        fpsView.setBackground(background);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.START);
        params.setMargins(margin, margin, margin, margin);
        addContentView(fpsView, params);
    }

    // Called by the native render thread, at most once per statistics interval.
    public void updateFps(final float fps, final float intervalMs, final int frames) {
        runOnUiThread(() -> {
            if (fps < 0) {
                fpsView.setText("FPS: --\nИнтервал: --");
            } else {
                fpsView.setText(String.format(Locale.US,
                        "FPS: %.1f\nИнтервал: %.1f мс\nКадров: %d",
                        fps, intervalMs, frames));
            }
        });
    }
}

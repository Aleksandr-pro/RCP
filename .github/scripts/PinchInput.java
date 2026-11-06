import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.MotionEvent;
import java.lang.reflect.Method;

/** Emulator-only two-pointer input injector, executed as the Android shell user. */
public final class PinchInput {
    private static Object manager;
    private static Method inject;
    private static long downTime;

    private static void send(int action, float[] xs, float y) throws Exception {
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[xs.length];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[xs.length];
        for (int i = 0; i < xs.length; i++) {
            props[i] = new MotionEvent.PointerProperties();
            props[i].id = i;
            props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x = xs[i];
            coords[i].y = y;
            coords[i].pressure = 1.0f;
            coords[i].size = 1.0f;
        }
        MotionEvent event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action,
                xs.length, props, coords, 0, 0, 1f, 1f, 0, 0,
                InputDevice.SOURCE_TOUCHSCREEN, 0);
        try {
            if (!((Boolean) inject.invoke(manager, event, 2))) {
                throw new IllegalStateException("Input injection failed: " + action);
            }
        } finally {
            event.recycle();
        }
        SystemClock.sleep(30);
    }

    public static void main(String[] args) throws Exception {
        String mode = args[0];
        float centerX = Float.parseFloat(args[1]) / 2;
        float centerY = Float.parseFloat(args[2]) / 2;
        Class<?> cls = Class.forName("android.hardware.input.InputManager");
        manager = cls.getMethod("getInstance").invoke(null);
        inject = cls.getMethod("injectInputEvent", InputEvent.class, int.class);
        float startGap = mode.equals("in") ? 160 : 80;
        float endGap = mode.equals("in") ? 20 : 160;
        downTime = SystemClock.uptimeMillis();
        send(MotionEvent.ACTION_DOWN, new float[]{centerX - startGap / 2}, centerY);
        send(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new float[]{centerX - startGap / 2, centerX + startGap / 2}, centerY);
        if (mode.equals("hold")) SystemClock.sleep(3000);
        for (int i = 1; i <= 12; i++) {
            float gap = startGap + (endGap - startGap) * i / 12;
            send(MotionEvent.ACTION_MOVE, new float[]{centerX - gap / 2, centerX + gap / 2}, centerY);
        }
        send(MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new float[]{centerX - endGap / 2, centerX + endGap / 2}, centerY);
        // The remaining finger moves after pinch ends; this must not rotate the view.
        send(MotionEvent.ACTION_MOVE, new float[]{centerX - endGap / 2 + 30}, centerY);
        send(MotionEvent.ACTION_UP, new float[]{centerX - endGap / 2 + 30}, centerY);
    }
}

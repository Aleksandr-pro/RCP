import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.MotionEvent;
import java.lang.reflect.Method;

// Emulator-only multi-touch injector; never packaged in the app.
public final class TerrainInput {
    static Object manager;
    static Method inject;
    static long down;
    static void send(int action, int count, float x, float y) throws Exception {
        MotionEvent.PointerProperties[] p = new MotionEvent.PointerProperties[count];
        MotionEvent.PointerCoords[] c = new MotionEvent.PointerCoords[count];
        for (int i=0; i<count; i++) {
            p[i] = new MotionEvent.PointerProperties(); p[i].id=i; p[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
            c[i] = new MotionEvent.PointerCoords(); c[i].x=x+i*16; c[i].y=y+i*16;
            c[i].pressure=1; c[i].size=1;
        }
        MotionEvent e=MotionEvent.obtain(down,SystemClock.uptimeMillis(),action,count,p,c,0,0,1,1,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
        try { if (!((Boolean)inject.invoke(manager,e,2))) throw new Exception("inject failed"); }
        finally { e.recycle(); }
        SystemClock.sleep(30);
    }
    static void gesture(int count, float x, float y, float dx, float dy) throws Exception {
        down=SystemClock.uptimeMillis(); send(MotionEvent.ACTION_DOWN,1,x,y);
        for (int n=2;n<=count;n++) send(MotionEvent.ACTION_POINTER_DOWN | ((n-1)<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),n,x,y);
        for (int j=1;j<=12;j++) send(MotionEvent.ACTION_MOVE,count,x+dx*j/12,y+dy*j/12);
        for (int n=count;n>=2;n--) send(MotionEvent.ACTION_POINTER_UP | ((n-1)<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),n,x+dx,y+dy);
        send(MotionEvent.ACTION_UP,1,x+dx,y+dy);
    }
    public static void main(String[] args) throws Exception {
        Class<?> cl=Class.forName("android.hardware.input.InputManager");
        manager=cl.getMethod("getInstance").invoke(null); inject=cl.getMethod("injectInputEvent",InputEvent.class,int.class);
        float width=Float.parseFloat(args[1]), height=Float.parseFloat(args[2]);
        if (args[0].equals("switch")) gesture(4,width*.45f,height*.45f,0,0);
        else if (args[0].equals("move")) {
            int count=Integer.parseInt(args[3]);
            for(int i=0;i<count;i++) {
                gesture(3,width*.2f,height*.4f,width*.6f,0);
                gesture(3,width*.8f,height*.4f,-width*.6f,0);
            }
        }
    }
}

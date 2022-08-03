package xyz.fragcolor.virtual_ui_demo;
import org.libsdl.app.SDLActivity;

public class AppActivity extends SDLActivity {
    protected String[] getLibraries() {
        return  new String[] {
            "virtual_ui_demo" // libApp.so
        };
    }
}

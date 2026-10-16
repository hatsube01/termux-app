package com.termux.app;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.preference.PreferenceManager;

import com.termux.x11.controller.core.AppUtils;
import com.termux.x11.controller.inputcontrols.ControlsProfile;
import com.termux.x11.controller.inputcontrols.InputControlsManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * 内置按键预设（assets/inputcontrols/profiles）的回归测试。
 * 覆盖两点：全新安装只带新预设；已有安装升级后旧预设被清掉、新预设补上。
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 33, application = android.app.Application.class)
public class InputControlsPresetsTest {

    /** 期望的内置预设名，顺序与 ControlsProfile.compareTo（按 id 升序）一致 */
    private static final List<String> EXPECTED = Arrays.asList("RTS", "Virtual Gamepad", "Visual Novels");

    private Context context;

    @Before
    public void setUp() throws Exception {
        context = RuntimeEnvironment.getApplication();

        File profilesDir = InputControlsManager.getProfilesDir(context);
        File[] files = profilesDir.listFiles();
        if (files != null) for (File file : files) file.delete();

        PreferenceManager.getDefaultSharedPreferences(context).edit().clear().commit();
    }

    private static void writeProfile(File dir, String fileName, int id, String name) throws Exception {
        String json = "{\"id\":" + id + ",\"name\":\"" + name + "\",\"cursorSpeed\":1,\"elements\":[]}";
        try (FileOutputStream out = new FileOutputStream(new File(dir, fileName))) {
            out.write(json.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static List<String> namesOf(InputControlsManager manager) {
        List<String> names = new ArrayList<>();
        for (ControlsProfile profile : manager.getProfiles()) names.add(profile.getName());
        return names;
    }

    @Test
    public void freshInstallShipsOnlyTheNewPresets() {
        List<String> names = namesOf(new InputControlsManager(context));
        System.out.println("PRESETS_FRESH " + names);
        Assert.assertEquals(EXPECTED, names);
    }

    @Test
    public void upgradeDropsRemovedPresetsAndAddsVisualNovels() throws Exception {
        // 模拟「已装过旧版本」：本地有 5 个旧内置预设，且同步标记已经是最新 versionCode
        File dir = InputControlsManager.getProfilesDir(context);
        writeProfile(dir, "controls-1.icp", 1, "RTS");
        writeProfile(dir, "controls-2.icp", 2, "Template (12 buttons)");
        writeProfile(dir, "controls-3.icp", 3, "Virtual Gamepad");
        writeProfile(dir, "controls-4.icp", 4, "FPS");
        writeProfile(dir, "controls-5.icp", 5, "暗黑走路摇杆");

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        preferences.edit().putInt("inputcontrols_app_version", AppUtils.getVersionCode(context)).commit();

        List<String> names = namesOf(new InputControlsManager(context));
        System.out.println("PRESETS_UPGRADE " + names);
        Assert.assertEquals(EXPECTED, names);
        Assert.assertFalse("controls-2.icp 应被清理", new File(dir, "controls-2.icp").exists());
        Assert.assertFalse("controls-5.icp 应被清理", new File(dir, "controls-5.icp").exists());
    }
}

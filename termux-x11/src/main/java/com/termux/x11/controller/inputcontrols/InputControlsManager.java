package com.termux.x11.controller.inputcontrols;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.media.MediaScannerConnection;
import android.os.Environment;
import android.util.JsonReader;

import androidx.preference.PreferenceManager;

import com.termux.x11.controller.core.AppUtils;
import com.termux.x11.controller.core.FileUtils;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

public class InputControlsManager {
    private final Context context;
    private ArrayList<ControlsProfile> profiles;
    private int maxProfileId;
    private boolean profilesLoaded = false;

    public InputControlsManager(Context context) {
        this.context = context;
    }

    public static File getProfilesDir(Context context) {
        File profilesDir = new File(context.getFilesDir(), "profiles");
        if (!profilesDir.isDirectory()) profilesDir.mkdir();
        return profilesDir;
    }

    public ArrayList<ControlsProfile> getProfiles() {
        return getProfiles(false);
    }

    public ArrayList<ControlsProfile> getProfiles(boolean ignoreTemplates) {
        if (!profilesLoaded) loadProfiles(ignoreTemplates);
        return profiles;
    }

    /**
     * 内置预设的修订号。每次增删 assets/inputcontrols/profiles 下的内置预设都要 +1，
     * 这样已有安装（versionCode 不变）也能在下一次启动时同步一次内置预设。
     */
    private static final int ASSET_PROFILES_REVISION = 2;

    /** 已从内置预设中移除的按键配置名，用于升级时清理设备上残留的旧预设 */
    private static final String[] REMOVED_ASSET_PROFILE_NAMES = {"Template (12 buttons)", "FPS", "暗黑走路摇杆"};

    private void copyAssetProfilesIfNeeded() {
        File profilesDir = InputControlsManager.getProfilesDir(context);
        if (FileUtils.isEmpty(profilesDir)) {
            FileUtils.copy(context, "inputcontrols/profiles", profilesDir);
            return;
        }

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);

        int newVersion = AppUtils.getVersionCode(context);
        int oldVersion = preferences.getInt("inputcontrols_app_version", 0);
        int oldRevision = preferences.getInt("inputcontrols_assets_revision", 0);
        if (oldVersion == newVersion && oldRevision == ASSET_PROFILES_REVISION) return;
        preferences.edit()
                .putInt("inputcontrols_app_version", newVersion)
                .putInt("inputcontrols_assets_revision", ASSET_PROFILES_REVISION)
                .apply();

        File[] files = profilesDir.listFiles();
        if (files == null) return;

        try {
            AssetManager assetManager = context.getAssets();
            String[] assetFiles = assetManager.list("inputcontrols/profiles");
            for (String assetFile : assetFiles) {
                String assetPath = "inputcontrols/profiles/"+assetFile;
                ControlsProfile originProfile = loadProfile(context, assetManager.open(assetPath));
                if (originProfile == null) continue;

                File targetFile = null;
                for (File file : files) {
                    ControlsProfile targetProfile = loadProfile(context, file);
                    if (targetProfile == null) continue;
                    if (originProfile.id == targetProfile.id && originProfile.getName().equals(targetProfile.getName())) {
                        targetFile = file;
                        break;
                    }
                }

                // 新版新增的内置预设本地没有同 id 同名的文件，按资源文件名补一份；
                // 否则新版本带的内置预设永远不会出现在已有安装上
                FileUtils.copy(context, assetPath, targetFile != null ? targetFile : new File(profilesDir, assetFile));
            }

            pruneRemovedAssetProfiles(preferences, files);
        }
        catch (IOException e) {}
    }

    /** 一次性清理：删掉设备上残留的、本版本已不再作为内置预设提供的按键配置 */
    private void pruneRemovedAssetProfiles(SharedPreferences preferences, File[] files) {
        if (preferences.getBoolean("inputcontrols_removed_presets_pruned", false)) return;
        preferences.edit().putBoolean("inputcontrols_removed_presets_pruned", true).apply();

        for (File file : files) {
            try {
                ControlsProfile profile = loadProfile(context, file);
                if (profile != null && Arrays.asList(REMOVED_ASSET_PROFILE_NAMES).contains(profile.getName())) {
                    //noinspection ResultOfMethodCallIgnored
                    file.delete();
                }
            }
            catch (Exception e) {}
        }
    }

    public void loadProfiles(boolean ignoreTemplates) {
        File profilesDir = InputControlsManager.getProfilesDir(context);
        copyAssetProfilesIfNeeded();

        ArrayList<ControlsProfile> profiles = new ArrayList<>();
        File[] files = profilesDir.listFiles();
        if (files != null) {
            for (File file : files) {
                ControlsProfile profile = loadProfile(context, file);
                if (!(ignoreTemplates && profile.isTemplate())) profiles.add(profile);
                maxProfileId = Math.max(maxProfileId, profile.id);
            }
        }

        Collections.sort(profiles);
        this.profiles = profiles;
        profilesLoaded = true;
    }

    public ControlsProfile createProfile(String name) {
        ControlsProfile profile = new ControlsProfile(context, ++maxProfileId);
        profile.setName(name);
        profile.save();
        profiles.add(profile);
        return profile;
    }

    public ControlsProfile duplicateProfile(ControlsProfile source) {
        String newName;
        for (int i = 1;;i++) {
            newName = source.getName() + " ("+i+")";
            boolean found = false;
            for (ControlsProfile profile : profiles) {
                if (profile.getName().equals(newName)) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }

        int newId = ++maxProfileId;
        File newFile = ControlsProfile.getProfileFile(context, newId);

        try {
            JSONObject data = new JSONObject(FileUtils.readString(ControlsProfile.getProfileFile(context, source.id)));
            data.put("id", newId);
            data.put("name", newName);
            if (data.has("template")) data.remove("template");
            FileUtils.writeString(newFile, data.toString());
        }
        catch (JSONException e) {}

        ControlsProfile profile = loadProfile(context, newFile);
        profiles.add(profile);
        return profile;
    }

    public void removeProfile(ControlsProfile profile) {
        File file = ControlsProfile.getProfileFile(context, profile.id);
        if (file.isFile() && file.delete()) profiles.remove(profile);
    }

    public ControlsProfile importProfile(JSONObject data) {
        try {
            if (!data.has("id") || !data.has("name")) return null;
            int newId = ++maxProfileId;
            File newFile = ControlsProfile.getProfileFile(context, newId);
            data.put("id", newId);
            FileUtils.writeString(newFile, data.toString());
            ControlsProfile newProfile = loadProfile(context, newFile);

            int foundIndex = -1;
            for (int i = 0; i < profiles.size(); i++) {
                ControlsProfile profile = profiles.get(i);
                if (profile.getName().equals(newProfile.getName())) {
                    foundIndex = i;
                    break;
                }
            }

            if (foundIndex != -1) {
                profiles.set(foundIndex, newProfile);
            }
            else profiles.add(newProfile);
            return newProfile;
        }
        catch (JSONException e) {
            return null;
        }
    }

    public File exportProfile(ControlsProfile profile) {
        File downloadsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        File destination = new File(downloadsDir, "Winlator/profiles/"+profile.getName()+".icp");
        FileUtils.copy(ControlsProfile.getProfileFile(context, profile.id), destination);
        MediaScannerConnection.scanFile(context, new String[]{destination.getAbsolutePath()}, null, null);
        return destination.isFile() ? destination : null;
    }

    public static ControlsProfile loadProfile(Context context, File file) {
        try {
            return loadProfile(context, new FileInputStream(file));
        }
        catch (FileNotFoundException e) {
            return null;
        }
    }

    public static ControlsProfile loadProfile(Context context, InputStream inStream) {
        try (JsonReader reader = new JsonReader(new InputStreamReader(inStream, StandardCharsets.UTF_8))) {
            int profileId = 0;
            String profileName = null;
            float cursorSpeed = Float.NaN;
            boolean disableMouseInput = false;
            byte touchpadMode = 0;
            boolean moveCursorToTouchpoint = false;
            boolean twoFingersDrag = true;
            boolean twoFingersRightClick = true;
            boolean longPressRightClick = true;
            boolean pinchZoomEnabled = false;
            boolean shortDragEnabled = false;
            boolean twoFingersScroll = true;

            reader.beginObject();
            while (reader.hasNext()) {
                String name = reader.nextName();

                if (name.equals("id")) {
                    profileId = reader.nextInt();
                }
                else if (name.equals("name")) {
                    profileName = reader.nextString();
                }
                else if (name.equals("cursorSpeed")) {
                    cursorSpeed = (float)reader.nextDouble();
                }
                else if (name.equals("disableMouseInput")) {
                    disableMouseInput = reader.nextBoolean();
                }
                else if (name.equals("touchpadMode")) {
                    touchpadMode = (byte)reader.nextInt();
                }
                else if (name.equals("moveCursorToTouchpoint")) {
                    moveCursorToTouchpoint = reader.nextBoolean();
                }
                else if (name.equals("twoFingersDrag")) {
                    twoFingersDrag = reader.nextBoolean();
                }
                else if (name.equals("twoFingersRightClick")) {
                    twoFingersRightClick = reader.nextBoolean();
                }
                else if (name.equals("longPressRightClick")) {
                    longPressRightClick = reader.nextBoolean();
                }
                else if (name.equals("pinchZoomEnabled")) {
                    pinchZoomEnabled = reader.nextBoolean();
                }
                else if (name.equals("shortDragEnabled")) {
                    shortDragEnabled = reader.nextBoolean();
                }
                else if (name.equals("twoFingersScroll")) {
                    twoFingersScroll = reader.nextBoolean();
                }
                else {
                    reader.skipValue();
                }
            }

            ControlsProfile profile = new ControlsProfile(context, profileId);
            profile.setName(profileName);
            if (!Float.isNaN(cursorSpeed)) profile.setCursorSpeed(cursorSpeed);
            profile.setDisableMouseInput(disableMouseInput);
            profile.setTouchpadMode(touchpadMode);
            profile.setMoveCursorToTouchpoint(moveCursorToTouchpoint);
            profile.setTwoFingersDrag(twoFingersDrag);
            profile.setTwoFingersRightClick(twoFingersRightClick);
            profile.setLongPressRightClick(longPressRightClick);
            profile.setPinchZoomEnabled(pinchZoomEnabled);
            profile.setShortDragEnabled(shortDragEnabled);
            profile.setTwoFingersScroll(twoFingersScroll);
            return profile;
        }
        catch (IOException e) {
            return null;
        }
    }

    public ControlsProfile getProfile(int id) {
        for (ControlsProfile profile : getProfiles()) if (profile.id == id) return profile;
        return null;
    }
}

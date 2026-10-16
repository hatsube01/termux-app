package com.termux.app;

import android.content.res.Resources;

import com.termux.x11.R;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/**
 * 「配置对用户操作的响应」里三个下拉用到的数组必须是完整、可用的。
 *
 * 回归点：这些数组曾经写成 {@code <item>@array/userActionsValues</item>} 这种嵌套数组引用，
 * aapt2 不会展开它，运行时 {@link Resources#getTextArray(int)} 会返回
 * {@code [null, "send volume up"]}——下拉里只剩一个可选项，而且首项为 null 会让对话框
 * 反复重算高度（横屏时表现为弹窗抖动）。条目必须写全。
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 33, application = android.app.Application.class)
public class UserActionsArraysTest {

    private static final int BASE_COUNT = 9;
    private static final int FULL_COUNT = BASE_COUNT + 1;

    private static void assertArray(String name, int resId, int expectedLength) {
        Resources res = RuntimeEnvironment.getApplication().getResources();
        CharSequence[] values = res.getTextArray(resId);
        Assert.assertNotNull(name + " 不应为 null", values);
        Assert.assertEquals(name + " 长度不对（嵌套 @array 没被展开？）", expectedLength, values.length);
        for (int i = 0; i < values.length; i++) {
            Assert.assertNotNull(name + " 第 " + i + " 项为 null（嵌套 @array 没被展开？）", values[i]);
            Assert.assertTrue(name + " 第 " + i + " 项为空串", values[i].length() > 0);
        }
    }

    private static void assertUserActionArrays() {
        assertArray("userActionsValues", R.array.userActionsValues, BASE_COUNT);
        assertArray("userActionsEntries", R.array.userActionsEntries, BASE_COUNT);
        assertArray("userActionsVolumeUpValues", R.array.userActionsVolumeUpValues, FULL_COUNT);
        assertArray("userActionsVolumeUpEntries", R.array.userActionsVolumeUpEntries, FULL_COUNT);
        assertArray("userActionsVolumeDownValues", R.array.userActionsVolumeDownValues, FULL_COUNT);
        assertArray("userActionsVolumeDownEntries", R.array.userActionsVolumeDownEntries, FULL_COUNT);
        assertArray("userActionsMediaKeyValues", R.array.userActionsMediaKeyValues, FULL_COUNT);
        assertArray("userActionsMediaKeyEntries", R.array.userActionsMediaKeyEntries, FULL_COUNT);
    }

    @Test
    public void userActionArraysAreComplete() {
        assertUserActionArrays();
    }

    @Test
    @Config(sdk = 33, application = android.app.Application.class, qualifiers = "zh-rCN")
    public void userActionArraysAreCompleteZh() {
        assertUserActionArrays();
    }
}

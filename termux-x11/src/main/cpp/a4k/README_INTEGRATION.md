# A4K 画质增强：源码接入说明

本目录是**注入式适配层**（不是 Lorie 源码的一部分）：

```
termux-x11/src/main/cpp/a4k/          适配层：PLT 钩子 + 抓帧 + JNI
termux-x11/src/main/cpp/lorie/a4k/    渲染管线：Anime4K / FSR1 EASU / CAS / ArtCNN
termux-x11/src/main/assets/a4k/       上游着色器原件与许可证（运行期做 SHA256 校验）
termux-x11/src/main/java/com/a4k/integrated/  Bridge / 菜单 / 预设
a4k/upstream/                         pipeline.cpp 用相对路径引用的 FFX 头（6 级上溯）
```

## 接线方式

1. CMake 目标 `A4KIntegrated`（`src/main/cpp/CMakeLists.txt` 末尾）产出 `libA4KIntegrated.so`。
2. `LorieView` 静态初始化里 `System.loadLibrary("Xlorie")` 之后调用 `Bridge.boot()`。
3. `bridge.cpp` 起后台发现线程，最多 18 次重试安装钩子（100ms~2s 退避）。
4. 钩子装上后：`glDrawArrays` 里只做一次私有快照（锁内），
   `eglSwapBuffers` 里做多 pass 增强并呈现（锁外）。

## 尚未接入（重要）

| 项 | 状态 | 原因 |
|---|---|---|
| 设置里的"画质增强"菜单入口 | ❌ 未接 | 原包用 smali 往 layout/preferences 里插视图并挂点击，源码级要改 `LoriePreferences`，需先确认它当前的分片结构 |
| 旧版 Lorie viewport / 坐标迁移 | ❌ 未接 | 原包是逐方法替换旧版 smali（`LorieView`、`LorieView$2`、`TouchInputHandler`、`MainActivity`），**没有对应的旧版 Java 源码**，无法机械还原 |
| 菜单里的预设/倍率/对比 | ❌ 未接 | 依赖上面第一项 |

**结论**：引擎与钩子已经能编译进 APK，但`mode`默认 0，没有菜单就无法开启增强。
当前提交是"地基"，不是"可用功能"。

## 构建

```bash
git submodule update --init --recursive     # termux-x11 的 xserver/pixman 等子模块
./gradlew :termux-x11:assembleDebug
```

需要 NDK（本仓库 `ndkVersion` 已固定）与 CMake ≥ 3.22。

## 许可

Anime4K / CAS / FSR / ArtCNN 为 MIT，原始许可证在 `assets/a4k/upstream/` 与 `a4k/upstream/`。
Lorie 与本适配层遵循仓库原有 GPLv3。

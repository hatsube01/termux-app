#!/data/data/com.termux/files/usr/bin/bash
# A4K xMobox desktop launcher 0.4.2, embedded in the standalone APK.
# Derived from the user's startx11 (2026-11-17); does not source/overwrite it.
# --switch explicitly authorizes restarting recognized X11 servers/clients.
# Save and close games before using --switch. This is not live-session transfer.
set -u

APP_ID=com.a4k.termuxx11
ACTIVITY=com.termux.x11.MainActivity
SWITCH=0
CREATED_SERVER=''
STATE_DIR=''
CREATED_TOKEN=''
INPUT_RESOLUTION=''

say() { printf '%s\n' "$*"; }
fail() { say "启动未完成：$*" >&2; return 1; }
android_pm() { /system/bin/pm "$@"; }
android_am() { /system/bin/am "$@"; }

help_text() {
    cat <<'HELP'
用法：startx11-a4k [--switch | --original | --check | --repair-panel | --help]
  无参数     启动 A4K 的 XFCE 桌面；占用显示时会停止操作并说明原因。
  --switch   保存并关闭游戏后使用：重新启动已识别的 X11 桌面到 A4K。
  --check    只检查环境、输入尺寸和桌面组件，不启动或结束服务。
  --repair-panel 恢复当前桌面图标栏，不结束游戏或显示服务。
  --original 保存并关闭游戏后使用：结束 A4K 服务，调用原来的 startx11。
启动后在桌面中照常打开游戏，右侧“渲染”选择原画/CAS/Anime4K/组合。
这不是把原版正在运行的桌面或游戏搬到 A4K。原版 startx11 文件保留。
HELP
}

# Only same-UID processes with these exact argv[0] names are eligible.
# Never use an unanchored pgrep "termux-x11" (it matches A4K and scripts too).
server_records() {
    local pid argv kind
    while IFS= read -r pid; do
        [[ "$pid" =~ ^[0-9]+$ ]] || continue
        argv=''
        IFS= read -r -d '' argv < "/proc/$pid/cmdline" 2>/dev/null || :
        case "$argv" in
            termux-x11|termux-x11\ :0|termux-x11\ :0\ *) kind=original ;;
            termux-x11-a4k|termux-x11-a4k\ *) kind=a4k ;;
            *) continue ;;
        esac
        printf '%s|%s|%s\n' "$kind" "$pid" "$argv"
    done < <(pgrep -u "$(id -u)" -f '^termux-x11([[:space:]]|$)|^termux-x11-a4k([[:space:]]|$)' 2>/dev/null || :)
}

identity() {
    local stat rest
    [[ -r "/proc/$1/stat" ]] || return 1
    stat=$(<"/proc/$1/stat")
    rest=${stat##*) }
    # fields after comm start at field 3; field 22 is the start time.
    local -a fields
    read -r -a fields <<< "$rest"
    [[ "${fields[0]-}" != Z ]] || return 1
    printf '%s' "${fields[19]-}"
}

stop_one() {
    local pid=$1 token=$2 current i
    current=$(identity "$pid") || return 0
    [[ -n "$token" && "$current" == "$token" ]] || return 0
    kill -TERM "$pid" 2>/dev/null || return 1
    for ((i=0;i<40;i++)); do
        current=$(identity "$pid") || return 0
        [[ "$current" == "$token" ]] || return 0
        sleep 0.1
    done
    fail "进程 $pid 尚未退出。请关闭对应桌面后重试；不会强制结束它。"
}

pick_probe() {
    if command -v xdpyinfo >/dev/null 2>&1; then PROBE=xdpyinfo
    elif command -v xprop >/dev/null 2>&1; then PROBE=xprop
    elif command -v xset >/dev/null 2>&1; then PROBE=xset
    else fail '缺少 xdpyinfo、xprop 或 xset，无法验证 X11 是否真正就绪。'; return 1
    fi
}

x_ready() {
    case "$PROBE" in
        xdpyinfo) timeout 2 xdpyinfo -display :0 >/dev/null 2>&1 ;;
        xprop) timeout 2 xprop -display :0 -root _NET_SUPPORTING_WM_CHECK >/dev/null 2>&1 ;;
        xset) timeout 2 env DISPLAY=:0 xset q >/dev/null 2>&1 ;;
    esac
}

wm_ready() {
    pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1 || return 1
    if command -v xprop >/dev/null 2>&1; then
        local info
        info=$(timeout 2 xprop -display :0 -root _NET_SUPPORTING_WM_CHECK 2>/dev/null) || return 1
        [[ "$info" =~ 0x[1-9a-fA-F][0-9a-fA-F]* ]] || return 1
    elif ! pgrep -u "$(id -u)" -x xfwm4 >/dev/null 2>&1; then
        return 1
    fi
}

check_environment() {
    local item
    : "${PREFIX:=/data/data/com.termux/files/usr}"
    : "${TMPDIR:=$PREFIX/tmp}"
    export PREFIX TMPDIR
    [[ "$PREFIX" == /data/data/com.termux/files/usr ]] || {
        fail '请在原来的 Termux 命令窗口运行，不要在 Wine、proot 或 A4K 黑屏窗口输入。'; return 1;
    }
    for item in startxfce4 pgrep timeout nohup flock; do
        command -v "$item" >/dev/null 2>&1 || { fail "缺少命令 $item。"; return 1; }
    done
    APK_PATH=$(android_pm path "$APP_ID" 2>/dev/null | sed -n 's/^package://p' | head -n 1)
    [[ -n "$APK_PATH" && -r "$APK_PATH" ]] || {
        fail '未找到已安装的 Termux:X11 A4K APK。'; return 1;
    }
    pick_probe || return 1
}

# Read the X server's framebuffer dimensions before stopping it. This is not
# Android's panel size and does not claim to discover a game's internal render size.
live_resolution() {
    local info=''
    if command -v xdpyinfo >/dev/null 2>&1; then
        info=$(timeout 2 xdpyinfo -display :0 2>/dev/null) || info=''
        info=$(printf '%s\n' "$info" | awk '/dimensions:/ {print $2; exit}')
    fi
    if [[ -z "$info" ]] && command -v xrandr >/dev/null 2>&1; then
        info=$(timeout 2 xrandr --display :0 --current 2>/dev/null) || info=''
        info=$(printf '%s\n' "$info" | sed -n 's/.*current \([0-9][0-9]*\) x \([0-9][0-9]*\),.*/\1x\2/p' | head -n 1)
    fi
    valid_resolution "$info" || return 1
    printf '%s' "$info"
}

valid_resolution() {
    [[ "$1" =~ ^([0-9]{2,5})x([0-9]{2,5})$ ]] || return 1
    ((10#${BASH_REMATCH[1]} >= 64 && 10#${BASH_REMATCH[2]} >= 64 &&
      10#${BASH_REMATCH[1]} <= 16384 && 10#${BASH_REMATCH[2]} <= 16384))
}

resolve_resolution() {
    [[ "${A4K_AUTO_RESOLUTION-1}" == 1 ]] || return 0
    local size='' source='' cache="$HOME/.local/state/a4k-x11/input-resolution"
    if valid_resolution "${A4K_INHERIT_RESOLUTION-}"; then
        size=$A4K_INHERIT_RESOLUTION
        source='原版 X11 保存的显示设置'
    elif size=$(live_resolution); then
        source='当前 X11 桌面'
    elif [[ -r "$cache" ]]; then
        IFS= read -r size < "$cache" || :
        valid_resolution "$size" || size=''
        source='上次实际识别的桌面（当前未运行）'
    fi
    if [[ -z "$size" ]]; then
        fail '原版未返回可用的固定尺寸，当前 X11 和已识别记录也不可用。请分享检查结果；不会猜测尺寸或要求输入命令。'
        return 1
    fi
    INPUT_RESOLUTION=$size
    mkdir -p "${cache%/*}" || return 1
    printf '%s\n' "$size" > "$cache.new" && mv "$cache.new" "$cache" || return 1
    say "输入分辨率：$size；来源：$source。"
    say "A4K_INPUT_RESOLUTION_v1=$size"
}

show_check() {
    say "A4K 桌面连接检查 0.4.2"
    say "APK：$APK_PATH"
    say "桌面：$(command -v startxfce4)"
    say "目标显示：:0；临时目录：$TMPDIR；探测工具：$PROBE"
    say '已识别的 X11 服务（类型|PID|名称）：'
    server_records
    say '桌面组件（当前显示 :0）：'
    for component in xfce4-panel plank cairo-dock tint2; do
        component_running "$component" && say "  $component 正在运行" || :
    done
    if x_ready; then say '显示 :0 已可连接。'; else say '显示 :0 当前未就绪。'; fi
    resolve_resolution || return 1
    if pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1; then
        say '检测到正在运行的 XFCE 会话。'
    fi
}

prepare_servers() {
    local records kind pid argv own_zero='' token i
    local -a pids=() tokens=()
    records=$(server_records)
    while IFS='|' read -r kind pid argv; do
        [[ -n "$pid" ]] || continue
        if [[ "$kind" == a4k && "$argv" == 'termux-x11-a4k :0' ]]; then own_zero=$pid; fi
        pids+=("$pid"); tokens+=("$(identity "$pid" || :)")
    done <<< "$records"
    if [[ -n "$own_zero" ]] && (( ! SWITCH )) && x_ready; then
        say "复用已经就绪的 A4K :0 服务（PID $own_zero）。"
        return 0
    fi
    if ((${#pids[@]})) && (( ! SWITCH )); then
        fail '已有 X11 服务。请先保存并关闭游戏，再运行 startx11-a4k --switch。'
        return 1
    fi
    if x_ready && [[ "$records" != *'original|'* && -z "$own_zero" ]]; then
        fail '显示 :0 被未识别的服务占用；不会结束该服务。'
        return 1
    fi
    if ((${#pids[@]})); then
        say '正在结束已识别的旧 X11 服务；旧桌面将关闭。'
        for i in "${!pids[@]}"; do stop_one "${pids[$i]}" "${tokens[$i]}" || return 1; done
    fi
    if x_ready; then fail '显示 :0 仍被占用。'; return 1; fi
    for ((i=0;i<50;i++)); do
        pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1 || break
        sleep 0.1
    done
    if pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1; then
        fail '旧 XFCE 会话尚未退出，请在原桌面注销后再运行。'; return 1
    fi
    # Let Xserver validate/remove stale locks itself. Do not remove live sockets.
    start_server
}

start_server() {
    local i
    say '正在启动 A4K X11 :0…'
    nohup bash -c '
        export XSTARTUP_LD_LIBRARY_PATH="${LD_LIBRARY_PATH-}" XSTARTUP_LD_PRELOAD="${LD_PRELOAD-}"
        export XSTARTUP_CLASSPATH="${CLASSPATH-}"
        unset LD_LIBRARY_PATH LD_PRELOAD
        export CLASSPATH="$1"
        args=(:0)
        [[ -z "$2" ]] || args+=(-geometry "$2")
        case "$3" in builtin|external|dex) args+=(-output-name "$3") ;; esac
        exec /system/bin/app_process / --nice-name="termux-x11-a4k :0" com.termux.x11.CmdEntryPoint "${args[@]}"
    ' a4k-server "$APK_PATH" "$INPUT_RESOLUTION" "${A4K_OUTPUT_NAME-builtin}" >"$STATE_DIR/server.log" 2>&1 </dev/null &
    CREATED_SERVER=$!
    CREATED_TOKEN=$(identity "$CREATED_SERVER" || :)
    printf '%s\n' "$CREATED_SERVER" >"$STATE_DIR/server.pid"
    for ((i=0;i<60;i++)); do
        kill -0 "$CREATED_SERVER" 2>/dev/null || {
            fail "A4K 服务启动失败，见 $STATE_DIR/server.log"; return 1;
        }
        x_ready && return 0
        sleep 0.25
    done
    fail "等待 X11 就绪超时，见 $STATE_DIR/server.log"
}

start_support_services() {
    # Preserve the uploaded script's audio/MIDI/graphics settings. No tmp purge.
    if command -v pulseaudio >/dev/null 2>&1 && ! pgrep -x pulseaudio >/dev/null 2>&1; then
        pulseaudio --start --load='module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1' \
            --exit-idle-time=-1 >>"$STATE_DIR/support.log" 2>&1 || say '提示：PulseAudio 未能启动，详情见 support.log。'
    fi
    if [[ -f "$PREFIX/glibc/opt/virgl/libvirgl_test_server.so" ]] && ! pgrep -f 'libvirgl_test_server[.]so' >/dev/null 2>&1; then
        nohup "$PREFIX/glibc/opt/virgl/libvirgl_test_server.so" >>"$STATE_DIR/support.log" 2>&1 </dev/null &
    fi
    if [[ -f "$PREFIX/midi_server/midi_server" ]] && ! pgrep -f "$PREFIX/midi_server/midi_server" >/dev/null 2>&1; then
        nohup "$PREFIX/midi_server/midi_server" >>"$STATE_DIR/support.log" 2>&1 </dev/null &
    fi
    (
        export LD_LIBRARY_PATH="/vendor/lib64:/system/lib64:${LD_LIBRARY_PATH-}"
        if command -v virgl_test_server >/dev/null 2>&1 && ! pgrep -f 'virgl_test_server' >/dev/null 2>&1; then
            nohup env ANDROID_VENUS= virgl_test_server --venus --no-virgl \
                --socket-path="$PREFIX/tmp/virgl.sock" >>"$STATE_DIR/support.log" 2>&1 </dev/null &
        fi
        local path_conf="$PREFIX/glibc/opt/default-conf/conf/path.conf"
        local vortek_conf="$PREFIX/glibc/opt/default-conf/conf/vortek.conf"
        local driver='' version='' pid token
        local -a args=()
        if [[ -f "$path_conf" ]] && grep -q 'export VK_ICD_FILENAMES=.*vortek_icd.aarch64.json' "$path_conf"; then
            if [[ -f "$vortek_conf" ]]; then
                driver=$(sed -n 's/.*VORTEK_DRIVER_PATH="\([^"]*\)".*/\1/p' "$vortek_conf" | head -n 1)
                version=$(sed -n 's/.*VORTEK_VK_VERSION="\([^"]*\)".*/\1/p' "$vortek_conf" | head -n 1)
            fi
            if command -v vortekrenderer-cli >/dev/null 2>&1; then
                while IFS= read -r pid; do
                    token=$(identity "$pid" || :)
                    stop_one "$pid" "$token" || exit 1
                done < <(pgrep -u "$(id -u)" -f '(^|/)vortekrenderer-cli([[:space:]]|$)' || :)
                [[ -n "$driver" && -f "$driver" ]] && args+=(-v "$driver")
                [[ -n "$version" ]] && args+=(--vk-version "$version")
                nohup env DISPLAY=:0 VORTEK_CLI_X11_DRI3=1 VORTEK_CLI_X11_WSI=1 \
                    LD_PRELOAD=/system/lib64/libandroid.so vortekrenderer-cli "${args[@]}" \
                    >>"$STATE_DIR/support.log" 2>&1 </dev/null &
            else say '提示：配置选择了 Vortek，但未找到 vortekrenderer-cli。'; fi
        fi
        if command -v virgl_test_server >/dev/null 2>&1 && ! pgrep -f 'virgl_test_server.*venus' >/dev/null 2>&1; then
            nohup env LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$HOME/vk/adreno_driver" \
                VK_ICD_FILENAMES="$PREFIX/share/vulkan/icd.d/sysvk_icd.json" \
                VK_OVERRIDE="$HOME/vk/adreno_driver/libvulkan_freedreno.so" ANDROID_VENUS= \
                virgl_test_server --venus --no-virgl --socket-path="$PREFIX/tmp/virgl.sock" \
                >>"$STATE_DIR/support.log" 2>&1 </dev/null &
        fi
    )
}

# Select only the user's :0 desktop session. Do not terminate panel processes or
# replace ~/.config/xfce4/panel; the existing icon layout remains authoritative.
process_on_display() {
    local pid=$1 item display='' bus=''
    while IFS= read -r -d '' item; do
        case "$item" in DISPLAY=*) display=${item#DISPLAY=} ;; DBUS_SESSION_BUS_ADDRESS=*) bus=${item#DBUS_SESSION_BUS_ADDRESS=} ;; esac
    done < "/proc/$pid/environ" 2>/dev/null || :
    [[ "$display" == :0 || "$display" == :0.0 ]] || return 1
    [[ -z "${A4K_PANEL_BUS-}" || "$bus" == "$A4K_PANEL_BUS" ]]
}

component_running() {
    local pid
    while IFS= read -r pid; do
        [[ "$pid" =~ ^[0-9]+$ ]] && process_on_display "$pid" && return 0
    done < <(pgrep -u "$(id -u)" -x "$1" 2>/dev/null || :)
    return 1
}

record_desktop_components() {
    local kind found='' cache="$HOME/.local/state/a4k-x11/dock-components"
    for kind in xfce4-panel plank cairo-dock tint2; do
        component_running "$kind" && found+="$kind"$'\n' || :
    done
    [[ -n "$found" ]] || return 0
    mkdir -p "${cache%/*}" || return 1
    printf '%s' "$found" > "$cache.new" && mv "$cache.new" "$cache"
}

use_desktop_bus() {
    local pid item display bus
    while IFS= read -r pid; do
        display='';bus=''
        while IFS= read -r -d '' item; do
            case "$item" in DISPLAY=*) display=${item#DISPLAY=} ;; DBUS_SESSION_BUS_ADDRESS=*) bus=${item#DBUS_SESSION_BUS_ADDRESS=} ;; esac
        done < "/proc/$pid/environ" 2>/dev/null || :
        if [[ ( "$display" == :0 || "$display" == :0.0 ) && -n "$bus" ]]; then
            export DBUS_SESSION_BUS_ADDRESS=$bus A4K_PANEL_BUS=$bus
            return 0
        fi
    done < <(pgrep -u "$(id -u)" -x xfce4-session 2>/dev/null || :)
    return 1
}

ensure_desktop_panel() (
    local repair=${1-0} kind i missing=0 kinds='' cache="$HOME/.local/state/a4k-x11/dock-components"
    export DISPLAY=:0
    use_desktop_bus || { say '提示：尚未取得当前 XFCE 的 D-Bus，会用现有会话环境尝试图标栏。'; }
    if [[ -r "$cache" ]]; then kinds=$(<"$cache"); fi
    if [[ -z "$kinds" ]]; then
        for kind in xfce4-panel plank cairo-dock tint2; do
            component_running "$kind" && kinds+="$kind"$'\n' || :
        done
    fi
    if [[ -z "$kinds" ]] && command -v xfce4-panel >/dev/null 2>&1; then kinds=xfce4-panel; fi
    [[ -n "$kinds" ]] || { say '图标栏：未找到已记录或可用的桌面组件。'; return 1; }
    while IFS= read -r kind; do
        case "$kind" in xfce4-panel|plank|cairo-dock|tint2) ;; '') continue ;; *) say '图标栏：忽略未知组件记录。'; continue ;; esac
        if ! command -v "$kind" >/dev/null 2>&1; then say "图标栏：缺少 $kind，保留原配置。";missing=1;continue;fi
        if component_running "$kind"; then
            if [[ "$repair" == 1 && "$kind" == xfce4-panel ]]; then
                nohup xfce4-panel --restart >>"$STATE_DIR/panel.log" 2>&1 </dev/null &
                sleep 0.3
            fi
        else
            say "正在启动原桌面图标栏组件：$kind"
            nohup "$kind" >>"$STATE_DIR/panel.log" 2>&1 </dev/null &
        fi
        for ((i=0;i<30;i++)); do component_running "$kind" && break;sleep 0.1;done
        if component_running "$kind"; then say "图标栏组件运行中：$kind（沿用现有布局）";
        else say "图标栏组件未就绪：$kind，见 panel.log。";missing=1;fi
    done <<< "$kinds"
    ((missing==0))
)

repair_desktop_panel() {
    x_ready && wm_ready || { fail '当前 X11 / XFCE 桌面尚未就绪，无法恢复图标栏。'; return 1; }
    ensure_desktop_panel 1 || return 1
    say 'A4K_PANEL_READY_v1'
}

open_app() {
    [[ "${A4K_LAUNCHER_NO_OPEN-0}" != 1 ]] || return 0
    local result
    result=$(android_am start --user 0 -n "$APP_ID/$ACTIVITY" 2>&1)
    printf '%s\n' "$result" >"$STATE_DIR/app.log"
    if [[ "$result" == *'Error'* || "$result" == *'Exception'* ]]; then
        say '提示：系统没有自动切换界面，请手动打开“Termux:X11 A4K”。'
    fi
}

launch_desktop() {
    local i
    export DISPLAY=:0 XDG_SESSION_TYPE=x11 GDK_BACKEND=x11 TERMUX_X11_FORCE_FLIP=1
    export XDG_RUNTIME_DIR="$TMPDIR"
    export PULSE_SERVER=127.0.0.1 PULSE_LATENCY_MSEC=60 MIDI_SOCKET_PORT=43456 MALLOC_CHECK_=0
    # Must set these before spawning Android's X11 process as well as XFCE.
    resolve_resolution || return 1
    record_desktop_components || return 1
    prepare_servers || return 1
    mkdir -p "$PREFIX/var/run"
    printf 'export DISPLAY=:0\n' >"$PREFIX/var/run/tx11-xfce4.display"
    if wm_ready; then
        say 'XFCE 桌面已经就绪。'
        ensure_desktop_panel || say '提示：图标栏未完整就绪，可点恢复桌面图标栏并分享日志。'
        open_app
        return 0
    fi
    start_support_services || { fail '辅助图形服务启动失败，见 support.log。'; return 1; }
    say 'X11 已就绪，正在启动你的 XFCE 桌面…'
    (
        unset SESSION_MANAGER DBUS_SESSION_BUS_ADDRESS
        if command -v dbus-launch >/dev/null 2>&1; then
            exec nohup dbus-launch --exit-with-session startxfce4
        else exec nohup startxfce4
        fi
    ) >"$STATE_DIR/desktop.log" 2>&1 </dev/null &
    printf '%s\n' "$!" >"$STATE_DIR/desktop.pid"
    for ((i=0;i<80;i++)); do
        if x_ready && wm_ready; then
            ensure_desktop_panel || say '提示：图标栏未完整就绪，可点恢复桌面图标栏并分享日志。'
            say 'A4K 桌面已就绪。像平时一样在桌面打开游戏，再点右侧“渲染”。'
            open_app
            return 0
        fi
        sleep 0.25
    done
    fail "XFCE 桌面未能就绪。请发送 $STATE_DIR/desktop.log 和 server.log。"
}

cleanup() {
    local result=$?
    if (( result != 0 )) && [[ -n "$CREATED_SERVER" ]]; then
        # Only the X11 process created by this invocation, never Wine by name.
        if [[ -n "$CREATED_TOKEN" && "$(identity "$CREATED_SERVER" || :)" == "$CREATED_TOKEN" ]]; then
            kill -TERM "$CREATED_SERVER" 2>/dev/null || :
        fi
    fi
    return "$result"
}

restore_original() {
    local kind pid argv token i original_running=0 records
    command -v startx11 >/dev/null 2>&1 || { fail '找不到原版 startx11。'; return 1; }
    say '正在结束 A4K 显示服务，随后重新启动原版桌面。'
    records=$(server_records)
    while IFS='|' read -r kind pid argv; do
        [[ "$kind" != original ]] || original_running=1
        [[ "$kind" == a4k ]] || continue
        token=$(identity "$pid" || :)
        stop_one "$pid" "$token" || return 1
    done <<< "$records"
    if ((original_running)); then
        android_am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity
        return $?
    fi
    for ((i=0;i<50;i++)); do
        pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1 || break
        sleep 0.1
    done
    if pgrep -u "$(id -u)" -x xfce4-session >/dev/null 2>&1; then
        fail '仍有 XFCE 会话，请注销桌面后再点原版 STARTX11。'; return 1
    fi
    startx11
}

main() {
    local action=start self dest result
    case "${1-}" in
        '') ;;
        --switch) SWITCH=1 ;;
        --original) action=original ;;
        --check) action=check ;;
        --repair-panel) action=panel ;;
        --help|-h) help_text; return 0 ;;
        *) help_text; return 2 ;;
    esac
    (( $# <= 1 )) || { help_text; return 2; }
    if [[ "$action" == original ]]; then restore_original; return $?; fi
    check_environment || return 1
    [[ "$action" != check ]] || { show_check; return $?; }
    STATE_DIR="$HOME/.local/state/a4k-x11"
    mkdir -p "$STATE_DIR" || return 1
    exec 9>"$STATE_DIR/launcher.lock"
    flock -n 9 || { fail '另一个 A4K 启动操作正在进行，请稍等。'; return 1; }
    # Close lock FD in background children so the next launch can acquire it.
    self=$(readlink -f "${BASH_SOURCE[0]}")
    dest="$PREFIX/bin/startx11-a4k"
    if [[ "$self" != "$dest" ]]; then
        cp "$self" "$dest.new" && chmod 755 "$dest.new" && mv "$dest.new" "$dest" || return 1
    fi
    trap cleanup EXIT
    if [[ "$action" == panel ]]; then
        repair_desktop_panel 9>&- > >(tee "$STATE_DIR/launcher.log") 2>&1
    else
        launch_desktop 9>&- > >(tee "$STATE_DIR/launcher.log") 2>&1
    fi
    result=$?
    flock -u 9
    return "$result"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then main "$@"; exit $?; fi

#!/system/bin/sh

binder_ready() {
    status=$(/system/bin/toybox timeout 4 /system/bin/service check "$1")
    case "$status" in
        *": found") printf '1' ;;
        *) printf '0' ;;
    esac
}

resolve_launcher() {
    /system/bin/toybox timeout -k 2 10 /system/bin/cmd package resolve-activity --brief --user 0 -a android.intent.action.MAIN -c android.intent.category.HOME -p com.android.launcher3 |
            /system/bin/toybox grep -E '^[a-zA-Z][a-zA-Z0-9_.]+/' |
            /system/bin/toybox tail -n 1
}

alloc_probe=$(/system/bin/toybox timeout 28 /system/bin/klimt-vulkan-probe 2>&1)
trial_result=$?
if [ -n "$alloc_probe" ]; then
    printf '%s\n' "$alloc_probe" | /system/bin/toybox head -n 3 | (
        slot=30
        while IFS= read -r line; do
            slot=$((slot + 1))
            detail=$(printf '%s' "$line" | /system/bin/toybox cut -c1-74)
            /system/bin/setprop "debug.klimt_diag_$slot" "GRALLOC $detail"
        done
    )
fi
/system/bin/setprop debug.klimt_diag_36 "GRALLOC exit=$trial_result"

property=/system/bin/setprop
timeout() {
    /system/bin/toybox timeout -k 2 "$@"
}

cycle=0
while [ "$cycle" -lt 60 ]; do
    cycle=$((cycle + 1))
    /system/bin/toybox sleep 8
    boot=$(/system/bin/getprop sys.boot_completed)
    activity=$(binder_ready activity)
    package=$(binder_ready package)
    window=$(binder_ready window)
    display=$(binder_ready SurfaceFlinger)
    "$property" debug.klimt_home "step=$cycle,activity=$activity,package=$package,window=$window,sf=$display,boot=${boot:-unset}"
    [ "$boot" = 1 ] || continue
    if [ "$(/system/bin/id -u)" != 2000 ] ||
        ! /system/bin/awk '$5 == "/data" { for (i=6; i<NF; i++) if ($i == "-") { type=$(i+1); break } }
            END { exit type != "tmpfs" }' /proc/self/mountinfo; then
        "$property" debug.klimt_diag_31 'RAM_SETUP guard failed'
        exit 1
    fi
    if [ "${unlocked_observed:-0}" = 0 ]; then
        userstate=$(timeout 12 /system/bin/am get-started-user-state 0 2>/dev/null)
        "$property" debug.klimt_user "${userstate:-unreported},ce=$(/system/bin/getprop sys.user.0.ce_available)"
        [ "$userstate" = RUNNING_UNLOCKED ] || continue
        unlocked_observed=1
    fi
    if [ "${overlay_checked:-0}" = 0 ]; then
        overlay=$(timeout 18 /system/bin/cmd overlay lookup --user 0 android android:bool/config_displayColorFadeDisabled 2>/dev/null)
        "$property" debug.klimt_diag_34 "COLORFADE_DISABLED ${overlay:-unreported}"
        if [ "$overlay" = true ]; then
            overlay_checked=1
        fi
    fi
    if [ "${home_profile_ready:-0}" = 0 ]; then
        timeout 14 /system/bin/settings --user 0 put system screen_off_timeout 900000
        timeout 14 /system/bin/settings put global device_provisioned 1
        timeout 14 /system/bin/settings --user 0 put secure user_setup_complete 1
        provisioned=$(timeout 16 /system/bin/settings get global device_provisioned)
        user_setup=$(timeout 16 /system/bin/settings --user 0 get secure user_setup_complete)
        "$property" debug.klimt_diag_31 "RAM_SETUP device=${provisioned:-unset},user=${user_setup:-unset}"
        if [ "$provisioned" = 1 ] && [ "$user_setup" = 1 ]; then
            timeout 12 /system/bin/am force-stop --user 0 com.google.android.setupwizard
            home_profile_ready=1
        fi
    fi
    home=com.android.launcher3/.uioverrides.QuickstepLauncher
    activity_details=$(timeout 18 /system/bin/dumpsys -t 15 activity activities 2>/dev/null)
    resumed=$(printf '%s\n' "$activity_details" | /system/bin/awk '
        /topResumedActivity=.*ActivityRecord/ {
            for (i=1; i<NF; i++) if ($i == "u0") { print $(i+1); exit }
        }')
    case "$resumed" in
        com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) resumed=$home ;;
    esac
    "$property" debug.klimt_resumed "${resumed:-unreported}"
    if [ "$resumed" != "$home" ]; then
        candidate=$(resolve_launcher)
        "$property" debug.klimt_diag_32 "HOME_RESOLVE ${candidate:-none}"
        case "$candidate" in
            com.android.launcher3/.uioverrides.QuickstepLauncher|com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher)
                timeout 16 /system/bin/am start --user 0 -a android.intent.action.MAIN \
                    -c android.intent.category.HOME -p com.android.launcher3 2>&1 |
                    /system/bin/toybox tail -n 1 | /system/bin/toybox cut -c1-70 |
                    { IFS= read -r launch; "$property" debug.klimt_diag_33 "HOME_START ${launch:-unreported}"; }
                ;;
        esac
        continue
    fi
    window_details=$(timeout 15 /system/bin/dumpsys -t 12 window displays 2>/dev/null)
    focus=$(printf '%s\n' "$window_details" | /system/bin/awk '
        /mCurrentFocus=/ {
            for (i=1; i<NF; i++) if ($i == "u0") {
                sub(/}.*/, "", $(i+1)); print $(i+1); exit
            }
        }')
    case "$focus" in
        com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) focus=$home ;;
    esac
    "$property" debug.klimt_focus "${focus:-unreported}"
    if [ "$focus" != "$home" ] && [ "${wake_attempts:-0}" -lt 2 ]; then
        timeout 10 /system/bin/input keyevent KEYCODE_WAKEUP
        timeout 10 /system/bin/wm dismiss-keyguard
        wake_attempts=$((${wake_attempts:-0} + 1))
    fi
    physical=$(timeout 15 /system/bin/dumpsys -t 12 SurfaceFlinger --display-id |
        /system/bin/toybox grep -m 1 -E '^Display [0-9]+ \(HWC display 0\)' |
        /system/bin/toybox cut -c1-80)
    active=$(timeout 15 /system/bin/dumpsys -t 12 SurfaceFlinger --displays |
        /system/bin/toybox grep -m 1 -Ei 'powerMode[ :=]+ON|power mode[ :=]+ON' |
        /system/bin/toybox cut -c1-86)
    "$property" debug.klimt_display_hwc "${physical:-unreported}"
    "$property" debug.klimt_display_state "${active:-unreported}"
    [ "$focus" = "$home" ] && [ -n "$physical" ] && [ -n "$active" ] || continue
    if [ "$(/system/bin/getprop init.svc.bootanim)" = running ] &&
        [ "$(/system/bin/getprop service.bootanim.exit)" != 1 ]; then
        continue
    fi
    display_id=${physical#Display }
    display_id=${display_id%% *}
    capture=/data/local/tmp/klimt-home.raw
    if timeout 25 /system/bin/screencap -d "$display_id" "$capture" &&
        timeout 12 /system/bin/klimt-frame-probe "$capture"; then
        details=$(timeout 8 /system/bin/logcat -d -v raw -s KlimtFrame:I |
            /system/bin/toybox tail -n 1)
        case "$details" in
            capture=2560x1600,format=*,lit=[1-9]*|capture=1600x2560,format=*,lit=[1-9]*)
                "$property" debug.klimt_home_capture "$details"
                /system/bin/toybox rm -f "$capture"
                break ;;
        esac
    fi
    "$property" debug.klimt_home_capture failed
    /system/bin/toybox rm -f "$capture"
done

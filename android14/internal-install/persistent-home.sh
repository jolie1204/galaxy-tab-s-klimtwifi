#!/system/bin/sh

# One-shot verification of the installed system using the existing Android profile.
setprop=/system/bin/setprop
home=com.android.launcher3/.uioverrides.QuickstepLauncher
timeout() {
    /system/bin/toybox timeout -k 2 "$@"
}

# First-stage init verifies the partition identity before mounting this private node.
if ! /system/bin/awk '
    $2 == "/data" && $1 == "/dev/klimt-userdata" &&
    $3 == "ext4" && $4 ~ /(^|,)rw(,|$)/ { known=1 }
    END { exit !known }
' /proc/mounts; then
    "$setprop" debug.klimt_diag_35 install_data_unverified
    exit 1
fi

cycle=0
while [ "$cycle" -lt 60 ]; do
    cycle=$((cycle + 1))
    /system/bin/toybox sleep 8
    boot=$(/system/bin/getprop sys.boot_completed)
    "$setprop" debug.klimt_home "step=$cycle,activity=0,package=0,window=0,sf=0,boot=${boot:-unset}"
    [ "$boot" = 1 ] || continue
    if [ "${unlocked:-0}" = 0 ]; then
        state=$(timeout 12 /system/bin/am get-started-user-state 0 2>/dev/null)
        "$setprop" debug.klimt_user "${state:-unreported}"
        [ "$state" = RUNNING_UNLOCKED ] || continue
        unlocked=1
    fi
    activity=$(timeout 16 /system/bin/dumpsys -t 13 activity activities 2>/dev/null)
    resumed=$(printf '%s\n' "$activity" | /system/bin/awk '
        /topResumedActivity=.*ActivityRecord/ {
            for (i=1; i<NF; i++) if ($i == "u0") { print $(i+1); exit }
        }')
    case "$resumed" in
        com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) resumed=$home ;;
    esac
    "$setprop" debug.klimt_resumed "${resumed:-unreported}"
    [ "$resumed" = "$home" ] || continue

    windows=$(timeout 16 /system/bin/dumpsys -t 13 window displays 2>/dev/null)
    focus=$(printf '%s\n' "$windows" | /system/bin/awk '
        /mCurrentFocus=/ {
            for (i=1; i<NF; i++) if ($i == "u0") {
                sub(/}.*/, "", $(i+1)); print $(i+1); exit
            }
        }')
    case "$focus" in
        com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) focus=$home ;;
    esac
    "$setprop" debug.klimt_focus "${focus:-unreported}"
    [ "$focus" = "$home" ] || continue

    physical=$(timeout 14 /system/bin/dumpsys -t 12 SurfaceFlinger --display-id |
        /system/bin/toybox grep -m 1 -E '^Display [0-9]+ \(HWC display 0\)' |
        /system/bin/toybox cut -c1-80)
    display=$(timeout 14 /system/bin/dumpsys -t 12 SurfaceFlinger --displays |
        /system/bin/toybox grep -m 1 -Ei 'powerMode[ :=]+ON|power mode[ :=]+ON' |
        /system/bin/toybox cut -c1-86)
    "$setprop" debug.klimt_display_hwc "${physical:-unreported}"
    "$setprop" debug.klimt_display_state "${display:-unreported}"
    if [ -z "$display" ] && [ "${woke:-0}" = 0 ]; then
        timeout 10 /system/bin/input keyevent KEYCODE_WAKEUP
        woke=1
    fi
    [ -n "$physical" ] && [ -n "$display" ] || continue

    display_id=${physical#Display }
    display_id=${display_id%% *}
    capture=/data/local/tmp/klimt-installed-home.raw
    if timeout 55 /system/bin/screencap -q -d "$display_id" "$capture" &&
        timeout 12 /system/bin/klimt-frame-probe "$capture"; then
        details=$(timeout 10 /system/bin/logcat -d -v raw -s KlimtFrame:I |
            /system/bin/toybox tail -n 1)
        case "$details" in
            capture=640x400,format=*,lit=[1-9]*|capture=400x640,format=*,lit=[1-9]*)
                /system/bin/toybox rm -f "$capture"
                "$setprop" debug.klimt_home_capture "$details"
                "$setprop" debug.klimt_diag_35 install_ready
                exit 0 ;;
        esac
    fi
    /system/bin/toybox rm -f "$capture"
done
exit 1

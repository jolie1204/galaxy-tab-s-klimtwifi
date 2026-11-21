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

cycle=0
while [ "$cycle" -lt 50 ]; do
    cycle=$((cycle + 1))
    /system/bin/toybox sleep 12
    activity=$(binder_ready activity)
    package=$(binder_ready package)
    window=$(binder_ready window)
    display=$(binder_ready SurfaceFlinger)
    boot=$(/system/bin/getprop sys.boot_completed)
    if [ "$activity" = 1 ]; then
        saw_activity=1
    fi
    if [ "$activity" = 0 ] && [ "${saw_activity:-0}" = 1 ]; then
        runtime=$(/system/bin/toybox pidof system_server)
        gate=$(/system/bin/getprop init.svc.gatekeeperd)
        /system/bin/setprop debug.klimt_runtime "system_server=${runtime:-absent},gatekeeperd=${gate:-unset}"
        if [ -z "$runtime" ] && [ "${seen_crash:-0}" = 0 ]; then
            /system/bin/toybox timeout 8 /system/bin/logcat -b main -b system -d -v brief -s \
                PermissionControllerManager:E ServiceConnector:W | \
                /system/bin/toybox grep -E 'permission|TimeoutException|CancellationException|Failed to bind|DeadObject' | \
                /system/bin/toybox tail -n 6 | (
                    slot=0
                    while IFS= read -r line; do
                        slot=$((slot + 1))
                        detail=$(printf '%s' "$line" | /system/bin/toybox cut -c1-88)
                        /system/bin/setprop "debug.klimt_perm_$slot" "$detail"
                    done
                )
            /system/bin/toybox timeout 15 /system/bin/logcat -b main -b system -b crash -b events -d -v brief -s \
                AndroidRuntime:E SystemServer:E PermissionControllerManager:E ServiceConnector:W PermissionPolicyService:E \
                PermissionControllerServiceImpl:W RuntimePermissionsUpgradeController:W \
                Zygote:W libc:F DEBUG:F StorageManagerService:E vold:E installd:E \
                Watchdog:W watchdog:I system_server:F GenerationRegistry:E \
                ActivityManager:I am_proc_start:I am_proc_start_timeout:I | \
                /system/bin/awk '\
                    /[[:space:]]at [[:alnum:]]/ && !/GnssNative|LocationManagerService|WallpaperLocalColorExtractor|ImageWallpaper|MediaControlPanel|PermissionPolicyService/ { next }\
                    /Shutting down VM|linker namespace/ { next }\
                    /F\/system_server/ && !/Runtime aborting|Abort message|Check failed/ { next }\
                    /F\/DEBUG/ && !/Cmdline|signal |Cause:|Abort message:|#0[0-2] pc/ { next }\
                    seen[$0]++ { next }\
                    /ActivityManager|am_proc_start/ && !/permissioncontroller|PermissionController|failed to attach|Forcing bringing down service|starting process/ { next }\
                    { print }' | \
                /system/bin/toybox tail -n 22 | (
                    i=0
                    while IFS= read -r line; do
                        [ "$i" -lt 36 ] || break
                        i=$((i + 1))
                        key=$(printf 'debug.klimt_diag_%02d' "$i")
                        detail=$(printf '%s' "$line" | /system/bin/toybox cut -c1-88)
                        /system/bin/setprop "$key" "$detail"
                        if [ "${#line}" -gt 88 ] && [ "$i" -lt 36 ]; then
                            i=$((i + 1))
                            key=$(printf 'debug.klimt_diag_%02d' "$i")
                            detail=$(printf '%s' "$line" | /system/bin/toybox cut -c89-172)
                            /system/bin/setprop "$key" "+$detail"
                        fi
                    done
                    if [ "$i" = 0 ]; then
                        /system/bin/setprop debug.klimt_diag_01 'No filtered exception records returned'
                    fi
                )
            /system/bin/toybox timeout 7 /system/bin/logcat -b main -b system -d -v brief -s Watchdog:W |
                /system/bin/awk '
                    /annotated stack trace:/ { active=1; frames=0; print; next }
                    /GOODBYE!/ { active=0 }
                    active && /[[:blank:]]at / && frames++ < 8 { print }
                ' | /system/bin/toybox tail -n 9 | (
                    slot=17
                    while IFS= read -r line; do
                        [ "$slot" -lt 35 ] || break
                        slot=$((slot + 1))
                        key=$(printf 'debug.klimt_diag_%02d' "$slot")
                        prefix=$(printf '%s' "$line" | /system/bin/toybox cut -c1-82)
                        /system/bin/setprop "$key" "WATCH $prefix"
                        if [ "${#line}" -gt 82 ]; then
                            slot=$((slot + 1))
                            key=$(printf 'debug.klimt_diag_%02d' "$slot")
                            suffix=$(printf '%s' "$line" | /system/bin/toybox cut -c83-166)
                            /system/bin/setprop "$key" "+$suffix"
                        fi
                    done
                )
            seen_crash=1
        fi
    fi
    phase=$(/system/bin/toybox timeout 8 /system/bin/logcat -b main -b system -d -t 1200 -v brief -s SystemServerTiming:D AndroidRuntime:E Watchdog:I |
        /system/bin/toybox grep -E 'SystemServerTiming|FATAL EXCEPTION|KILLING SYSTEM PROCESS|for reason: dexopt' |
        /system/bin/toybox tail -n 1 | /system/bin/toybox cut -c1-86)
    /system/bin/setprop debug.klimt_home "step=$cycle,activity=$activity,package=$package,window=$window,sf=$display,boot=${boot:-unset}"
    /system/bin/setprop debug.klimt_java "${phase:-unreported}"
    if [ "$activity" = 1 ] && [ $((cycle % 4)) -eq 0 ]; then
        if [ "${usb_seen:-0}" = 0 ] && [ "$(binder_ready usb)" = 1 ]; then
            /system/bin/setprop debug.klimt_diag_34 'USB framework Binder service registered'
            usb_seen=1
        fi
        userstate=$(/system/bin/toybox timeout 4 /system/bin/am get-started-user-state 0 2>/dev/null)
        available=$(/system/bin/getprop sys.user.0.ce_available)
        /system/bin/setprop debug.klimt_user "${userstate:-unreported},ce=${available:-unset}"
        flags=$(/system/bin/toybox timeout 5 /system/bin/dumpsys -t 4 window -a 2>/dev/null |
            /system/bin/awk '/mSystemReady=|mDisplayEnabled=|mBootAnimationStopped=|mKeyguardDrawComplete=/ {
                for (k=1; k<=NF; ++k) if ($k ~ /^m(SystemReady|SystemBooted|DisplayEnabled|KeyguardDrawComplete)=/) print $k
            }' | /system/bin/toybox head -n 4 | /system/bin/toybox tr '\n' ',' | /system/bin/toybox cut -c1-88)
        /system/bin/setprop debug.klimt_window_state "${flags:-unreported}"
        resumed=$(/system/bin/toybox timeout 4 /system/bin/dumpsys -t 3 activity activities |
            /system/bin/toybox grep -m 1 'topResumedActivity.*ActivityRecord' |
            /system/bin/toybox grep -oE '[a-zA-Z0-9_.]+/[a-zA-Z0-9_.$]+')
        /system/bin/setprop debug.klimt_resumed "${resumed:-unreported}"
        if [ "$cycle" -ge 8 ] && [ "$window" = 1 ] && [ "$display" = 1 ]; then
            focus=$(/system/bin/toybox timeout 5 /system/bin/dumpsys -t 4 window windows |
                /system/bin/toybox grep -m 1 'mCurrentFocus=' |
                /system/bin/toybox grep -oE '[a-zA-Z0-9_.]+/[a-zA-Z0-9_.$]+')
            /system/bin/setprop debug.klimt_focus "${focus:-unreported}"
            physical=$(/system/bin/toybox timeout 6 /system/bin/dumpsys SurfaceFlinger --display-id |
                /system/bin/toybox grep -m 1 '(HWC display ' | /system/bin/toybox cut -c1-80)
            active=$(/system/bin/toybox timeout 6 /system/bin/dumpsys SurfaceFlinger --displays |
                /system/bin/toybox grep -m 1 -Ei 'powerMode[ :=]+(ON|OFF)|power mode[ :=]+(ON|OFF)' |
                /system/bin/toybox cut -c1-86)
            /system/bin/setprop debug.klimt_display_hwc "${physical:-unreported}"
            /system/bin/setprop debug.klimt_display_state "${active:-unreported}"
        fi
        found=$(/system/bin/toybox pidof com.google.android.permissioncontroller com.android.permissioncontroller)
        /system/bin/setprop debug.klimt_permission_process "pid=${found:-absent}"
        events=$(/system/bin/toybox timeout 5 /system/bin/logcat -b events -d -t 450 -v raw -s \
            am_proc_start:I am_proc_bound:I am_proc_died:I am_anr:I am_create_service:I am_schedule_service_restart:I |
            /system/bin/toybox grep -i 'permissioncontroller' | /system/bin/toybox tail -n 8)
        if [ -n "$events" ] && [ "$events" != "${last_perm_events:-}" ]; then
            printf '%s\n' "$events" | (
                slot=0
                while IFS= read -r line; do
                    slot=$((slot + 1))
                    detail=$(printf '%s' "$line" | /system/bin/toybox cut -c1-88)
                    /system/bin/setprop "debug.klimt_perm_event_$slot" "$detail"
                done
            )
            last_perm_events=$events
        fi
    fi
    if [ "$cycle" -ge 16 ] && [ "$activity" = 1 ] && [ "${ui_captured:-0}" = 0 ]; then
        ui=$(/system/bin/toybox timeout 8 /system/bin/logcat -b crash -d -v brief -s AndroidRuntime:E |
            /system/bin/awk '\
                /Process: com.android.systemui,/ { active=1; i=0 }\
                active { lines[++i]=$0; if (i==7) active=0 }\
                END { if (i>2) for (j=1; j<=i; j++) print lines[j] }')
        if [ -n "$ui" ]; then
            i=0
            printf '%s\n' "$ui" | while IFS= read -r line; do
                i=$((i + 1))
                first=$(printf '%s' "$line" | /system/bin/toybox cut -c1-88)
                /system/bin/setprop "$(printf 'debug.klimt_ui_%02d' "$i")" "$first"
                if [ "${#line}" -gt 88 ]; then
                    i=$((i + 1))
                    more=$(printf '%s' "$line" | /system/bin/toybox cut -c89-172)
                    /system/bin/setprop "$(printf 'debug.klimt_ui_%02d' "$i")" "+$more"
                fi
            done
            /system/bin/toybox timeout 8 /system/bin/logcat -b main -b system -d -v brief -s \
                HWUI:W OpenGLRenderer:W VulkanManager:W libEGL:W SurfaceFlinger:E Gralloc4:W cros_gralloc:E gralloc4:E minigbm:E | \
                /system/bin/toybox tail -n 6 | (
                    slot=30
                    while IFS= read -r line; do
                        slot=$((slot + 1))
                        detail=$(printf '%s' "$line" | /system/bin/toybox cut -c1-82)
                        /system/bin/setprop "debug.klimt_diag_$slot" "GFX $detail"
                    done
                )
            ui_captured=1
        fi
    fi
    if [ "$boot" != 1 ] && [ "$cycle" -ge 8 ] &&
        [ "$activity" = 1 ] && [ "$window" = 1 ] && [ "${early_home:-0}" = 0 ]; then
        if /system/bin/toybox timeout 8 /system/bin/dumpsys window |
            /system/bin/toybox grep -m 1 -q 'mSystemReady=true mSystemBooted=true'; then
            candidate=$(resolve_launcher)
            case "$candidate" in
                com.android.launcher3/?*)
                    /system/bin/toybox timeout 10 /system/bin/am start --user 0 \
                        -a android.intent.action.MAIN -c android.intent.category.HOME -n "$candidate"
                    /system/bin/setprop debug.klimt_early_home "$candidate"
                    early_home=1
                    ;;
            esac
        fi
    fi
    if [ "$boot" = 1 ]; then
        if [ "$(/system/bin/id -u)" != 2000 ] ||
            ! /system/bin/awk '$5 == "/data" { for (i=6; i<NF; i++) if ($i == "-") { type=$(i+1); break } }
                END { exit type != "tmpfs" }' /proc/self/mountinfo; then
            /system/bin/setprop debug.klimt_diag_31 'RAM_SETUP guard failed'
            exit 1
        fi
        if [ "${home_profile_ready:-0}" = 0 ]; then
            userstate=$(/system/bin/toybox timeout -k 2 6 /system/bin/am get-started-user-state 0 2>/dev/null)
            /system/bin/setprop debug.klimt_user "${userstate:-unreported},ce=$(/system/bin/getprop sys.user.0.ce_available)"
            if [ "$userstate" = RUNNING_UNLOCKED ]; then
                # Keep this disposable profile awake through the bounded test.
                /system/bin/toybox timeout -k 2 6 /system/bin/settings --user 0 put system screen_off_timeout 900000
                screen_timeout=$(/system/bin/toybox timeout -k 2 6 /system/bin/settings --user 0 get system screen_off_timeout)
                /system/bin/setprop debug.klimt_diag_35 "RAM_SCREEN_TIMEOUT ${screen_timeout:-unset}"
                /system/bin/toybox timeout -k 2 6 /system/bin/settings put global device_provisioned 1 &&
                    /system/bin/toybox timeout -k 2 6 /system/bin/settings --user 0 put secure user_setup_complete 1
                provisioned=$(/system/bin/toybox timeout -k 2 6 /system/bin/settings get global device_provisioned)
                user_setup=$(/system/bin/toybox timeout -k 2 6 /system/bin/settings --user 0 get secure user_setup_complete)
                /system/bin/setprop debug.klimt_diag_31 "RAM_SETUP device=${provisioned:-unset},user=${user_setup:-unset}"
                if [ "$provisioned" = 1 ] && [ "$user_setup" = 1 ]; then
                    /system/bin/toybox timeout -k 2 6 /system/bin/am force-stop --user 0 com.google.android.setupwizard
                    home_profile_ready=1
                fi
            fi
        fi
        home=$(resolve_launcher)
        /system/bin/setprop debug.klimt_diag_32 "HOME_RESOLVE ${home:-none}"
        if [ "$(/system/bin/getprop init.svc.bootanim)" = running ] &&
            [ "$(/system/bin/getprop service.bootanim.exit)" != 1 ]; then
            continue
        fi
        resumed=$(/system/bin/toybox timeout -k 2 10 /system/bin/dumpsys activity activities |
            /system/bin/toybox grep -m 1 -E 'topResumedActivity.*ActivityRecord|mResumedActivity.*ActivityRecord' |
            /system/bin/toybox grep -oE '[a-zA-Z0-9_.]+/[a-zA-Z0-9_.$]+')
        /system/bin/setprop debug.klimt_resumed "${resumed:-unreported}"
        case "$home" in
            com.android.launcher3/.uioverrides.QuickstepLauncher) ;;
            com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher)
                home=com.android.launcher3/.uioverrides.QuickstepLauncher ;;
            *) continue ;;
        esac
        case "$resumed" in
            com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) resumed=$home ;;
        esac
        if [ "$resumed" != "$home" ]; then
            result=$(/system/bin/toybox timeout -k 2 12 /system/bin/am start --user 0 \
                -a android.intent.action.MAIN -c android.intent.category.HOME -p com.android.launcher3 2>&1)
            status=$?
            detail=$(printf '%s\n' "$result" | /system/bin/toybox tail -n 1 | /system/bin/toybox cut -c1-66)
            /system/bin/setprop debug.klimt_diag_33 "HOME_START code=$status ${detail:-unreported}"
            continue
        fi
        focus=$(/system/bin/toybox timeout -k 2 8 /system/bin/dumpsys window windows |
            /system/bin/toybox grep -m 1 'mCurrentFocus=' |
            /system/bin/toybox grep -oE '[a-zA-Z0-9_.]+/[a-zA-Z0-9_.$]+' |
            /system/bin/toybox head -n 1)
        case "$focus" in
            com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher) focus=$home ;;
        esac
        /system/bin/setprop debug.klimt_focus "${focus:-unreported}"
        physical=$(/system/bin/toybox timeout -k 2 10 /system/bin/dumpsys SurfaceFlinger --display-id |
            /system/bin/toybox grep -m 1 -E '^Display [0-9]+ \(HWC display 0\)' |
            /system/bin/toybox cut -c1-80)
        active=$(/system/bin/toybox timeout -k 2 10 /system/bin/dumpsys SurfaceFlinger --displays |
            /system/bin/toybox grep -m 1 -Ei 'powerMode[ :=]+ON|power mode[ :=]+ON' |
            /system/bin/toybox cut -c1-86)
        /system/bin/setprop debug.klimt_display_hwc "${physical:-unreported}"
        /system/bin/setprop debug.klimt_display_state "${active:-unreported}"
        if [ "$focus" != "$home" ] || [ -z "$physical" ] || [ -z "$active" ]; then
            if [ -z "$active" ]; then
                /system/bin/toybox timeout -k 2 8 /system/bin/input keyevent KEYCODE_WAKEUP
            fi
            continue
        fi
        display_id=${physical#Display }
        display_id=${display_id%% *}
        capture=/data/local/tmp/klimt-home.raw
        if /system/bin/toybox timeout -k 2 20 /system/bin/screencap -d "$display_id" "$capture" &&
            /system/bin/klimt-frame-probe "$capture"; then
            details=$(/system/bin/toybox timeout -k 2 5 /system/bin/logcat -d -v raw -s KlimtFrame:I |
                /system/bin/toybox tail -n 1)
            case "$details" in
                capture=2560x1600,format=*,lit=[1-9]*)
                    /system/bin/setprop debug.klimt_home_capture "$details"
                    /system/bin/toybox rm -f "$capture"
                    break
                    ;;
            esac
        fi
        /system/bin/setprop debug.klimt_home_capture failed
        /system/bin/toybox rm -f "$capture"
    fi
done

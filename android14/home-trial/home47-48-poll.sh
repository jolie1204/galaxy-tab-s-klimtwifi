#!/system/bin/sh

cycle=0
while [ "$cycle" -lt 50 ]; do
    cycle=$((cycle + 1))
    /system/bin/toybox sleep 12
    services=$(/system/bin/toybox timeout 8 /system/bin/service list)
    activity=0; package=0; window=0; display=0
    case "$services" in *"activity:"*) activity=1;; esac
    case "$services" in *"package:"*) package=1;; esac
    case "$services" in *"window:"*) window=1;; esac
    case "$services" in *"SurfaceFlinger:"*) display=1;; esac
    boot=$(/system/bin/getprop sys.boot_completed)
    if [ "$activity" = 0 ] && [ "$cycle" -ge 10 ]; then
        runtime=$(/system/bin/toybox pidof system_server)
        gate=$(/system/bin/getprop init.svc.gatekeeperd)
        /system/bin/setprop debug.klimt_runtime "system_server=${runtime:-absent},gatekeeperd=${gate:-unset}"
        if [ "${seen_crash:-0}" = 0 ]; then
            i=0
            /system/bin/toybox timeout 12 /system/bin/logcat -b crash -b system -d -v brief -s AndroidRuntime:E SystemServer:E Watchdog:E |
                /system/bin/toybox grep -E 'FATAL EXCEPTION|Failed to start service|Caused by:|Gatekeeper service not available|KILLING SYSTEM PROCESS|IllegalStateException' |
                /system/bin/toybox tail -n 4 | while IFS= read -r line; do
                    i=$((i + 1))
                    detail=$(/system/bin/toybox printf '%s' "$line" | /system/bin/toybox cut -c1-86)
                    /system/bin/setprop "debug.klimt_crash_$i" "$detail"
                done
            seen_crash=1
        fi
    fi
    phase=$(/system/bin/toybox timeout 8 /system/bin/logcat -b main -b system -d -t 1200 -v brief -s SystemServerTiming:D AndroidRuntime:E Watchdog:I |
        /system/bin/toybox grep -E 'SystemServerTiming|FATAL EXCEPTION|KILLING SYSTEM PROCESS|for reason: dexopt' |
        /system/bin/toybox tail -n 1 | /system/bin/toybox cut -c1-86)
    /system/bin/setprop debug.klimt_home "step=$cycle,activity=$activity,package=$package,window=$window,sf=$display,boot=${boot:-unset}"
    /system/bin/setprop debug.klimt_java "${phase:-unreported}"
    if [ "$boot" = 1 ]; then
        home=$(/system/bin/toybox timeout 10 /system/bin/cmd package resolve-activity --brief --user 0 -a android.intent.action.MAIN -c android.intent.category.HOME |
            /system/bin/toybox grep -E '^[a-zA-Z][a-zA-Z0-9_.]+/' |
            /system/bin/toybox tail -n 1)
        resumed=$(/system/bin/toybox timeout 10 /system/bin/dumpsys activity activities |
            /system/bin/toybox grep -m 1 -E 'topResumedActivity.*ActivityRecord|mResumedActivity.*ActivityRecord' |
            /system/bin/toybox grep -oE '[a-zA-Z0-9_.]+/[a-zA-Z0-9_.$]+')
        /system/bin/setprop debug.klimt_resumed "${resumed:-unreported}"
        case "$home" in
            */*FallbackHome|*/) continue ;;
            */*) ;;
            *) continue ;;
        esac
        if [ "$resumed" != "$home" ]; then
            /system/bin/toybox timeout 15 /system/bin/am start -W --user 0 -a android.intent.action.MAIN -c android.intent.category.HOME
            continue
        fi
        capture=/data/local/tmp/klimt-home.raw
        if /system/bin/toybox timeout 20 /system/bin/screencap "$capture" &&
            /system/bin/klimt-frame-probe "$capture"; then
            details=$(/system/bin/logcat -d -v raw -s KlimtFrame:I |
                /system/bin/toybox tail -n 1)
            /system/bin/setprop debug.klimt_home_capture "$details"
        else
            /system/bin/setprop debug.klimt_home_capture failed
        fi
        /system/bin/toybox rm -f "$capture"
        break
    fi
done

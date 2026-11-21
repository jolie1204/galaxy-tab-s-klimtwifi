#!/system/bin/sh
set -u
umask 077
PATH=/system/bin
export PATH
output=/data/local/tmp/klimt-setup-frame-v3
tag=debug.klimt_diag_35
mkdir "$output" || exit 1
cd "$output" || exit 1

watch_touch() {
    for node in /dev/input/event*; do
        [ -c "$node" ] || continue
        getevent -lp "$node" 2>/dev/null | grep -Fq 'Synaptics s5707' || continue
        printf '%s\n' "$node" > touch-source.txt
        cat /proc/interrupts 2>/dev/null | grep -iE 'rmi|s5707|12c60000' > touch-irqs-before.txt
        setprop "$tag" panel_touch_listening
        toybox timeout -k 2 125 getevent -l "$node" 2>touch-errors.txt |
            awk '
                /EV_ABS.*ABS_MT_POSITION_[XY]/ {position=1}
                /EV_ABS.*ABS_MT_TRACKING_ID/ {tracking++}
                /EV_SYN.*SYN_REPORT/ && position {
                    position_reports++;
                    position=0;
                    if (position_reports==1) {
                        print "First Synaptics position report received" > "touch-first-report.txt";
                        close("touch-first-report.txt");
                    }
                }
                END {printf "position_reports=%d tracking_events=%d\n", position_reports, tracking}
            ' > touch-counts.txt
        cat /proc/interrupts 2>/dev/null | grep -iE 'rmi|s5707|12c60000' > touch-irqs-after.txt
        return 0
    done
    printf '%s\n' 'Synaptics input node could not be read' > touch-errors.txt
    setprop "$tag" panel_touch_unavailable
}

sleep 6
watch_touch &
touch_pid=$!
for attempt in 1 2 3 4; do
    dumpsys -t 8 activity activities | grep -m 3 'topResumedActivity=' > top-activity.txt
    if grep -q 'com.google.android.setupwizard/.*WelcomeActivity' top-activity.txt; then
        break
    fi
    sleep 3
done
dumpsys -t 14 window displays | grep -E 'mCurrentFocus|mFocusedApp' > focus.txt
if grep -q 'com.google.android.setupwizard/.*WelcomeActivity' top-activity.txt; then
    cat /proc/uptime > before-frame-uptime.txt
    if toybox timeout -k 2 55 screencap -q setup-frame.raw 2> screencap-error.txt; then
        cat /proc/uptime > after-frame-uptime.txt
        setprop "$tag" panel_capture_saved
        sleep 6
        dumpsys -t 12 window displays | grep -E 'mCurrentFocus|mFocusedApp' > focus-later.txt
        dumpsys -t 12 SurfaceFlinger --list | grep -iE 'setupwizard|WelcomeActivity|NavigationBar|StatusBar' > layers.txt
        logcat -d -t 1800 -v threadtime ActivityTaskManager:I WindowManager:I AndroidRuntime:E SurfaceFlinger:W '*:S' |
            grep -iE 'setupwizard|WelcomeActivity' > launch-trace.txt
        pid=$(pidof com.google.android.setupwizard)
        pid=${pid%% *}
        case "$pid" in
            ''|*[!0-9]*) printf '%s\n' 'Setup Wizard process ID unavailable' > setup-process.txt ;;
            *) logcat -d -t 1800 --pid="$pid" -v threadtime '*:W' > setup-process.txt ;;
        esac
    else
        setprop "$tag" panel_capture_failed
    fi
    dumpsys -t 12 window windows | grep -E -m 1 -A 34 'Window #[0-9]+ Window\{[^}]*com.google.android.setupwizard/' > welcome-window.txt
    dumpsys -t 12 SurfaceFlinger --list | grep -iE 'setupwizard|WelcomeActivity|NavigationBar|StatusBar' > layers.txt
else
    setprop "$tag" panel_capture_skipped
fi
wait "$touch_pid"

#!/system/bin/sh
set -u
umask 077
PATH=/system/bin
export PATH
report() { setprop debug.klimt_diag_35 "WIFI_FINAL $*"; }
bounded() { toybox timeout -k 2 "$@"; }

for attempt in $(seq 1 15); do
    status=$(bounded 12 cmd wifi status 2>/dev/null)
    ssid=$(printf '%s\n' "$status" | sed -n 's/^Wifi is connected to //p' | head -n 1)
    if [ -z "$ssid" ]; then sleep 6; continue; fi
    ssid=${ssid#\"}
    ssid=${ssid%\"}
    links=$(bounded 15 dumpsys -t 12 connectivity 2>/dev/null | grep 'InterfaceName: wlan0')
    ipv4=$(printf '%s\n' "$links" | sed -nE 's/.*LinkAddresses: *\[([^]]*)\].*/\1/p' |
        tr ' ,' '\n' | grep -m 1 -E '^[0-9]{1,3}(\.[0-9]{1,3}){3}/[0-9]+$')
    if [ -z "$ipv4" ]; then sleep 6; continue; fi
    if ! printf '%s\n' "$links" | grep -q '0.0.0.0/0'; then sleep 6; continue; fi

    fingerprint=$(printf '%s' "$ssid" | sha256sum | cut -c1-24)
    report "associated with IPv4 and default route profile=$fingerprint"
    if printf '%s\n' "$links" | grep -q VALIDATED; then
        report "Android verified external network reachability"
        exit 0
    fi
    gateway=$(printf '%s\n' "$links" |
        sed -nE 's/.*0\.0\.0\.0\/0 *-> *([0-9]+(\.[0-9]+){3}).*/\1/p' | head -n 1)
    if [ -n "$gateway" ] && bounded 10 ping -c 2 -W 3 "$gateway" >/dev/null 2>&1; then
        report "wlan0 gateway reachable"
        exit 0
    fi
    sleep 6
done
report "saved Wi-Fi profile verification incomplete"
exit 1

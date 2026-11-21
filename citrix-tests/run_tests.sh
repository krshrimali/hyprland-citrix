#!/bin/bash
# Nested end-to-end tests for the citrix-multimonitor Hyprland patches.
T=$(dirname "$(readlink -f "$0")")
I=$(hyprctl instances | awk '/^instance/{s=$2} /wl socket: wayland-2/{print s}' | tr -d ':')
[ -z "$I" ] && { echo "nested instance not running"; exit 1; }
H() { hyprctl -i "$I" "$@"; }
for n in A B C; do H output create headless $n >/dev/null; done; sleep 1
H eval 'hl.monitor({output="A",mode="2560x1440@60",position="0x0",scale=1,transform=1}); hl.monitor({output="B",mode="2560x1440@60",position="1440x560",scale=1,transform=0}); hl.monitor({output="C",mode="1920x1080@60",position="4000x320",scale=1,transform=1}); hl.monitor({output="WAYLAND-1",mode="preferred",position="9000x0",scale=1})' >/dev/null
H eval "hl.bind('SUPER + T', hl.dsp.exec_cmd('touch $T/bindfired'))" >/dev/null
sleep 2
ND=$(for d in 2 3 4 5; do DISPLAY=:$d xprop -root _NET_SUPPORTED 2>/dev/null | grep -q FULLSCREEN_MONITORS && { echo :$d; break; }; done)
PASS=0; FAIL=0
ok() { if [ "$1" = 1 ]; then echo "PASS  $2"; PASS=$((PASS+1)); else echo "FAIL  $2"; FAIL=$((FAIL+1)); fi; }
band() { # monitor -> "blue red" percentages in top 250px
  WAYLAND_DISPLAY=wayland-2 grim -o $1 -t ppm $T/top_$1.ppm && python3 -I -c "
import sys
d=open(sys.argv[1],'rb').read(); p=d.split(b'\n',3); w,h=map(int,p[1].split()); px=p[3]
pts=[(x,y) for y in range(0,250,10) for x in range(0,w,20)]
b=sum(1 for x,y in pts if px[3*(y*w+x)+2]>200 and px[3*(y*w+x)]<50); r=sum(1 for x,y in pts if px[3*(y*w+x)]>200 and px[3*(y*w+x)+2]<50)
print(100*b//len(pts), 100*r//len(pts))" $T/top_$1.ppm; }
# P2
XR=$(DISPLAY=$ND xrandr --listmonitors 2>/dev/null)
ok "$(echo "$XR" | grep -q 'A 1440/[0-9]*x2560/[0-9]*+0+0' && echo "$XR" | grep -q '+1440+560' && echo "$XR" | grep -q '+4000+320' && echo 1)" "P2 xwayland sees real layout"
# panels
for m in A B C; do (WAYLAND_DISPLAY=wayland-2 timeout 60 $T/ls/panel $m >/dev/null 2>&1 &); done; sleep 1
for m in A B C; do set -- $(band $m); ok "$([ "$1" -ge 90 ] && echo 1)" "panel visible on $m without span (blue=$1%)"; done
# P1/P3 span
H eval 'hl.dispatch(hl.dsp.focus({monitor="B"}))' >/dev/null
(DISPLAY=$ND timeout 40 $T/spanclient 1 1 1 3 > $T/span.out 2>&1 &); sleep 5
ok "$(grep -q 'final 5080x2560+0+0' $T/span.out && echo 1)" "P1 span window geometry 5080x2560+0+0"
for m in A B C; do set -- $(band $m); ok "$([ "$2" -ge 90 ] && echo 1)" "P3 span drawn above panel on $m (red=$2%)"; done
for p in "200 100" "2700 600" "4500 400"; do set -- $p; H eval "hl.dispatch(hl.dsp.cursor.move({x=$1, y=$2}))" >/dev/null; sleep 0.4
  t=$(H activewindow -j | python3 -I -c 'import json,sys;print(json.load(sys.stdin).get("title"))'); ok "$([ "$t" = spanclient ] && echo 1)" "P3 pointer over panel area ($1,$2) focuses span window ($t)"; done
# pinned floating window (picture-in-picture) stays above a span on a covered monitor
(WAYLAND_DISPLAY=wayland-2 timeout 20 kitty --class pinpip -o background=#00ff00 -o cursor_blink_interval=0 -e sleep 20 >/dev/null 2>&1 &); sleep 2.5
H eval 'local w for _,x in ipairs(hl.get_windows()) do if x.class=="pinpip" then w=x end end hl.dispatch(hl.dsp.window.fullscreen({mode="maximized", action="unset", window=w})) hl.dispatch(hl.dsp.window.float({action="enable", window=w})) hl.dispatch(hl.dsp.window.resize({x=600, y=500, window=w})) hl.dispatch(hl.dsp.window.move({x=100, y=1000, window=w})) hl.dispatch(hl.dsp.window.pin({action="enable", window=w}))' >/dev/null; sleep 1
G=$(WAYLAND_DISPLAY=wayland-2 grim -o A -t ppm $T/pin_A.ppm && python3 -I -c "
import sys
d=open(sys.argv[1],'rb').read(); p=d.split(b'\n',3); w,h=map(int,p[1].split()); px=p[3]
pts=[(x,y) for y in range(1100,1450,10) for x in range(150,650,20)]
print(100*sum(1 for x,y in pts if px[3*(y*w+x)+1]>200 and px[3*(y*w+x)]<50)//len(pts))" $T/pin_A.ppm)
ok "$([ "$G" -ge 90 ] && echo 1)" "pinned window drawn above span on A (green=$G%)"
H eval 'hl.dispatch(hl.dsp.cursor.move({x=400, y=1250}))' >/dev/null; sleep 0.4
t=$(H activewindow -j | python3 -I -c 'import json,sys;print(json.load(sys.stdin).get("class"))'); ok "$([ "$t" = pinpip ] && echo 1)" "pointer over pinned window above span focuses it ($t)"
pkill -f "class pinpip"
pkill -f "$T/spanclient"; sleep 2
for m in A C; do set -- $(band $m); ok "$([ "$1" -ge 90 ] && echo 1)" "panel restored on $m after span ends (blue=$1%)"; done
# extend while already fullscreen on one monitor (toolbar "extend to all displays")
for m in A B C; do (WAYLAND_DISPLAY=wayland-2 timeout 40 $T/ls/panel $m >/dev/null 2>&1 &); done; sleep 1
H eval 'hl.dispatch(hl.dsp.focus({monitor="B"}))' >/dev/null
(DISPLAY=$ND timeout 30 $T/spanclient 1 1 1 3 late > $T/span_late.out 2>&1 &); sleep 10
ok "$(sed -n '/before-extend/,$p' $T/span_late.out | grep -q 'final 5080x2560+0+0' && sed -n '1,/before-extend/p' $T/span_late.out | grep -q 'configure 2560x1440+1440+560' && echo 1)" "extend from single-monitor fullscreen spans 5080x2560+0+0"
for m in A C; do set -- $(band $m); ok "$([ "$2" -ge 90 ] && echo 1)" "extend: span drawn above panel on $m (red=$2%)"; done
ok "$(DISPLAY=$ND xprop -root _NET_ACTIVE_WINDOW >/dev/null && [ "$(grep -c configure $T/span_late.out)" -ge 1 ] && echo 1)" "extend: client received configure"
pkill -f "$T/spanclient"; pkill -f "$T/ls/panel"; sleep 2
# P4 keyboard grab
rm -f $T/bindfired; WAYLAND_DISPLAY=wayland-2 $T/vk/vkey; sleep 1; ok "$([ -f $T/bindfired ] && echo 1)" "P4 bind fires with no grab"; rm -f $T/bindfired
(DISPLAY=$ND $T/grabclient >/dev/null 2>&1 &); sleep 3.5; WAYLAND_DISPLAY=wayland-2 $T/vk/vkey; sleep 1; ok "$([ ! -f $T/bindfired ] && echo 1)" "P4 bind suppressed during X keyboard grab"; rm -f $T/bindfired
sleep 4; WAYLAND_DISPLAY=wayland-2 $T/vk/vkey; sleep 1; ok "$([ -f $T/bindfired ] && echo 1)" "P4 bind fires again after ungrab"
pkill -f "$T/ls/panel"
echo "== $PASS passed, $FAIL failed"

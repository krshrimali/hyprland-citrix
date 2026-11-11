#!/bin/bash
# Builds the nested test clients. Usage: ./build.sh && start nested Hyprland (see README) && ./run_tests.sh
set -e
cd "$(dirname "$(readlink -f "$0")")"
P=../protocols
gcc -O2 -o spanclient spanclient.c -lX11
gcc -O2 -o grabclient grabclient.c -lX11
(cd ls && wayland-scanner client-header ../$P/wlr-layer-shell-unstable-v1.xml ls.h && wayland-scanner private-code ../$P/wlr-layer-shell-unstable-v1.xml ls.c \
  && wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h \
  && wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg.c && gcc -O2 -o panel panel.c ls.c xdg.c -lwayland-client)
(cd vk && wayland-scanner client-header ../$P/virtual-keyboard-unstable-v1.xml vk.h && wayland-scanner private-code ../$P/virtual-keyboard-unstable-v1.xml vk.c && gcc -O2 -o vkey vkey.c vk.c -lwayland-client -lxkbcommon)
echo built

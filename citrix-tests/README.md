# Nested tests for the Citrix multi-monitor patches

Runs a second, patched Hyprland in a window on your desktop, with three headless
outputs laid out like the real desk (portrait / landscape offset / portrait).

    ./build.sh
    env -u HYPRLAND_INSTANCE_SIGNATURE ../build/Hyprland -c "$PWD/hyprland.lua" &   # wait ~5s
    ./run_tests.sh

Expects the nested instance on `wayland-2` and its Xwayland on `:2`-`:5`.
Checks: real Xwayland layout, span geometry, drawing above top layers on every
covered monitor, pointer focus over panels, panels restored afterwards, and
keybind suppression during an X11 keyboard grab.

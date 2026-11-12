# Citrix client-side performance helpers

- `hide-nvidia.c`: LD_PRELOAD shim. wfica disables hardware H.264 decode when any NVIDIA GPU is
  present. This hides only `/sys/class/drm/cardN/device/{uevent,vendor}` of NVIDIA cards from it, so
  it decodes on the Intel iGPU (VA-API). OpenGL stays on NVIDIA.
  Build: `gcc -O2 -Wall -shared -fPIC -o hide-nvidia.so hide-nvidia.c -ldl`
- `citrix-wfica`: launcher used by the `citrix-wfica.desktop` override (`citrix-wfica.desktop.in`).
  Install: `~/.local/lib/citrix/hide-nvidia.so`, `~/.local/bin/citrix-wfica`,
  `~/.local/share/applications/citrix-wfica.desktop`.
  `CITRIX_HIDE_NVIDIA=0` disables. Uninstall: delete those three files.
- `~/.ICAClient/wfclient.ini` `[Thinwire3.0]`: `HWAccelerationEnabled`, `TextTrackingEnabled`,
  `SmallFramesEnabled` = True (backup: `wfclient.ini.bak-perf`).

Verify after connecting: `journalctl --since -5min | grep -iE 'hardware decod|software decod|TextTracking|SmallFrame|citrix-hide-nvidia'`.

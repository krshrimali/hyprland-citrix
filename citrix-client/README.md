# Citrix client-side performance helpers

- `hide-nvidia.c`: LD_PRELOAD shim. wfica disables hardware H.264 decode when any NVIDIA GPU is
  present (it counts PCI devices whose sysfs `class` is 0x030000). The shim reports NVIDIA as 0x030200
  (3D controller) and the Intel iGPU as 0x030000, and hides NVIDIA's `/sys/class/drm/cardN/device/{uevent,vendor}`, so
  it enables HW decode. OpenGL stays on NVIDIA. Its ffmpeg VA-API then opens the first render node
  (Intel), whose tiled surfaces NVIDIA EGL can't import (`failed to eglCreateImageKHR`), so the
  launcher sets `CITRIX_VAAPI_FROM/TO` (shim redirects that node to NVIDIA's) +
  `LIBVA_DRIVER_NAME=nvidia NVD_BACKEND=direct` (libva-nvidia-driver, NVDEC).
  Build: `gcc -O2 -Wall -shared -fPIC -o hide-nvidia.so hide-nvidia.c -ldl`
- `citrix-wfica`: launcher used by the `citrix-wfica.desktop` override (`citrix-wfica.desktop.in`).
  Install: `~/.local/lib/citrix/hide-nvidia.so`, `~/.local/bin/citrix-wfica`,
  `~/.local/share/applications/citrix-wfica.desktop`.
  `CITRIX_HIDE_NVIDIA=0` disables. Uninstall: delete those three files.
- `~/.ICAClient/wfclient.ini` `[Thinwire3.0]`: `HWAccelerationEnabled`, `TextTrackingEnabled`,
  `SmallFramesEnabled` = True (backup: `wfclient.ini.bak-perf`).

Verify after connecting: `journalctl --since -5min | grep -iE 'hardware decod|software decod|TextTracking|SmallFrame|citrix-hide-nvidia'`.
  Debug: `CITRIX_HIDE_NVIDIA_DEBUG=/path/log` logs every file wfica opens. Do not launch through `timeout`/wrappers: the shim strips itself from LD_PRELOAD for children.

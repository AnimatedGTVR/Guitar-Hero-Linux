# Clone Hero on GHL

GHL packages a small `clonehero` launcher instead of embedding the game in the
base image. The official Linux v1.1.0.6142 standalone archive is approximately
1.17 GB.

```sh
ampkg install clonehero
clonehero doctor
clonehero status
clonehero install
clonehero
```

The installer downloads the official standalone archive, verifies its
published SHA-1 (`63350926e834b72a7ca193d46391199076f91bf3`), and extracts it
under `/opt/clonehero`. It requires explicit EULA acceptance.

Downloads are stored in disk-backed `/var/cache/clonehero`, never `/tmp`
(which is RAM-backed on GHL). Installation checks for at least 3 GB free and
removes the 1.17 GB archive after successful extraction. The default QEMU
root image is sparse: it has 4 GB capacity without occupying 4 GB on the host.
Interrupted downloads are retained and resume the next time `clonehero install`
is run. Use `clonehero clean` to discard a partial download, or
`clonehero remove` to remove the game while preserving songs and player data.

`clonehero doctor` reports DRM, ALSA, graphical-session, payload, and evdev
controller readiness. The launcher will not pretend the game can start while
one of those platform layers is missing.

Backstage writes compositor startup diagnostics to
`/var/log/backstage-gamescope.log`. On QEMU it uses accelerated VirtIO/Venus;
if that session cannot initialize within 45 seconds, the menu falls back to
direct KMS graphics instead of hanging indefinitely.

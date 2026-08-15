# The virtual display

The second display a client streams is normally not a monitor anyone owns. It is
the size of the client's second panel, it should exist only while a session is
running, and no physical monitor is likely to match it.

## Driver requirement

Sunshine DS controls a virtual display driver; it does not install or implement
one. Windows virtual monitors are provided by signed indirect display drivers
(IddCx), so one of the supported drivers must already be installed:

- [SudoVDA](https://github.com/SudoMaker/SudoVDA) is preferred. Its control
  protocol creates a monitor at the client's exact requested width, height, and
  refresh rate for each session.
- [MikeTheTech Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)
  is supported as a compatibility fallback. Its available modes are configured
  before the driver starts, so Sunshine DS can select only a mode the running
  driver already exposes.

`libdisplaydevice`, which Sunshine already uses, configures display topology and
modes. It cannot add a new mode to a running indirect display driver.

## Configuration

Set `dual_display_source` in `sunshine.conf`:

| Value | Meaning |
| --- | --- |
| *(empty)* | The feature is off. This is the default. |
| `virtual` | Acquire a supported virtual display for the client's second panel. |
| anything else | Capture the real monitor identified by that output name or stable device identifier. |

The server advertises two-stream support only when the configured source can be
resolved. If acquisition later fails, the session continues with its primary
stream rather than failing the whole connection.

### SudoVDA lifecycle

For `dual_display_source = virtual`, Sunshine DS first opens the installed
SudoVDA interface and verifies its protocol version. It then:

1. derives a stable monitor identity from the paired client;
2. creates a monitor at the exact client-requested mode;
3. extends the Windows desktop and resolves the new target to its GDI/DXGI
   output name;
4. keeps the driver's watchdog alive for the session; and
5. removes the monitor when the second stream ends.

The stable identity lets Windows remember the monitor's position between
reconnects without accumulating a new ghost monitor for every session. An
overlapping session cannot claim the same identity.

### MikeTheTech fallback

When SudoVDA is unavailable, Sunshine DS can lease a recognized MikeTheTech
virtual output. It can use an already active output or activate a detached one
by stable device identifier. It does not treat an arbitrary disconnected
physical monitor as virtual.

The driver reads its modes from `vdd_settings.xml` when it initializes. The
default location is `C:\VirtualDisplayDriver\vdd_settings.xml`; a `VDDPATH`
value under `HKLM\SOFTWARE\MikeTheTech\VirtualDisplayDriver` can override that
directory. Add every client-panel mode that must be exact before restarting the
driver. For example, an AYN Thor lower panel entry is:

```xml
<resolution>
    <width>1240</width>
    <height>1080</height>
    <refresh_rate>30</refresh_rate>
</resolution>
```

Editing the XML alone does not update the running driver's mode table. Restart
only the virtual display device, or reboot Windows, after changing it. The
driver's named-pipe reload command in existing releases does not add a new
resolution to an already initialized mode table.

At session start Sunshine DS chooses an exact enumerated mode when available.
If the driver does not expose one, it logs the nearest supported mode and the
encoder scales or letterboxes that capture to the client dimensions. An exact
driver mode avoids that extra conversion and preserves one-to-one desktop and
touch geometry.

For an existing virtual output, Sunshine DS records the original Windows mode.
On teardown it restores that mode only if the output is still using the mode
Sunshine applied; a mode changed by the user or another component is preserved.
An automatically activated output also restores its preceding topology.

### Using a real monitor

Set `dual_display_source` to the output name or stable display identifier of an
attached monitor. Nothing is created or removed. Capture uses that display and
the encoder produces the dimensions requested by the client.

## Behavior without a supported source

When the feature is disabled or no configured source is available,
`dual_display::supported()` reports false. The server then advertises one video
stream and refuses setup of `streamid=video/1/0`. Primary streaming, audio, and
input retain normal Sunshine behavior.

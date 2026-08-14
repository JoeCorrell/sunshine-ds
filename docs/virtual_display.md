# The virtual display

The second display a client streams is normally not a monitor anyone owns. It is
the size of the client's second panel, it should exist only while a session is
running, and no real monitor matches it.

## What this fork cannot do

**Sunshine cannot create a monitor, and neither can this fork.**

Windows has no API for it. A virtual monitor is an *indirect display driver* — a
WDDM/IddCx kernel-mode driver — and installing one requires that it be signed.
That is a separate product with a separate release and signing story, and no
amount of work inside this codebase produces one.

`libdisplaydevice`, which Sunshine already uses, does not help here: it
*configures* displays that exist (topology, resolution, HDR, which is primary). It
has no notion of bringing one into being.

So the honest description of this feature is: **this fork drives an indirect
display driver that the user has installed.** The driver is a dependency, not a
component.

## What happens without one

Nothing, loudly enough to diagnose and quietly enough not to break anything.

`dual_display::supported()` reports false, and everything downstream follows from
that one answer:

- `/serverinfo` advertises `MaxVideoStreams` as 1.
- `RTSP SETUP` for `streamid=video/1/0` is refused with 404.
- A client that asks for a second display in its SDP anyway is told, in the log,
  that none is available, and gets a single-display session.

A client behaves exactly as it does against stock Sunshine. This is the default
state — `dual_display_source` is empty unless somebody sets it — so upgrading to
this fork changes nothing until it is configured.

## Configuration

One setting, `dual_display_source`, in `sunshine.conf`:

| Value | Meaning |
| --- | --- |
| *(empty)* | The feature is off. This is the default. |
| `virtual` | Ask the indirect display driver for a monitor sized to the client's second panel. |
| anything else | Capture the real monitor with that output name. |

A single setting with a reserved word rather than a boolean plus a name, because
two settings permit the contradictory state — a named monitor with the virtual
flag also set — that somebody would then have to define the meaning of.

### Using a real monitor

Works today, with no driver. Set `dual_display_source` to the output name of a
monitor you are not otherwise using; the name is the same one `output_name`
takes, and `dual_display::supported()` checks it is actually attached before
advertising anything.

The mode streamed is what the *client* asked for rather than the monitor's native
mode, because the encoder scales and the alternative sends a 4K desktop to a
panel that cannot show it.

### Using a virtual display

Not yet bound to a driver. `virtual_display_available()` in
[`src/dual_display.cpp`](../src/dual_display.cpp) returns false, which switches
the whole feature off as described above.

Binding one means implementing two functions against a chosen driver:

- `virtual_display_available()` — is the driver installed and responding.
- a `lease_t` that asks it for a monitor at a given mode on construction, and
  removes that monitor on destruction.

The rest of the fork is written against `lease_t` and needs no further change:
capture opens the leased display by name through the ordinary path and does not
know it is virtual.

Candidate drivers, all IddCx-based and permissively licensed:

- [Virtual-Display-Driver](https://github.com/VirtualDisplay/Virtual-Display-Driver)
- [IddSampleDriver](https://github.com/roshkins/IddSampleDriver)

The choice matters mostly for how monitors are requested at run time — some are
driven by a config file read at driver start, which is unusable here because the
mode is not known until a client connects, and some expose a control channel,
which is what this needs.

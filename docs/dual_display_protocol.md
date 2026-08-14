# Dual-display streaming

A second video stream, so a client with two screens can be sent two displays at
once rather than one display and a trackpad.

This document is the contract. It is implemented on both sides of the wire — by
this fork on the host, and by a fork of `moonlight-common-c` plus Moonlight DS on
the client — and neither side may be changed without the other.

## Why a second stream rather than a composite

The obvious cheaper design is to stack both displays into one tall frame, encode
that, and let the client crop it into two. It needs no protocol change at all.

It was rejected because the two screens are not the same thing. The Thor's panels
differ in size and aspect, a desktop on the lower one wants a different
resolution from a game on the upper one, and a composite forces both to share one
bitrate budget and one frame cadence — so a still desktop on the second panel
spends bandwidth that the game needed, every frame, forever. A second stream
costs more to build once and nothing thereafter.

## What already exists

Rather more than one would expect, because NVIDIA's original protocol was
designed for this and the implementations only ever filled in the first slot.

- The SDP already reserves four video slots, `x-nv-video[0]` through
  `x-nv-video[3]`. Only `[0]` is ever populated with real values; the other three
  receive `transferProtocol` and `rateControlMode` boilerplate and nothing else.
- RTSP stream identifiers already carry an index: `streamid=video/0/0`. Sunshine
  parses the type before the `/` and discards the rest, so `video/1/0` currently
  resolves to the same port as `video/0/0`.
- Ports are negotiated rather than fixed. `SETUP` replies with
  `Transport: server_port=<port>`, and the client's `parseServerPortFromTransport`
  reads it, falling back to the well-known port only when parsing fails.

So the protocol has the shape for this already. What is missing is that both ends
implement exactly one video stream.

## Negotiation

Four steps, each of which degrades to current behaviour when the other end does
not implement it.

### 1. The host advertises the capability

`/serverinfo` gains one element:

```xml
<MaxVideoStreams>2</MaxVideoStreams>
```

Absent means one, which is what every current host means. A client that does not
understand the element ignores it, as it ignores every other unknown element.

### 2. The client requests a second display

In the `ANNOUNCE` SDP, the client fills in the reserved slot it wants. These are
the same attribute names slot `[0]` uses, so no new grammar is introduced:

```
x-nv-video[1].clientViewportWd:  <width>
x-nv-video[1].clientViewportHt:  <height>
x-nv-video[1].maxFPS:            <fps>
x-nv-video[1].initialBitrateKbps: <kbps>
x-ml-video[1].enable:            1
```

`x-ml-video[1].enable` is the explicit request. It exists because the `x-nv-`
slots carry boilerplate today, so their presence alone cannot be read as intent —
a host must not start a second encoder because an old client sent
`x-nv-video[1].rateControlMode` and meant nothing by it.

A host that does not implement this ignores all five and streams one display. The
client detects that from step 3.

### 3. The client sets up the second stream

```
SETUP rtsp://<host>:48010/streamid=video/1/0
```

The host parses the index after `video/` and replies with a *different*
`server_port` from the one it gave `video/0/0`.

A host that has not implemented the index replies with the same port it gave
slot 0. **That collision is how the client detects an unsupporting host**, and it
must check: two streams on one port is not a degraded stream, it is two decoders
being fed one interleaved bytestream, which fails in a way that looks like
corruption rather than like a missing feature. On detecting it the client tears
the second stream down and carries on with one display.

### 4. Play

```
PLAY rtsp://<host>:48010/streamid=video/1/0
```

Both streams then run independently: separate RTP sockets, separate FEC, separate
sequence numbering, separate IDR requests.

## What is deliberately shared

One control stream, one audio stream, and one session. The second display is a
second *video* stream and nothing else.

This matters for the control channel in particular. IDR requests, bitrate changes
and the termination message all carry a stream index now, and the existing
messages are defined to mean stream 0 so an old client's request still means what
it always meant.

Input is likewise unchanged and unsplit. The client sends absolute pointer
coordinates against a viewport it declares, and which display those land on is
resolved host-side from the same topology it is capturing — a second input
channel would be a second thing to keep in step for no gain.

## Bitrate

The two streams are budgeted separately, and the client asks for each. The
temptation is to split one budget by area, which is wrong here: the second panel
is usually showing a desktop that is static for minutes at a time, and a
proportional split would starve the game to hold bandwidth for a screen that is
not changing.

The host applies its own ceiling to the total, as it does today for one stream.

## Failure and teardown

The second stream may fail on its own — the virtual display can be removed by the
user, the second encoder can fail to initialise on a machine with one NVENC
session available. None of that may take the first stream down.

A second stream that ends sends `x-ml-video[1]` termination on the control
channel; the client drops to one display and says so. The reverse does not apply:
the first stream ending ends the session, because that is the session.

## The virtual display

The second display is normally not a monitor anyone owns. It is created on demand,
sized to the client's second panel, and removed when the session ends.

**Sunshine cannot create one by itself, and neither can this fork.** A virtual
monitor on Windows is an indirect display driver — a WDDM/IddCx driver that must
be signed to install. What this fork does is *drive* one: it discovers an
installed IDD, asks it for a monitor at the requested mode, captures it as an
ordinary display, and releases it afterwards. The driver is a dependency, not a
component, and where it is absent the second stream is refused at step 1 —
`MaxVideoStreams` reports 1 and every client behaves exactly as it does today.

See `docs/virtual_display.md` for which drivers are supported and how one is
selected.

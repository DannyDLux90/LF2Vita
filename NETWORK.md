# Network Play implementation plan

Status in **0.69**: UI/protocol scaffold only; no sockets or Ad-Hoc transport are started.

## Intended user flows

1. **Vita Ad-Hoc - Host** — create a nearby Vita game, advertise the session, wait for a peer and start synchronized play.
2. **Vita Ad-Hoc - Game Join** — discover/join a nearby Vita host.
3. **PC LF2 2.00a - Host** — expose a session compatible with the original PC network mode.
4. **PC LF2 2.00a - Game Join** — connect to an original-compatible PC host.

## Architecture direction

Networking must synchronize **inputs and deterministic simulation state**, not send rendered frames. The port therefore needs a stable deterministic core first: identical 30 Hz ticks, RNG sequencing, object IDs/spawn order, AI decisions, stage state and collision outcomes. A per-tick checksum will be added before transport is enabled so desyncs can be diagnosed in `lf2.log`.

The Vita-only path can then use a local peer-discovery/Ad-Hoc transport. PC compatibility is a separate profile because it has to reproduce the original LF2 2.00a connection framing and synchronization behaviour rather than invent a new packet format. Interoperability will only be marked enabled after capture/reverse-engineering and Vita-to-PC tests succeed.


Milestone order is tracked in `ROADMAP.md`: native network play, then server browser, then PC-compatible networking.

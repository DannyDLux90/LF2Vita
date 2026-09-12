# Stage clone and native AdHoc notes — 2026-09-12

This branch keeps Stage wave actors separate from dynamically spawned fighters. Rudolf clones use the dedicated dynamic fighter pool instead of authored Stage enemy slots, so an allied clone cannot keep `stage_phase_settled()` false after a wave is cleared.

Native Vita-to-Vita AdHoc now uses protocol version 2. Input sampled on simulation frame N is scheduled for N+1, with frame 0 as a deterministic neutral warm-up. Each input packet also repeats the previously scheduled frame for one-packet loss/delay recovery. This keeps deterministic lockstep while avoiding the old per-frame RTT stall when the peer input arrives within the 33.33 ms simulation interval.

The native lockstep timeout is 350 ms rather than 5 s. Periodic `NET native lockstep` log lines include `wait_avg_us`, `wait_max_us`, and `buffered` counters for hardware jitter diagnosis.

Both Vitas must run the same protocol-v2 build for AdHoc testing.

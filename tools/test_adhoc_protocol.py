#!/usr/bin/env python3
"""Deterministic CI stress tests for native AdHoc lockstep protocol v4.

The hardware transport is Vita-only, but the packet/history contract is pure:
one-TU delayed input, an 8-frame recovery window, a neutral frame-0 barrier,
and bounded retransmission. This model deliberately injects packet loss,
reordering and scheduler spikes and verifies both peers consume identical input.
"""
from __future__ import annotations
import heapq
import random

WINDOW = 8
CACHE = 64
REGULAR_RESEND_MS = 8
BARRIER_RESEND_MS = 50
REGULAR_TIMEOUT_MS = 1500
BARRIER_TIMEOUT_MS = 5000


def make_packet(history: dict[int, int], newest: int):
    first = max(0, newest - (WINDOW - 1))
    return first, newest, tuple(history[f] for f in range(first, newest + 1))


def apply_packet(cache: dict[int, tuple[int, int]], pkt):
    first, newest, held = pkt
    assert len(held) == newest - first + 1
    assert 1 <= len(held) <= WINDOW
    for i, value in enumerate(held):
        f = first + i
        cache[f % CACHE] = (f, value)


def peek(cache: dict[int, tuple[int, int]], frame: int):
    value = cache.get(frame % CACHE)
    return None if value is None or value[0] != frame else value[1]


def basic_window_tests():
    history = {0: 0}
    for f in range(1, 2000):
        history[f] = ((f * 0x45D9F3B) ^ (f << 7)) & 0xFFFFFFFF
    cache = {}
    for newest in range(1, 1500):
        if newest % 10 not in (0, 7, 8, 9):
            continue
        apply_packet(cache, make_packet(history, newest))
        first = max(0, newest - 7)
        for f in range(first, newest + 1):
            assert peek(cache, f) == history[f], (newest, f)

    # Cache wrap must not make a stale frame look valid.
    cache = {}
    for newest in range(1, 1000):
        apply_packet(cache, make_packet(history, newest))
    for f in range(0, 900):
        if 999 - f >= CACHE:
            assert peek(cache, f) is None


def stress_lockstep(seed=0x4C463256, frames=2400):
    rng = random.Random(seed)
    local = [{0: 0}, {0: 0}]
    remote_cache = [{}, {}]
    events = []  # (delivery_ms, serial, receiver, packet)
    serial = 0
    now = 0

    def physical(peer: int, frame: int) -> int:
        # Deterministic but busy controller pattern using all native action bits.
        x = ((frame + 1) * (0x9E3779B1 ^ (peer * 0x13579BDF))) & 0x7FF
        return x ^ ((frame // 17) & 0x7FF)

    def send(sender: int, packet, t: int):
        nonlocal serial
        # 18% independent loss plus occasional burst-like drop regions.
        if rng.random() < 0.18:
            return
        delay = rng.randint(2, 28)
        if rng.random() < 0.025:
            delay += rng.randint(80, 360)  # scheduler/radio spike
        # Reordering naturally falls out of independently sampled delay.
        serial += 1
        heapq.heappush(events, (t + delay, serial, 1 - sender, packet))

    for frame in range(frames):
        # Sample current physical input for frame+1 on both peers.
        for peer in (0, 1):
            local[peer][frame + 1] = physical(peer, frame)

        packets = [make_packet(local[p], frame + 1) for p in (0, 1)]
        resend = BARRIER_RESEND_MS if frame == 0 else REGULAR_RESEND_MS
        timeout = BARRIER_TIMEOUT_MS if frame == 0 else REGULAR_TIMEOUT_MS
        next_send = [now, now]
        got = [peek(remote_cache[p], frame) is not None for p in (0, 1)]
        deadline = now + timeout

        while not all(got):
            next_event = events[0][0] if events else 10**18
            send0 = next_send[0] if not got[1] else 10**18
            send1 = next_send[1] if not got[0] else 10**18
            step = min(next_event, send0, send1, deadline)
            now = step
            if now >= deadline:
                raise AssertionError(f"timeout frame={frame} got={got} queue={len(events)}")
            for sender in (0, 1):
                receiver = 1 - sender
                if not got[receiver] and now >= next_send[sender]:
                    send(sender, packets[sender], now)
                    next_send[sender] = now + resend
            while events and events[0][0] <= now:
                _, _, receiver, pkt = heapq.heappop(events)
                apply_packet(remote_cache[receiver], pkt)
                if peek(remote_cache[receiver], frame) is not None:
                    got[receiver] = True

        # Both sides must consume exactly the peer's scheduled frame.
        assert peek(remote_cache[0], frame) == local[1][frame]
        assert peek(remote_cache[1], frame) == local[0][frame]
        # One simulation TU = 33.333ms. If the network was already buffered,
        # advance on the simulation cadence; if not, waiting can push it later.
        now += 33


if __name__ == "__main__":
    basic_window_tests()
    for s in (1, 7, 1234, 0x4C463256, 0xDEADBEEF):
        stress_lockstep(seed=s)
    print("adhoc protocol v4 window/loss/reorder stress: OK")

#!/usr/bin/env python3
"""Deterministic CI stress tests for native AdHoc lockstep protocol v6.

The hardware transport is Vita-only, but the packet/history contract is pure:
one-TU delayed input, an 8-frame recovery window, a neutral frame-0 barrier,
bounded retransmission, and a callback fast-path that bypasses the lobby event
queue for active match input. The model injects independent loss, short loss
bursts, packet reordering and scheduler/radio spikes.
"""
from __future__ import annotations
import heapq
import random
from pathlib import Path

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


def source_contract_tests():
    root = Path(__file__).resolve().parents[1]
    header = (root / "src/net70/adhoc_net.h").read_text()
    cb = (root / "src/net70/adhoc_net_02.inc").read_text()
    init = (root / "src/net70/adhoc_net_03.inc").read_text()
    cache = (root / "src/net70/adhoc_net_04.inc").read_text()
    lockstep = (root / "src/net70/adhoc_net_05.inc").read_text()
    ui = "".join((root / f"src/net70/net_ui_modern_common_01{x}.inc").read_text() for x in "abcd")
    assert "LF2_ADHOC_PROTO_VERSION 6" in header
    assert "partner_character" in header and "battle_follower" in header and "battle_count" in header
    assert "lf2_adhoc_lobby_set_battle_follower" in header and "lf2_adhoc_lobby_set_battle_count" in header
    # The callback must offer match data to the fast path before queue_event.
    fn = cb[cb.index("static void matching_cb"):cb.index("static bool pop_event")]
    assert fn.index("fast_match_input") < fn.index("queue_event")
    assert 'sceKernelCreateLwMutex(&g_input_mutex' in init
    assert "g_native_fast_rx++" in cache
    assert "g_native_peer_addr" in cache
    assert "native_peer_acked" in cache and "native_ack_frame_snapshot" in cache
    assert "g_native_prefetch_deferred" in lockstep
    # Input prefetch must happen before non-critical event draining, and after
    # frame 0 the current step is allowed to finish without waiting for a
    # future-input send to leave the local Matching queue.
    lock = lockstep[lockstep.index("bool lf2_adhoc_lockstep_frame"):lockstep.index("void lf2_adhoc_match_report_state")]
    assert lock.index("initial_rc=send_to") < lock.index("queued_event_t e")
    assert "got&&(frame!=0||peer_acked_next)" in lock
    assert "p.ack_frame=native_ack_frame_snapshot()" in lock
    assert "send_lobby_to(&peer)" in lock and "100000ULL" in lock
    assert "Dreieck+L/R Partner" in ui
    assert "battle_count" in ui and "Dreieck+Hoch/Runter Anzahl" in ui
    assert "net70_run_adhoc_championship_1v1" in ui and "net70_run_adhoc_championship_2v2" in ui
    assert "st.max_players=(uint8_t)v" in ui
    lobby = (root / "src/net70/adhoc_net_05.inc").read_text()
    create_ui = (root / "src/net70/net_ui_modern_common_02.inc").read_text()
    host_impl = (root / "src/net70/adhoc_net_04.inc").read_text()
    assert "local_choice_changed" in lobby and "ready_reset=1" in lobby
    assert "LF2_ADHOC_MAX_PLAYERS" in create_ui and "%u Vitas" in create_ui
    assert "settings.max_players<2" in host_impl and "settings.max_players>LF2_ADHOC_MAX_PLAYERS" in host_impl



def stress_starting_recovery(seed=0x53544152, trials=300):
    """A lost STARTING transition must be repaired by the frame-0 refresh.

    This models only the lobby-to-game handoff: the host repeats STARTING every
    100 ms while it waits at frame 0. Include independent loss, short burst
    loss, reordering/jitter and occasional ~1 s radio/scheduler blackouts.
    """
    rng = random.Random(seed)
    worst = 0
    for _ in range(trials):
        now = 0
        next_send = 0
        events = []
        serial = 0
        burst = 0
        blackout_until = rng.randint(0, 1100) if rng.random() < 0.18 else 0
        delivered_at = None
        while now < BARRIER_TIMEOUT_MS:
            next_event = events[0][0] if events else 10**18
            step = min(next_send, next_event, BARRIER_TIMEOUT_MS)
            now = step
            if now >= BARRIER_TIMEOUT_MS:
                break
            if now >= next_send:
                next_send += 100
                drop = now < blackout_until
                if burst > 0:
                    burst -= 1
                    drop = True
                elif rng.random() < 0.06:
                    burst = rng.randint(1, 3)
                    drop = True
                elif rng.random() < 0.30:
                    drop = True
                if not drop:
                    delay = rng.randint(2, 45)
                    if rng.random() < 0.08:
                        delay += rng.randint(80, 480)
                    serial += 1
                    heapq.heappush(events, (now + delay, serial))
            while events and events[0][0] <= now:
                delivered_at = heapq.heappop(events)[0]
                break
            if delivered_at is not None:
                break
        assert delivered_at is not None and delivered_at < BARRIER_TIMEOUT_MS, (seed, blackout_until)
        worst = max(worst, delivered_at)
    return worst

def basic_window_tests():
    history = {0: 0}
    for f in range(1, 3000):
        history[f] = ((f * 0x45D9F3B) ^ (f << 7)) & 0xFFFFFFFF
    cache = {}
    for newest in range(1, 2200):
        if newest % 13 not in (0, 1, 6, 10, 12):
            continue
        apply_packet(cache, make_packet(history, newest))
        first = max(0, newest - (WINDOW - 1))
        for f in range(first, newest + 1):
            assert peek(cache, f) == history[f], (newest, f)

    # Cache wrap must never make a stale frame look valid.
    cache = {}
    for newest in range(1, 1400):
        apply_packet(cache, make_packet(history, newest))
    for f in range(0, 1200):
        if 1399 - f >= CACHE:
            assert peek(cache, f) is None


def stress_lockstep(seed=0x4C463256, frames=6000):
    rng = random.Random(seed)
    local = [{0: 0}, {0: 0}]
    remote_cache = [{}, {}]
    events = []  # (delivery_ms, serial, receiver, packet)
    serial = 0
    now = 0
    burst_left = [0, 0]

    def physical(peer: int, frame: int) -> int:
        # Deterministic busy controller pattern using the complete native mask.
        x = ((frame + 1) * (0x9E3779B1 ^ (peer * 0x13579BDF))) & 0x7FF
        return x ^ ((frame // 17) & 0x7FF)

    def send(sender: int, packet, t: int):
        nonlocal serial
        # Independent loss plus realistic short radio/scheduler loss bursts.
        if burst_left[sender] > 0:
            burst_left[sender] -= 1
            return
        if rng.random() < 0.008:
            burst_left[sender] = rng.randint(2, 5)
            return
        if rng.random() < 0.22:
            return
        delay = rng.randint(2, 34)
        if rng.random() < 0.035:
            delay += rng.randint(70, 520)
        serial += 1
        heapq.heappush(events, (t + delay, serial, 1 - sender, packet))

    max_wait = 0
    total_wait = 0
    waits = 0
    for frame in range(frames):
        for peer in (0, 1):
            local[peer][frame + 1] = physical(peer, frame)

        packets = [make_packet(local[p], frame + 1) for p in (0, 1)]
        resend = BARRIER_RESEND_MS if frame == 0 else REGULAR_RESEND_MS
        timeout = BARRIER_TIMEOUT_MS if frame == 0 else REGULAR_TIMEOUT_MS
        next_send = [now, now]
        got = [peek(remote_cache[p], frame) is not None for p in (0, 1)]
        deadline = now + timeout
        wait_started = now

        while not all(got):
            next_event = events[0][0] if events else 10**18
            send0 = next_send[0] if not got[1] else 10**18
            send1 = next_send[1] if not got[0] else 10**18
            step = min(next_event, send0, send1, deadline)
            now = step
            if now >= deadline:
                raise AssertionError(f"timeout frame={frame} got={got} queue={len(events)} seed={seed}")
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

        waited = now - wait_started
        max_wait = max(max_wait, waited)
        total_wait += waited
        waits += 1
        assert peek(remote_cache[0], frame) == local[1][frame]
        assert peek(remote_cache[1], frame) == local[0][frame]
        now += 33

    assert max_wait < REGULAR_TIMEOUT_MS
    return total_wait / max(1, waits), max_wait


if __name__ == "__main__":
    source_contract_tests()
    basic_window_tests()
    stats = []
    for s in (1, 7, 1234, 0x4C463256, 0xDEADBEEF, 0xA5A5A5A5, 0x13579BDF):
        stats.append(stress_lockstep(seed=s))
    start_worst = max(stress_starting_recovery(seed=s ^ 0x53544152) for s in (1, 7, 1234, 0xDEADBEEF))
    avg = sum(x[0] for x in stats) / len(stats)
    worst = max(x[1] for x in stats)
    print(f"adhoc protocol v6 stress: OK avg_wait_ms={avg:.2f} worst_wait_ms={worst} start_worst_ms={start_worst}")

#ifndef LF2_PC_NET_H
#define LF2_PC_NET_H

#include <stdint.h>

typedef enum {
    LF2_PC_PROBE_IDLE=0,
    LF2_PC_PROBE_NET_INIT,
    LF2_PC_PROBE_CONNECTING,
    LF2_PC_PROBE_BANNER,
    LF2_PC_PROBE_IDENTITY,
    LF2_PC_PROBE_SEED,
    LF2_PC_PROBE_COMPATIBLE,
    LF2_PC_PROBE_ERROR,
    LF2_PC_PROBE_CANCELLED
} lf2_pc_probe_phase_t;

typedef struct {
    volatile int running;
    volatile int phase;
    int last_error;
    int inet_state;
    char target_ip[16];
    char message[128];
    char host_slots[9];
    char host_names[4][11];
    uint32_t seed_hash;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t elapsed_ms;
} lf2_pc_probe_state_t;

int lf2_pc_init(void);
void lf2_pc_shutdown(void);
int lf2_pc_inet_state(void);
const char *lf2_pc_error(void);

int lf2_pc_probe_start(const char *ipv4);
void lf2_pc_probe_cancel(void);
const lf2_pc_probe_state_t *lf2_pc_probe_state(void);
const char *lf2_pc_probe_phase_name(int phase);


typedef enum {
    LF2_PC_STOCK_IDLE=0,
    LF2_PC_STOCK_LISTENING,
    LF2_PC_STOCK_CONNECTING,
    LF2_PC_STOCK_HANDSHAKE,
    LF2_PC_STOCK_LOCKSTEP,
    LF2_PC_STOCK_ERROR,
    LF2_PC_STOCK_CLOSED
} lf2_pc_stock_phase_t;

typedef enum {
    LF2_PC_STOCK_ROLE_NONE=0,
    LF2_PC_STOCK_ROLE_HOST=1,
    LF2_PC_STOCK_ROLE_CLIENT=2
} lf2_pc_stock_role_t;

typedef struct {
    volatile int running;
    volatile int connected;
    volatile int phase;
    int role;
    int last_error;
    int inet_state;
    char target_ip[16];
    char message[160];
    char peer_slots[9];
    char peer_names[4][11];
    uint32_t seed_hash;
    uint32_t frames;
    uint32_t tu_frames;
    uint32_t stalls;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t last_exchange_ms;
    /* Diagnostics for the 22-byte stock packet fields that are not input bytes.
       Key bytes 4..11 are excluded.  We preserve opaque bytes byte-for-byte and
       only observe changes until their HP/state/checksum semantics are proven. */
    uint32_t opaque_change_events;
    uint32_t opaque_change_mask;
    uint32_t opaque_byte_changes[22];
    uint8_t last_rx[22];
    uint8_t last_tx[22];
} lf2_pc_stock_state_t;

/* Original LF2 2.00a TCP/12345 transport.  Client mode is the Vita equivalent
   of "Connect to opponent" and owns stock control slots 5..8; host mode is the
   Vita equivalent of "Waiting for opponent" and owns slots 1..4.  After the
   14/77/3001-byte handshake, lf2_pc_stock_exchange() performs one blocking
   22-byte LF2 lockstep exchange. */
int lf2_pc_stock_join_start(const char *ipv4);
int lf2_pc_stock_host_start(void);
void lf2_pc_stock_stop(void);
const lf2_pc_stock_state_t *lf2_pc_stock_state(void);
int lf2_pc_stock_ready(void);
int lf2_pc_stock_role(void);
uint32_t lf2_pc_stock_seed_hash(void);
const uint8_t *lf2_pc_stock_seed_data(void);
uint8_t lf2_pc_stock_encode_keys(uint32_t remote_mask);
uint32_t lf2_pc_stock_decode_keys(uint8_t key_byte);
int lf2_pc_stock_exchange(const uint32_t local_masks[4], uint32_t remote_masks[4], uint32_t timeout_ms);
/* Original LF2 responds on a two-game-frame network cadence.  Call this once
   per 30 Hz LF2 time unit; it samples/exchanges on the first TU of each pair
   and returns the same effective local/remote controls for both TUs. */
void lf2_pc_stock_clock_reset(void);
int lf2_pc_stock_clock_step(uint32_t local_masks[4], uint32_t remote_masks[4], uint32_t timeout_ms);

typedef enum {
    LF2_PC_BRIDGE_IDLE=0,
    LF2_PC_BRIDGE_CONNECTING,
    LF2_PC_BRIDGE_HANDSHAKE,
    LF2_PC_BRIDGE_CONNECTED,
    LF2_PC_BRIDGE_ERROR,
    LF2_PC_BRIDGE_CLOSED
} lf2_pc_bridge_phase_t;

typedef struct {
    volatile int running;
    volatile int connected;
    volatile int phase;
    int last_error;
    int inet_state;
    char target_ip[16];
    char message[160];
    volatile uint32_t input_mask;
    uint32_t input_packets;
    uint32_t frames_sent;
    uint32_t frames_dropped;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t last_input_age_ms;
} lf2_pc_bridge_state_t;

/* LF2Vita PC companion protocol.  Unlike the stock-LF2 probe above, this is
   an authoritative Vita simulation: the PC sends player-2 input and receives
   a low-bandwidth live view.  This avoids deterministic desync while the
   native Vita runtime is still converging on exact Windows-LF2 behavior. */
int lf2_pc_bridge_connect(const char *ipv4);
void lf2_pc_bridge_disconnect(void);
const lf2_pc_bridge_state_t *lf2_pc_bridge_state(void);
uint32_t lf2_pc_bridge_input_mask(void);
int lf2_pc_bridge_alive(void);
void lf2_pc_bridge_submit_frame(void);
void lf2_pc_bridge_match_info(const char *p1,const char *p2,const char *stage,int difficulty,int running);

#endif

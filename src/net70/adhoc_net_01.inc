#include "adhoc_net.h"

#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/adhoc_matching.h>
#include <psp2/pspnet_adhoc.h>
#include <psp2/pspnet_adhocctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <psp2/io/fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define LF2_MATCH_PORT 0x4C32 /* 'L2' */
#define LF2_MATCH_POOL (256*1024)
#define LF2_NET_POOL (1024*1024)
#define LF2_MATCH_RX 0x4000
#define LF2_MATCH_HELLO_US 500000
#define LF2_MATCH_PING_US 500000
#define LF2_MATCH_RETRY_COUNT 5
#define LF2_MATCH_REXMT_US 30000
#define LF2_EVENT_QUEUE_CAP 256
#define LF2_MATCH_INPUT_WINDOW 8
#define LF2_MATCH_CACHE_CAP 64
#define LF2_NATIVE_RESEND_US 8000ULL
#define LF2_NATIVE_BARRIER_RESEND_US 50000ULL
#define LF2_NATIVE_FRAME_TIMEOUT_US 1500000ULL
#define LF2_NATIVE_BARRIER_TIMEOUT_US 5000000ULL
/* Matching creates an internal Vita user thread. 0x40 is a PSP-era priority
   and is not a valid Vita user-thread priority. Keep it in the same range as
   LF2Vita's audio worker and give the matching worker a full 64 KiB stack. */
#define LF2_MATCH_THREAD_PRIORITY 0x10000100
#define LF2_MATCH_THREAD_STACK    0x10000
#define LF2_MATCH_THREAD_AFFINITY SCE_KERNEL_CPU_MASK_USER_2
#define LF2_NET_MAGIC 0x304E324Cu /* L2N0 */
#define LF2_HELLO_MAGIC 0x4832464Cu /* LF2H */

/* PSP/Vita matching event values used by the compatibility API. */
enum {
    MATCH_HELLO=1, MATCH_JOIN=2, MATCH_LEFT=3, MATCH_REJECT=4,
    MATCH_CANCEL=5, MATCH_ACCEPT=6, MATCH_COMPLETE=7, MATCH_TIMEOUT=8,
    MATCH_ERROR=9, MATCH_DISCONNECT=10, MATCH_DATA=11,
    MATCH_DATA_CONFIRM=12, MATCH_DATA_TIMEOUT=13
};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t proto;
    uint8_t players;
    uint8_t max_players;
    uint8_t status;
    uint8_t mode;
    uint8_t stage;
    uint8_t difficulty;
    uint8_t reserved;
    uint32_t revision;
    uint32_t avatar_hash;
    char host[18];
    char game_version[LF2_NET_VERSION_MAX];
    char vita_version[LF2_NET_VERSION_MAX];
} hello_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t proto;
    uint8_t probe;
    uint8_t character;
    uint8_t partner_character;
    uint8_t battle_follower;
    uint8_t battle_count;
    uint32_t avatar_hash;
    uint32_t peer_id;
    char name[18];
    char game_version[LF2_NET_VERSION_MAX];
    char vita_version[LF2_NET_VERSION_MAX];
} join_t;

typedef enum {
    PKT_LOBBY=1, PKT_PLAYER=2, PKT_PING=3, PKT_PONG=4,
    PKT_MATCH_INPUT=5
} packet_type_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t proto;
    uint8_t type;
    uint16_t size;
    uint32_t seq;
} packet_hdr_t;

typedef struct __attribute__((packed)) {
    packet_hdr_t h;
    uint32_t revision;
    lf2_net_settings_t settings;
    lf2_net_player_t players[LF2_ADHOC_MAX_PLAYERS];
} lobby_packet_t;

typedef struct __attribute__((packed)) {
    packet_hdr_t h;
    uint8_t character;
    uint8_t partner_character;
    uint8_t battle_follower;
    uint8_t battle_count;
    uint8_t ready;
    uint8_t reserved[3];
} player_packet_t;

typedef struct __attribute__((packed)) {
    packet_hdr_t h;
    uint64_t stamp_us;
} ping_packet_t;

typedef struct __attribute__((packed)) {
    packet_hdr_t h;
    uint32_t match_id;
    /* Each packet carries the newest scheduled input plus up to seven older
       frames. Any later packet can therefore repair a short loss/reorder burst
       without changing the deterministic one-TU input delay. */
    uint32_t first_frame;
    uint32_t newest_frame;
    uint32_t ack_frame;
    uint32_t state_tu;
    uint32_t state_digest;
    uint8_t count;
    uint8_t reserved[3];
    uint32_t held[LF2_MATCH_INPUT_WINDOW];
} match_input_packet_t;

typedef struct {
    bool valid;
    uint32_t match_id;
    uint32_t frame;
    uint32_t held;
} match_input_cache_t;

typedef struct {
    int event;
    SceNetInAddr addr;
    int len;
    uint8_t data[1024];
    uint64_t when_us;
} queued_event_t;

typedef struct {
    bool valid;
    bool probe;
    SceNetInAddr addr;
    join_t join;
} pending_t;

typedef struct {
    bool used;
    SceNetInAddr addr;
    int slot;
    uint16_t ping_ms;
    uint64_t ping_sent_us;
} peer_t;

static bool g_init;
static int g_match=-1;
static bool g_browser;
static bool g_host;
static void *g_net_mem;
static void *g_match_mem;
static char g_name[18]="Vita Player";
static uint32_t g_avatar_hash;
static uint32_t g_local_peer_id;
static char g_error[160];
static char g_diag[192]="Netzwerk noch nicht gestartet";
static int g_adhoc_state=-1;
static int g_adhoc_state_rc=0;
static queued_event_t g_events[LF2_EVENT_QUEUE_CAP];
static volatile int g_ev_head,g_ev_tail;
static SceKernelLwMutexWork g_event_mutex;
static SceKernelLwMutexWork g_input_mutex;
static lf2_adhoc_session_t g_sessions[LF2_ADHOC_MAX_SESSIONS];
static int g_session_count;
static lf2_adhoc_lobby_t g_lobby;
static pending_t g_pending[LF2_ADHOC_MAX_PLAYERS+4];
static peer_t g_peers[LF2_ADHOC_MAX_PLAYERS];
static uint32_t g_seq=1;
static int g_probe_index=-1;
static uint64_t g_probe_started;
static SceNetInAddr g_probe_addr;
static uint64_t g_last_ping;
static bool g_joining;
static uint32_t g_probe_ignore_addr;
static uint64_t g_probe_ignore_until;
static SceNetInAddr g_join_host_addr;
static bool g_have_join_host;
static uint8_t g_local_character;
static uint8_t g_local_partner;
static uint8_t g_local_battle_follower;
static uint8_t g_local_battle_count;
static uint8_t g_local_ready;
static uint32_t g_last_logged_revision;
static bool g_native_match_active;
static bool g_native_have_peer;
static SceNetInAddr g_native_peer_addr;
static uint32_t g_native_match_id;
static uint32_t g_native_match_frame;
static uint32_t g_native_prev_physical;
static uint64_t g_native_wait_total_us;
static uint64_t g_native_wait_max_us;
static uint32_t g_native_wait_samples;
static uint32_t g_native_buffer_hits;
static uint32_t g_native_retransmits;
static uint32_t g_native_data_timeouts;
static uint32_t g_native_soft_drops;
static uint32_t g_native_fast_rx;
static uint32_t g_native_start_refreshes;
static uint32_t g_event_drops;
static uint32_t g_event_peak;
static uint32_t g_native_peer_highest;
static uint32_t g_native_peer_ack;
static bool g_native_peer_highest_valid;
static bool g_native_peer_ack_valid;
static uint32_t g_native_prefetch_deferred;
static uint32_t g_native_ack_suppressed;
static uint32_t g_native_barrier_ack_waits;
static uint32_t g_native_state_tu;
static uint32_t g_native_state_digest;
static uint32_t g_native_peer_state_tu;
static uint32_t g_native_peer_state_digest;
static uint32_t g_native_desyncs;
static bool g_native_desync;
static uint64_t g_last_lobby_broadcast;
static uint64_t g_last_player_refresh;
static match_input_cache_t g_match_inputs[LF2_MATCH_CACHE_CAP];
static match_input_cache_t g_local_inputs[LF2_MATCH_CACHE_CAP];
static bool g_mutex_ready;
static bool g_input_mutex_ready;

static bool fast_match_input(const SceNetInAddr *addr,const void *data,int len);
static bool g_mod_net_loaded,g_mod_adhoc_loaded,g_mod_matching_loaded;
static bool g_net_ready,g_netctl_ready,g_adhoc_ready,g_ctl_ready,g_matching_ready;

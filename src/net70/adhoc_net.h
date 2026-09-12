#ifndef LF2_ADHOC_NET_H
#define LF2_ADHOC_NET_H

#include <stdbool.h>
#include <stdint.h>

#define LF2_ADHOC_MAX_PLAYERS 8
#define LF2_ADHOC_MAX_SESSIONS 12
#define LF2_ADHOC_NAME_MAX 17
#define LF2_ADHOC_PROTO_VERSION 4
#define LF2_GAME_VERSION "2.00a"
#define LF2_VITA_VERSION "0.70"
#define LF2_NET_VERSION_MAX 8

typedef enum {
    LF2_NET_MODE_VS=0,
    LF2_NET_MODE_STAGE=1,
    LF2_NET_MODE_BATTLE=2,
    LF2_NET_MODE_1V1=3,
    LF2_NET_MODE_2V2=4
} lf2_net_game_mode_t;

typedef enum {
    LF2_NET_STATUS_WAITING=0,
    LF2_NET_STATUS_STARTING=1,
    LF2_NET_STATUS_IN_GAME=2,
    LF2_NET_STATUS_RESULTS=3
} lf2_net_status_t;

typedef struct {
    char name[LF2_ADHOC_NAME_MAX+1];
    uint8_t character;
    uint8_t team;
    uint8_t ready;
    uint8_t present;
    uint32_t avatar_hash;
    uint32_t peer_id;
    uint16_t ping_ms;
} lf2_net_player_t;

typedef struct {
    uint8_t mode;
    uint8_t max_players;
    uint8_t status;
    uint8_t difficulty;
    uint8_t stage;
    uint8_t friendly_fire;
    uint8_t allow_cheat;
    uint8_t reserved;
} lf2_net_settings_t;

typedef struct {
    uint32_t address;
    char host[LF2_ADHOC_NAME_MAX+1];
    uint32_t avatar_hash;
    uint8_t players;
    uint8_t max_players;
    uint8_t status;
    uint8_t mode;
    uint8_t stage;
    uint8_t difficulty;
    uint16_t ping_ms;       /* 0xffff until a probe RTT exists */
    uint32_t revision;
    char game_version[LF2_NET_VERSION_MAX];
    char vita_version[LF2_NET_VERSION_MAX];
    uint64_t last_seen_us;
} lf2_adhoc_session_t;

typedef struct {
    bool active;
    bool host;
    bool connected;
    int local_slot;
    lf2_net_settings_t settings;
    lf2_net_player_t players[LF2_ADHOC_MAX_PLAYERS];
    uint32_t revision;
    uint16_t host_ping_ms;
} lf2_adhoc_lobby_t;

int lf2_adhoc_init(void);
void lf2_adhoc_shutdown(void);
const char *lf2_adhoc_local_name(void);
uint32_t lf2_adhoc_local_avatar_hash(void);
const char *lf2_adhoc_error(void);
const char *lf2_adhoc_game_version(void);
const char *lf2_adhoc_vita_version(void);

int lf2_adhoc_browser_start(void);
void lf2_adhoc_browser_stop(void);
void lf2_adhoc_browser_update(void);
int lf2_adhoc_browser_count(void);
const lf2_adhoc_session_t *lf2_adhoc_browser_session(int index);
int lf2_adhoc_browser_probe(int index);
int lf2_adhoc_join(int index, int character);

int lf2_adhoc_host_start(const lf2_net_settings_t *settings, int character);
void lf2_adhoc_lobby_update(void);
const lf2_adhoc_lobby_t *lf2_adhoc_lobby(void);
int lf2_adhoc_lobby_set_character(int character);
int lf2_adhoc_lobby_set_ready(bool ready);
int lf2_adhoc_lobby_host_settings(const lf2_net_settings_t *settings);
int lf2_adhoc_lobby_host_status(lf2_net_status_t status);

/* Native Vita-to-Vita 30 Hz lockstep. Protocol v4 keeps one-TU input delay,
   adds an 8-frame recovery window, reliable lobby refresh, soft DATA_TIMEOUT
   handling and bounded retransmission. The first simulation TU is a neutral
   post-load barrier, so both Vitas finish loading before gameplay advances. */
int lf2_adhoc_match_begin(uint32_t match_id);
bool lf2_adhoc_lockstep_frame(void *userdata, uint32_t local_held[4], uint32_t remote_held[4]);
void lf2_adhoc_match_report_state(void *userdata, uint32_t tu, uint32_t digest);
void lf2_adhoc_match_end(void);

void lf2_adhoc_leave(void);

const char *lf2_net_mode_name(int mode);
const char *lf2_net_status_name(int status);
int lf2_net_ping_bars(uint16_t ping_ms);

#endif

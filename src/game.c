/* LF2Vita v0.69 fix3 generated runtime split. */
#include "fix3/game_01.inc"

/* game_02.inc historically crosses a function boundary. Keep the original
   generated fragment intact for reference, but compile it through two exact
   slices so the stock wrappers are inserted at a real top-level C boundary. */
#define in_action stock_base_in_action
#include "fix3/game_02_prefix.inc"
#undef in_action

/* Public stock-data exports, diagnostics and the stock packet input adapter
   depend only on the core fighter/object types established above, so keep
   them at this known top-level boundary rather than inside later split
   functions. */
#include "fix3/game_stock_exports.inc"
#define stock_state_digest stock_base_state_digest
#include "fix3/game_stock_diag.inc"
#undef stock_state_digest
#include "fix3/game_stock_input.inc"

/* The hitlag layer owns the first-stage defend/update wrappers. Rename those
   symbols while including it so the armor layer can add the hard-coded stock
   character armor and caught-recovery exception behind them. */
#define stock_try_defend_hit stock_hitlag_try_defend_hit
#define update_control stock_hitlag_update_control
#include "fix3/game_stock_hitlag.inc"
#undef update_control
#undef stock_try_defend_hit
#include "fix3/game_stock_armor.inc"

#include "fix3/game_02_suffix.inc"
#include "fix3/game_03.inc"

#define make_ai_input stock_base_make_ai_input
#include "fix3/game_04.inc"
#undef make_ai_input

#define update_control stock_base_update_control
#include "fix3/game_05.inc"
#undef update_control

#define handle_catch_input stock_base_handle_catch_input
#include "fix3/game_06.inc"
#undef handle_catch_input
#include "fix3/game_07.inc"

#define fighter_handle_item_input stock_base_fighter_handle_item_input
#include "fix3/game_08.inc"
#undef fighter_handle_item_input

#define spawn_fighter_opoints stock_base_spawn_fighter_opoints
#include "fix3/game_09.inc"
#undef spawn_fighter_opoints
#include "fix3/game_10.inc"
#include "fix3/game_11.inc"

#define stock_try_defend_hit stock_base_try_defend_hit
#define stock_note_unblocked_hit stock_base_note_unblocked_hit
#include "fix3/game_12.inc"
#undef stock_note_unblocked_hit
#undef stock_try_defend_hit

#define object_apply_hits stock_base_object_apply_hits
#include "fix3/game_13.inc"
#undef object_apply_hits

#define apply_hits stock_base_apply_hits
#include "fix3/game_14.inc"
#include "fix3/game_15.inc"
#undef apply_hits

#define make_ai_retreat_input stock_base_make_ai_retreat_input
#include "fix3/game_16.inc"
#undef make_ai_retreat_input

#include "fix3/game_17.inc"

#define process_frame_state stock_base_process_frame_state
#include "fix3/game_18.inc"
#undef process_frame_state

#define handle_form_input stock_base_handle_form_input
#include "fix3/game_19.inc"
#undef handle_form_input

/* The generated game_20 split is intentionally included verbatim.  Keep this
   compatibility alias local to the include so an accidental shortened token
   in that fragment cannot leak into later runtime pieces. */
#define remote_pre remote_prev
#include "fix3/game_20.inc"
#undef remote_pre
#include "fix3/game_21.inc"
#include "fix3/game_22.inc"
#include "fix3/game_23.inc"
#include "fix3/game_24.inc"

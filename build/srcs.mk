# Link orders of all modules (build/<module>.mk picks one as MOD_SRC).
#
# Entries ending in .asm are passed to q3asm as-is; all others are compiled from <name>.c.
# Both are looked up in the module's SRCDIR first, then in GAMEDIR (build/Makefile). q3asm lays
# out code/data/lit in exactly this order, so the order is part of the output.
#
# QA_SRC: OSP 1.03a qagame order: id's code/game/game.q3asm order (not Conscript) with OSP's
# 19 files inserted: bg_auth after g_main (where game.q3asm lists g_syscalls), bg_promode
# after bg_pmove, g_osp_freeze between g_combat and g_items, and 16 g_osp_* files appended
# after g_weapon. 52 C files + g_syscalls.asm (equ only, no code or data, so its position
# does not change the output).
QA_SRC = \
 g_main g_syscalls.asm bg_auth \
 bg_misc bg_lib bg_pmove bg_promode bg_slidemove \
 q_math q_shared \
 ai_dmnet ai_dmq3 ai_team ai_main ai_chat ai_cmd ai_vcmd \
 g_active g_arenas g_bot g_client g_cmds g_combat g_osp_freeze g_items g_mem g_misc \
 g_missile g_mover g_session g_spawn g_svcmds g_target g_team \
 g_trigger g_utils g_weapon \
 g_osp_admin g_osp_bot g_osp_cmds g_osp_config g_osp_customgfx g_osp_decals \
 g_osp_main g_osp_maps g_osp_motd g_osp_multiview g_osp_referee g_osp_restrict \
 g_osp_svcmds g_osp_team g_osp_viewcam g_osp_vote g_botmem

# UI_SRC: OSP ui order: id's q3_ui.q3asm order, i.e. ui.q3asm without ui_loadconfig /
# ui_saveconfig. bg_misc, bg_lib, q_math and q_shared come from src/game (-DQ3UI);
# ui_syscalls.asm is equ only.
UI_SRC =  ui_main ui_syscalls.asm ui_gameinfo ui_atoms ui_cinematics ui_connect ui_controls2  ui_demo2 ui_mfield ui_credits ui_menu ui_ingame ui_confirm ui_setup ui_options ui_display  ui_sound ui_network ui_playermodel ui_players ui_playersettings ui_preferences ui_qmenu  ui_serverinfo ui_servers2 ui_sparena ui_specifyserver ui_sppostgame ui_splevel ui_spskill   ui_startserver ui_team ui_video ui_addbots ui_removebots ui_teamorders ui_cdkey ui_mods  bg_misc bg_lib q_math q_shared

# CG_SRC: OSP cgame order: id's cgame.q3asm order with cg_osp_colors after cg_weapons, then
# OSP's qagame files bg_auth and bg_promode, then the remaining cg_osp_* files. The shared
# bg_* / q_* files come from src/game (-DCGAME); cg_syscalls.asm is equ only.
CG_SRC =  cg_main cg_syscalls.asm cg_consolecmds cg_draw cg_drawtools cg_effects cg_ents  cg_event cg_info cg_localents cg_marks cg_players cg_playerstate cg_predict  cg_scoreboard cg_servercmds cg_snapshot cg_view cg_weapons cg_osp_colors  bg_slidemove bg_pmove bg_lib bg_misc q_math q_shared  bg_auth bg_promode  cg_osp_freeze cg_osp_cmds cg_osp_cpmhud cg_osp_chs cg_osp_hud cg_osp_customloc  cg_osp_hmicmds cg_osp_hmi cg_osp_main cg_osp_scoreboard cg_osp_draw cg_osp_malloc

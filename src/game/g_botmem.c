/*
===========================================================================

OSP 1.04b botmem (bot decision memory)

P1: logs bot goal decisions and their outcomes to
bot_memory/<map>.mem in the mod directory (fs_homepath+fs_gamedir).
Line format:
  dec  <bot> <goal> <kind> <time_ms>      new goal decision (kind 0=LTG 1=NBG)
  good <bot> <goal> <kind> <delta_ms>     decision survived (next decision / 60 s)
  bad  <bot> <goal> <kind> <delta_ms>     bot died while decision pending
  open <bot> <goal> <kind> <delta_ms>     still pending at map end
   stat <goal> <good> <bad>                end-of-map summary
   end  <maptime_ms> 0 0 0
Controlled by cvar bot_memory (1 = on, 0 = off; DEFAULT 0 = off,
2026-11-02 - live servers opt in with +set bot_memory 1).

Per-map learning: good/bad counters are stored per map in
bot_memory/learn_<map>.txt (same 65541 B layout as the old global
learn.txt) - each map is loaded at map start, saved at map end.
v2 (2026-11-03): "learn2" (6 B) + 2 x 8192 x 4 pooled +
2 x 3072 x 4 per-(bot,classname,health band) = 90118 B.
v1 ("learn" 5 B header, 65541 B, pooled only) is still accepted
(band section stays zero) - the seed tools write v1.

Paramount map tactics: cvar botmem_paramount_<map> (space-separated
goal classnames) lists goals the bot must always pursue - they get a
fixed weight boost (botmem_paramount_boost, default 16) INSTEAD of the
learned scale (2026-11-02).

Per-feature toggles (2026-11-04): bot_learn_enable (learning/collection:
the per-map .mem decision log, the good/bad/kill/tk outcome counting and
the learn file persistence; evaluated at map start - file open = collect
= save) and bot_learn_use (use of the learned data: the learned-scale
pass, bot_memo_boost, the paramount boost and the 1x/s band re-apply;
read LIVE, a rcon toggle takes effect immediately). Both default 1,
subordinate to bot_memory.

Engine capability probe (2026-11-05): the DWC botmem syscalls (traps
-702..-705) exist only in the patched engine (doomwarriors-clan/Quake3e).
At G_Init the QVM probes the four "_DWC" names via the stock
trap_GetValue (trap -701, the engine answers them with the syscall enum
values); on a stock engine botmem_engine_ok stays 0 and the QVM runs in
log-only mode (the .mem decision log + the pooled outcome counting keep
working, the engine side of BotMem_BotLearn is skipped, one warning per
VM restart). One QVM is therefore safe on both engine types.

Ported to the real OSP 1.03a source tree (2026-11-05): the raw
decompiled client offsets are replaced by typed accesses (layout
verified identical by a 32-bit compile against the tree headers:
sizeof(gclient_t) = 1448, ps.pm_type @ 4, ps.stats[STAT_HEALTH] @ 184,
ps.persistant[PERS_SCORE] @ 248; in this tree PERS_SCORE = 0 - there is
no PERS_STATE). The .mem and learn-file formats are unchanged, so the
knowledge accumulated by the decompiled osp1.04b build carries over.

Combat-weapon learning (2026-11-09, clan issue #2): OSP's fight weapon is
chosen by the hardcoded distance table (OSP_BotChooseWeaponByRange in
ai_dmq3.c) - the _w.c/fw_weap.c weapon weights are never read for it. So the
QVM learns per (bot, weapon 1..9, health band, map) whether holding a weapon
correlates with kills (good) or deaths (bad), and BotMem_FightGate rejects
weapons with a persistently bad record inside the distance table (and both
height-override branches). Rejection only, no forcing (the table has no
weight channel). Pending combat decisions are recorded by BotMem_FightWeapon
(hook at the end of BotChooseWeapon, fired on every call, ~10x/s per bot
while fighting) and resolved every frame in BotMem_Frame: score up (kill)
-> good, score down while alive (teamkill) -> bad, death -> bad; a weapon
change or 60 s without any combat event -> expire, no count (holding without
fighting is not a combat signal - deliberate difference to the goal side,
where 60 s survival counts as good). Counters persist per map in the learn
file v3
("learn3", 97030 B, v1/v2 load unchanged); .mem tags fdec/fgood/fbad; flearn
lines at map end (BotMem_MapEnd, works in log-only mode); cvars
botmem_fight_min (3) + botmem_fight_threshold (25), subordinate to
bot_memory/bot_learn_enable/bot_learn_use (the gate reads bot_learn_use
live). No engine change - works on every engine type.

Warmup / intermission collection gate (2026-11-10, clan issue #4): during
the OSP warmup (g_doWarmup ready-wait or countdown) and the intermission
frags neither score nor log, so the PERS_SCORE diff can only see deaths -
every warmup death would resolve as "bad" (combat: fbad; goal: bad) and no
"good" can ever be recorded, poisoning the per-map learn counters before
the real match. So while level.warmupTime != 0 or level.intermissiontime
!= 0, NO new pending decisions are opened (BotMem_LogDecision /
BotMem_FightWeapon) and pending ones are dropped WITHOUT counting
(BotMem_Frame) - the .mem log, the good/bad/kill/tk counters and the
flearn summary stay clean. The APPLICATION side is intentionally NOT
gated: BotMem_FightGate, the learned-scale pass (BotMem_BotLearn), the
paramount boost and the 1x/s band re-apply all keep running, so a bot
still plays with the knowledge it has.

===========================================================================
*/

#include "g_local.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_ai_goal.h"
#include "ai_main.h"

/*
 OSP 1.04b botmem: BSS state.

The table sizes (and thus the learn-file v2 layout: 8192 pooled x 2 +
3072 band x 2 int32 counters) match the decompiled build exactly.
Counters are indexed DIRECTLY by botnum (clientnum), NOT by a dynamic
per-VM "bot index": the engine restarts the game VM on every match
restart (VM_Restart), which zeroes the BSS and re-rolled a
first-decision-order slot assignment every map (a seed in slot 0 was
applied to a DIFFERENT bot on each map). clientnum is stable across map
changes, so botnum is the correct persistent key. Bots with
clientnum >= 32 simply do not learn (table bound).
*/

static int	botmem_enabled = 0;
/* 2026-11-05: set by the trap_GetValue probe in BotMem_InitGame when the
   engine offers the DWC botmem syscalls (the "_DWC" names in
   SV_GetValue). 0 = stock engine without them -> log-only mode. BSS, so
   it is re-probed on every VM restart (the engine restarts the game VM
   per match). */
static int	botmem_engine_ok = 0;
static int	botmem_file = 0;
static int	botmem_handle = 0;
static char	botmem_map[64];
static char	botmem_path[128];
static int	botmem_goal[1024];
static int	botmem_kind[1024];
static int	botmem_time[1024];
static int	botmem_seen[1024];
static int	botmem_active[1024];
static int	botmem_nactive = 0;
static int	botmem_good[256];
static int	botmem_bad[256];
static int	botmem_bgood[8192];
static int	botmem_bbad[8192];
static int	botmem_loaded = 0;
static int	botmem_boost = 0;
static int	botmem_goalmap_done = 0;
static char	botmem_lmap[128];
static int	botmem_applied[1024];
/* 2026-11-03: the re-apply key is the goalstate's item WEIGHT CONFIG
   generation counter (engine trap BotGoalWeightConfigPtr, -702) - the
   engine REUSES goalstate handles across bots, so the handle is NOT a
   reliable "same bot" identity; the config changes on every re-add. */
static int	botmem_scaled_cfg[1024];
static int	botmem_gs[1024];
static int	botmem_score[1024];
static int	botmem_band[1024];
static int	botmem_goalcn[256];
/* 2026-11-03 (finer buckets): map-global classname table (first-
   occurrence order from the goalmap, max 32 x 32 B, stable per map/AAS)
   + the goal -> classname index mapping (-1 = nameless goal). */
static char	botmem_cnames2[1024];
static int	botmem_cnames2n = 0;
/* per-(bot,classname,band) outcome counters: idx (bot*32+cn)*3+band */
static int	botmem_bg[3072];
static int	botmem_bb[3072];
/* last-applied scale per (bot,classname) as float-bits + 1 (0 = not
   applied; zeroed per new weight config so the first application is
   ABSOLUTE, later ones RATIO) */
static int	botmem_last[1024];
static int	botmem_applytime[32];
static char	botmem_parmname[64];
/* 2026-11-05 port fix: the decompiled build reads the cvar value with size 255
   into a 64-byte buffer (a latent overflow, only contained by the following
   memset of the adjacent BSS). 256 B is the correct size (8 names x 31 chars
   + separators); the BSS layout is not part of the learn-file format. */
static char	botmem_parmval[256];
static char	botmem_parm[256];
static int	botmem_parmn = 0;
static int	botmem_parmboost = 16;
/* 2026-11-09 (combat-weapon learning, clan issue #2): the pending combat
   decision per bot (fweapon 0 = no pending) + the per-(bot, weapon 1..9,
   health band) outcome counters. idx (bot*9 + (weapon-1))*3 + band ->
   32*9*3 = 864. The grapple (weapon 10) is not a fight weapon and is not
   tracked. fmin/fthresh cache the botmem_fight_min / botmem_fight_threshold
   cvar values (set in BotMem_InitGame). */
static int	botmem_fweapon[32];
static int	botmem_fband[32];
static int	botmem_fscore[32];
static int	botmem_ftime[32];
static int	botmem_fgood[864];
static int	botmem_fbad[864];
static int	botmem_fmin = 3;
static int	botmem_fthresh = 25;

/*
 Forward declarations (LCC 4.1: no implicit declarations). Only the
 functions used before their definition need them here.
*/
static void BotMem_SaveLearn( void );
static void BotMem_BotGoalCount( int botnum, int goal, int isgood );
static void BotMem_BotGoalCountB( int botnum, int goal, int band, int isgood );
static int	 BotMem_HealthBand( int botnum );
static void BotMem_DesiredScale( int bi, int cn2, int band, float *out );
static void BotMem_ApplyScales( int bi, int goalstate );
static int	 BotMem_IsParamount( char *name );

/* float <-> IEEE754 bits via memcpy (4 bytes, no sign issues); the QVM uses
   the engine's VM-import memset/memcpy (Com_Mem* are not in the QVM build) */
static int BotMem_FbitsI( float f ) {
	int i;

	memcpy( &i, &f, 4 );
	return i;
}

static float BotMem_FbitsF( int i ) {
	float f;

	memcpy( &f, &i, 4 );
	return f;
}

/* level.time in ms */
static int BotMem_LevelTime( void ) {
	return level.time;
}

/*
=================
BotMem_MatchLive

1 = the match is live, 0 = warmup (countdown, or waiting-for-ready under
g_doWarmup) or intermission.

Clan issue #4: during the OSP warmup frags neither score nor log, so the
PERS_SCORE diff that credits kills can only ever go DOWN (deaths) - every
warmup death resolves as "bad" and no "good" can ever be recorded. The
counters persist per map in the learn file, so the warmup noise would
carry into real matches (a weapon can hit botmem_fight_min samples at 0 %
success and get rejected by the gate before the match even starts). No
outcome data is credible while the match is not live, so collection is
gated on this.

The match is live exactly when level.warmupTime == 0: every warmup /
ready / waiting state sets it to -1 (waiting for players/ready) or to a
future level.time (countdown), and the in-progress path of the team code
(G_CheckLiveness) returns early on level.warmupTime == 0. Intermission is
flagged by level.intermissiontime != 0 (the match has ended, no further
scoring).
=================
*/
static int BotMem_MatchLive( void ) {
	if ( level.intermissiontime != 0 ) {
		return 0;
	}
	if ( level.warmupTime != 0 ) {
		return 0;
	}
	return 1;
}

static int BotMem_Int( char *dst, int v ) {
	int n, i, m;
	unsigned u;
	char t[12];

	n = 0;
	if ( v < 0 ) {
		*dst++ = '-';
		v = -v;
		n = 1;
	}
	u = (unsigned)v;
	m = 11;
	t[11] = 0;
	do {
		t[m] = (char)('0' + (int)(u % 10u));
		m = m - 1;
		u = u / 10u;
	} while ( u > 0u );
	for ( i = m + 1; i < 12; i++ ) {
		dst[n] = t[i];
		n = n + 1;
	}
	dst[n] = 0;
	return n;
}

static int BotMem_Write( const char *s ) {
	int n;

	n = 0;
	while ( s[n] != 0 ) n = n + 1;
	if ( n > 0 && botmem_file != 0 ) {
		trap_FS_Write( s, n, botmem_file );
	}
	return 0;
}

static int BotMem_LogTag( const char *tag, int a, int b, int c, int d ) {
	char line[80];
	char *p;
	int n, i;

	p = line;
	for ( i = 0; tag[i] != 0; i++ ) *p++ = tag[i];
	*p++ = ' ';
	n = BotMem_Int( p, a );
	p += n;
	*p++ = ' ';
	n = BotMem_Int( p, b );
	p += n;
	*p++ = ' ';
	n = BotMem_Int( p, c );
	p += n;
	*p++ = ' ';
	n = BotMem_Int( p, d );
	p += n;
	*p++ = '\n';
	*p = 0;
	BotMem_Write( line );
	return 0;
}

static int BotMem_LogStr( const char *tag, const char *s ) {
	char line[128];
	char *p;
	int i;

	p = line;
	for ( i = 0; tag[i] != 0; i++ ) *p++ = tag[i];
	*p++ = ' ';
	for ( i = 0; s[i] != 0 && (p - line) < 120; i++ ) *p++ = s[i];
	*p++ = '\n';
	*p = 0;
	BotMem_Write( line );
	return 0;
}

/*
=================
BotMem_MapStart

Per-map state reset + (bot_learn_enable != 0) open bot_memory/<map>.mem
in APPEND mode (each match appends its own "v1" block).
=================
*/
void BotMem_MapStart( void ) {
	int i, n;
	char *p;

	if ( botmem_enabled == 0 ) {
		return;
	}
	botmem_file = 0;
	botmem_handle = 0;
	/* the engine's "mapname" cvar (CVAR_SERVERINFO|CVAR_ROM, set by the
	   engine in SV_SetMap) - the same cvar the decompiled QVM read */
	trap_Cvar_VariableStringBuffer( "mapname", botmem_map, 63 );
	botmem_map[63] = 0;
	/* "bot_memory/<map>.mem" */
	p = botmem_path;
	memcpy( p, "bot_memory/", 11 );
	n = 11;
	for ( i = 0; i < 63 && botmem_map[i] != 0; i++ ) p[n] = botmem_map[i], n = n + 1;
	p[n] = '.';
	n = n + 1;
	p[n] = 'm';
	n = n + 1;
	p[n] = 'e';
	n = n + 1;
	p[n] = 'm';
	n = n + 1;
	p[n] = 0;
	memset( botmem_goal, 0, sizeof( botmem_goal ) );
	memset( botmem_kind, 0, sizeof( botmem_kind ) );
	memset( botmem_time, 0, sizeof( botmem_time ) );
	memset( botmem_seen, 0, sizeof( botmem_seen ) );
	memset( botmem_active, 0, sizeof( botmem_active ) );
	memset( botmem_good, 0, sizeof( botmem_good ) );
	memset( botmem_bad, 0, sizeof( botmem_bad ) );
	memset( botmem_applied, 0, sizeof( botmem_applied ) );
	memset( botmem_gs, 0, sizeof( botmem_gs ) );
	memset( botmem_score, 0, sizeof( botmem_score ) );
	memset( botmem_band, 0, sizeof( botmem_band ) );
	memset( botmem_goalcn, -1, sizeof( botmem_goalcn ) );
	memset( botmem_cnames2, 0, sizeof( botmem_cnames2 ) );
	memset( botmem_bg, 0, sizeof( botmem_bg ) );
	memset( botmem_bb, 0, sizeof( botmem_bb ) );
	memset( botmem_last, 0, sizeof( botmem_last ) );
	memset( botmem_applytime, 0, sizeof( botmem_applytime ) );
	memset( botmem_fweapon, 0, sizeof( botmem_fweapon ) );
	memset( botmem_fband, 0, sizeof( botmem_fband ) );
	memset( botmem_fscore, 0, sizeof( botmem_fscore ) );
	memset( botmem_ftime, 0, sizeof( botmem_ftime ) );
	memset( botmem_fgood, 0, sizeof( botmem_fgood ) );
	memset( botmem_fbad, 0, sizeof( botmem_fbad ) );
	botmem_cnames2n = 0;
	botmem_nactive = 0;
	botmem_goalmap_done = 0;	/* goal numbers are per-map (per AAS): re-print */
	/* bot_learn_enable 0 (2026-11-04): no collection for this map - the
	   per-map .mem file stays closed, so BotMem_LogDecision / BotMem_Frame /
	   BotMem_MapEnd (all gated on botmem_file) are no-ops for the whole map.
	   The map-cvar read, path build and memsets above still run (BotMem_
	   LearnPath needs botmem_map; the zeroed per-map state stays consistent). */
	if ( trap_Cvar_VariableIntegerValue( "bot_learn_enable" ) == 0 ) {
		return;
	}
	n = trap_FS_FOpenFile( botmem_path, &botmem_handle, FS_APPEND );
	if ( botmem_handle > 0 ) {
		botmem_file = botmem_handle;
		BotMem_Write( "# osp1.04b botmem v1\n" );
		BotMem_LogStr( "mapname", botmem_map );
	}
}

static int BotMem_Float2( char *dst, float v ) {
	int iv, fv, n;

	/* "X.YY" with 2 decimal digits; v is in [0, 16] here */
	iv = (int)v;
	fv = (int)((v - (float)iv) * 100.0f);
	if ( fv < 0 ) fv = 0;
	if ( fv > 99 ) fv = 99;
	n = BotMem_Int( dst, iv );
	dst[n] = '.';
	dst[n + 1] = (char)('0' + (fv / 10));
	dst[n + 2] = (char)('0' + (fv % 10));
	dst[n + 3] = 0;
	return n + 3;
}

/*
=================
BotMem_DropPending

Clears the bot's pending goal decision without counting any outcome
(clan issue #4: warmup / intermission carry no credible outcome data, so
a pending decision must not resolve to good / bad / kill / tk - it is
simply dropped). Mirrors the active-list removal of the BotMem_Frame
close branches, minus the counting and the .mem tag. botmem_gs / score /
band / time are left as-is, exactly like the close branches (the band
re-apply keys off botmem_applied / botmem_gs, not botmem_seen).
=================
*/
static void BotMem_DropPending( int b ) {
	int j;

	if ( botmem_seen[b] == 0 ) {
		return;
	}
	for ( j = 0; j < botmem_nactive; j++ ) {
		if ( botmem_active[j] == b ) {
			botmem_active[j] = botmem_active[botmem_nactive - 1];
			botmem_nactive = botmem_nactive - 1;
			break;
		}
	}
	botmem_seen[b] = 0;
}

/*
=================
BotMem_LogDecision

Called at the three goal-choose sites (kind 0 = LTG, 1 = NBG) right
after the engine trap call - fires on EVERY call, including the final
one that finds no goal (the bot then just has no new pending decision).
Closes the bot's previous pending decision (survived -> good) and opens
the new one, recording the goalstate handle, the score (PERS_SCORE) and
the health band AT DECISION TIME (2026-11-03: the score diff detects
kills / teamkills in BotMem_Frame; the band buckets the outcome for
health-conditional scaling).
=================
*/
void BotMem_LogDecision( bot_state_t *bs, int kind ) {
	int i, j, goalstate, botnum, goal;
	bot_goal_t g;

	if ( botmem_file == 0 ) {
		return;
	}
	goalstate = bs->gs;
	if ( goalstate == 0 ) {
		return;
	}
	botnum = bs->client;
	if ( botnum < 0 || botnum >= MAX_CLIENTS ) {
		return;
	}
	/* Warmup / intermission (clan issue #4): no credible outcome data -
	   drop the previous pending decision without counting and open no new
	   one (the BotMem_BotLearn application pass that follows the call site
	   is intentionally NOT gated). */
	if ( BotMem_MatchLive() == 0 ) {
		BotMem_DropPending( botnum );
		return;
	}
	/* the chosen goal is the TOP of the bot's goal stack. The old code
	   logged the boolean return of the choose traps (0/1) instead, which
	   made every run look like "100% goal 1" (a measurement artifact).
	   BotGetTopGoal fills a bot_goal_t whose `number` field sits at
	   offset 44 (origin 12 + areanum 4 + mins 12 + maxs 12 + entitynum 4);
	   it returns 0 for an empty goal stack. */
	if ( !trap_BotGetTopGoal( goalstate, &g ) ) {
		return;
	}
	goal = g.number;
	if ( goal < 0 || goal >= 256 ) {
		return;
	}
	i = botnum;
	if ( botmem_seen[i] != 0 ) {
		if ( botmem_goal[i] >= 0 && botmem_goal[i] < 256 ) {
			botmem_good[botmem_goal[i]] = botmem_good[botmem_goal[i]] + 1;
			BotMem_BotGoalCountB( i, botmem_goal[i], botmem_band[i], 1 );
		}
		BotMem_LogTag( "good", i, botmem_goal[i], botmem_kind[i], BotMem_LevelTime() - botmem_time[i] );
		for ( j = 0; j < botmem_nactive; j++ ) {
			if ( botmem_active[j] == i ) {
				botmem_active[j] = botmem_active[botmem_nactive - 1];
				break;
			}
		}
		botmem_nactive = botmem_nactive - 1;
		botmem_seen[i] = 0;
	}
	botmem_goal[i] = goal;
	botmem_kind[i] = kind;
	botmem_time[i] = BotMem_LevelTime();
	botmem_gs[i] = goalstate;
	botmem_score[i] = level.clients[i].ps.persistant[PERS_SCORE];
	botmem_band[i] = BotMem_HealthBand( i );
	botmem_seen[i] = 1;
	if ( botmem_nactive < 1024 ) {
		botmem_active[botmem_nactive] = i;
		botmem_nactive = botmem_nactive + 1;
	}
	BotMem_LogTag( "dec", i, goal, kind, BotMem_LevelTime() );
}

/*
=================
BotMem_FightWeapon

Combat-weapon learning hook (2026-11-09, clan issue #2). Called at the end
of BotChooseWeapon (ai_dmq3.c) - fires on EVERY call (~10x/s per bot while
fighting). Records the bot's current combat decision: the weapon it is
holding (bs->weaponnum), the health band at that moment, the time and the
score baseline. The baseline must stay fixed while the same weapon is
still pending: this hook runs ~10x/s, but the per-frame resolution pass in
BotMem_Frame compares the score against the baseline - a per-call
overwrite would move the baseline past a freshly landed kill before the
pass looks, so the kill would never be credited (and the later death
would count as bad for the weapon). Hence: a call with the same weapon as
the pending entry returns without touching it; only a weapon change
replaces the entry (the old one is never resolved - "holding without
fighting" is not a combat signal). The 60 s expiry counts from the weapon
selection. Resolved every frame in BotMem_Frame (BotMem_FightCount). No-op when
botmem is off, the bot is out of the 32-slot table, or the weapon is
outside 1..9 (gauntlet..BFG; the grapple, 10, is not a fight weapon).
=================
*/
void BotMem_FightWeapon( bot_state_t *bs ) {
	int botnum, weapon;

	if ( botmem_enabled == 0 ) {
		return;
	}
	/* Warmup / intermission (clan issue #4): no credible combat outcome -
	   record no pending combat decision. The BotMem_FightGate that reads
	   the counters is intentionally NOT gated. */
	if ( BotMem_MatchLive() == 0 ) {
		return;
	}
	botnum = bs->client;
	if ( botnum < 0 || botnum >= 32 ) {
		return;
	}
	weapon = bs->weaponnum;
	if ( weapon < 1 || weapon >= 10 ) {
		return;
	}
	if ( botmem_fweapon[botnum] == weapon ) {
		return;		/* same pending decision, keep its score baseline */
	}
	botmem_fweapon[botnum] = weapon;
	botmem_fband[botnum] = BotMem_HealthBand( botnum );
	botmem_fscore[botnum] = level.clients[botnum].ps.persistant[PERS_SCORE];
	botmem_ftime[botnum] = BotMem_LevelTime();
	BotMem_LogTag( "fdec", botnum, weapon, botmem_fband[botnum], BotMem_LevelTime() );
}

/*
=================
BotMem_FightCount

Records a resolved combat outcome for (bot, weapon, band): isgood 1 = the
weapon held led to a kill (good), 0 = it led to a death / teamkill (bad).
Called from the combat pass in BotMem_Frame (every server frame).
=================
*/
static void BotMem_FightCount( int botnum, int weapon, int band, int isgood ) {
	int idx;

	if ( botnum < 0 || botnum >= 32 ) {
		return;
	}
	if ( weapon < 1 || weapon >= 10 ) {
		return;
	}
	if ( band < 0 || band > 2 ) {
		band = 0;
	}
	idx = (botnum*9 + (weapon-1))*3 + band;
	if ( isgood != 0 ) {
		botmem_fgood[idx] = botmem_fgood[idx] + 1;
	}
	else {
		botmem_fbad[idx] = botmem_fbad[idx] + 1;
	}
}

/*
=================
BotMem_FightGate

Returns 1 = REJECT this weapon for this bot right now, 0 = allow. Called
from OSP_BotChooseWeaponByRange (ai_dmq3.c) in the table loop and both
height-override branches. Rejection only: a weapon with a persistently bad
record (success rate < botmem_fight_threshold % over at least
botmem_fight_min samples, for the current health band or pooled across
bands when that band has no data yet) is not chosen. No-op when botmem is
off, bot_learn_use is off (read live, rcon toggle immediate), the
bot/weapon is out of range, or there is not enough evidence yet.
=================
*/
int BotMem_FightGate( int botnum, int weapon ) {
	int band, idx, good, bad, total, s100, b;

	if ( botmem_enabled == 0 ) return 0;
	if ( botnum < 0 || botnum >= 32 ) return 0;
	if ( weapon < 1 || weapon >= 10 ) return 0;
	if ( trap_Cvar_VariableIntegerValue( "bot_learn_use" ) == 0 ) return 0;
	band = BotMem_HealthBand( botnum );
	idx = (botnum*9 + (weapon-1))*3 + band;
	good = botmem_fgood[idx];
	bad = botmem_fbad[idx];
	if ( good + bad == 0 ) {
		/* pooled fallback across bands (this band has no data yet) */
		good = 0;
		bad = 0;
		for ( b = 0; b < 3; b++ ) {
			good = good + botmem_fgood[(botnum*9 + (weapon-1))*3 + b];
			bad = bad + botmem_fbad[(botnum*9 + (weapon-1))*3 + b];
		}
	}
	total = good + bad;
	if ( total < botmem_fmin ) return 0;
	s100 = good * 100 / total;
	if ( s100 < botmem_fthresh ) return 1;
	return 0;
}

/*
=================
BotMem_Frame

Every server frame (the band re-apply is rate-limited to 1x/s per bot):
resolves the pending decisions - death -> bad, score up (kill credit,
2026-11-03) -> good, score down while alive (teamkill) -> bad, 60 s
survival -> good - and re-applies the band-conditional scales.
=================
*/
void BotMem_Frame( void ) {
	int i, b, now, sc;

	if ( botmem_file == 0 ) {
		return;
	}
	now = BotMem_LevelTime();
	for ( i = 0; i < botmem_nactive; i++ ) {
		b = botmem_active[i];
		if ( botmem_seen[b] == 0 ) {
			continue;
		}
		if ( b >= MAX_CLIENTS ) {
			continue;
		}
		if ( BotMem_MatchLive() == 0 ) {
			/* Warmup / intermission (clan issue #4): no credible outcome
			   data - drop the pending decision without counting (no good,
			   no bad, no kill, no tk). The band re-apply runs explicitly
			   below: only the collection is gated, the application stays
			   active. Skip the outcome branches. */
			BotMem_DropPending( b );
			i = i - 1;
			if ( b < 32 && trap_Cvar_VariableIntegerValue( "bot_learn_use" ) != 0 && botmem_applied[b] != 0 && now - botmem_applytime[b] >= 1000 ) {
				botmem_applytime[b] = now;
				if ( botmem_gs[b] != 0 && trap_BotGoalWeightConfigPtr( botmem_gs[b] ) == botmem_scaled_cfg[b] ) {
					BotMem_ApplyScales( b, botmem_gs[b] );
				}
			}
			continue;
		}
		/* the bot died while the decision was pending (pm_type == PM_DEAD) */
		if ( level.clients[b].ps.pm_type == PM_DEAD ) {
			if ( botmem_goal[b] >= 0 && botmem_goal[b] < 256 ) {
				botmem_bad[botmem_goal[b]] = botmem_bad[botmem_goal[b]] + 1;
				BotMem_BotGoalCountB( b, botmem_goal[b], botmem_band[b], 0 );
			}
			BotMem_LogTag( "bad", b, botmem_goal[b], botmem_kind[b], now - botmem_time[b] );
			botmem_active[i] = botmem_active[botmem_nactive - 1];
			botmem_nactive = botmem_nactive - 1;
			botmem_seen[b] = 0;
			i = i - 1;
			continue;
		}
		/* Kill credit (2026-11-03): the score (PERS_SCORE) went UP since the
		   decision -> the bot fragged someone -> the pursued goal led to a
		   kill -> resolve as GOOD immediately (no 60 s wait). The victim is
		   handled by the death branch above (that branch fires first for the
		   victim's own entry). */
		sc = level.clients[b].ps.persistant[PERS_SCORE];
		if ( sc > botmem_score[b] ) {
			if ( botmem_goal[b] >= 0 && botmem_goal[b] < 256 ) {
				botmem_good[botmem_goal[b]] = botmem_good[botmem_goal[b]] + 1;
				BotMem_BotGoalCountB( b, botmem_goal[b], botmem_band[b], 1 );
			}
			BotMem_LogTag( "kill", b, botmem_goal[b], botmem_kind[b], now - botmem_time[b] );
			botmem_active[i] = botmem_active[botmem_nactive - 1];
			botmem_nactive = botmem_nactive - 1;
			botmem_seen[b] = 0;
			i = i - 1;
			continue;
		}
		/* Teamkill (TDM: score -1 while alive) -> the goal was bad -> BAD. */
		if ( sc < botmem_score[b] ) {
			if ( botmem_goal[b] >= 0 && botmem_goal[b] < 256 ) {
				botmem_bad[botmem_goal[b]] = botmem_bad[botmem_goal[b]] + 1;
				BotMem_BotGoalCountB( b, botmem_goal[b], botmem_band[b], 0 );
			}
			BotMem_LogTag( "tk", b, botmem_goal[b], botmem_kind[b], now - botmem_time[b] );
			botmem_active[i] = botmem_active[botmem_nactive - 1];
			botmem_nactive = botmem_nactive - 1;
			botmem_seen[b] = 0;
			i = i - 1;
			continue;
		}
		if ( now - botmem_time[b] >= 60000 ) {
			if ( botmem_goal[b] >= 0 && botmem_goal[b] < 256 ) {
				botmem_good[botmem_goal[b]] = botmem_good[botmem_goal[b]] + 1;
				BotMem_BotGoalCountB( b, botmem_goal[b], botmem_band[b], 1 );
			}
			BotMem_LogTag( "good", b, botmem_goal[b], botmem_kind[b], now - botmem_time[b] );
			botmem_active[i] = botmem_active[botmem_nactive - 1];
			botmem_nactive = botmem_nactive - 1;
			botmem_seen[b] = 0;
			i = i - 1;
			continue;
		}
		/* Band re-apply (2026-11-03): the weight config is static per bot, so
		   the health-band-conditional scale must be pushed from here: at most
		   once per second per bot, move the config from the last-applied scale
		   to the desired one for the bot's CURRENT band (ratio). The cfgptr
		   check guards against the engine reusing the goalstate handle for a
		   different bot. bot_learn_use is read live (2026-11-04): a rcon
		   toggle takes effect immediately for already-applied bots on their
		   next re-apply. */
		if ( b < 32 && trap_Cvar_VariableIntegerValue( "bot_learn_use" ) != 0 && botmem_applied[b] != 0 && now - botmem_applytime[b] >= 1000 ) {
			botmem_applytime[b] = now;
			if ( botmem_gs[b] != 0 && trap_BotGoalWeightConfigPtr( botmem_gs[b] ) == botmem_scaled_cfg[b] ) {
				BotMem_ApplyScales( b, botmem_gs[b] );
			}
		}
	}
	/* Combat-weapon learning (2026-11-09, clan issue #2): resolve the pending
	   combat decisions every frame, over the 32 bot slots - independent of the
	   goal-decision active list, so a stray kill without a pending goal
	   decision is still credited. score up (kill credit) -> good, score down
	   while alive (teamkill) -> bad, death -> bad; 60 s without any combat
	   event -> expire, no count (holding without fighting is not a combat
	   signal). Runs only when collection is on (the botmem_file guard above).
	   The gate (BotMem_FightGate) is separate: it reads these counters
	   directly and works in log-only mode. */
	for ( b = 0; b < 32; b++ ) {
		if ( botmem_fweapon[b] == 0 ) {
			continue;
		}
		if ( BotMem_MatchLive() == 0 ) {
			/* Warmup / intermission (clan issue #4): no credible combat
			   outcome - drop the pending combat decision without counting
			   (no fgood, no fbad). */
			botmem_fweapon[b] = 0;
			continue;
		}
		if ( level.clients[b].ps.pm_type == PM_DEAD ) {
			BotMem_FightCount( b, botmem_fweapon[b], botmem_fband[b], 0 );
			BotMem_LogTag( "fbad", b, botmem_fweapon[b], botmem_fband[b], now - botmem_ftime[b] );
			botmem_fweapon[b] = 0;
			continue;
		}
		sc = level.clients[b].ps.persistant[PERS_SCORE];
		if ( sc > botmem_fscore[b] ) {
			BotMem_FightCount( b, botmem_fweapon[b], botmem_fband[b], 1 );
			BotMem_LogTag( "fgood", b, botmem_fweapon[b], botmem_fband[b], now - botmem_ftime[b] );
			botmem_fweapon[b] = 0;
			continue;
		}
		if ( sc < botmem_fscore[b] ) {
			BotMem_FightCount( b, botmem_fweapon[b], botmem_fband[b], 0 );
			BotMem_LogTag( "fbad", b, botmem_fweapon[b], botmem_fband[b], now - botmem_ftime[b] );
			botmem_fweapon[b] = 0;
			continue;
		}
		if ( now - botmem_ftime[b] >= 60000 ) {
			botmem_fweapon[b] = 0;
			continue;
		}
	}
}

/*
=================
BotMem_MapEnd

Closes the per-map .mem file (open + stat + end lines) and persists the
accumulated per-bot learning to bot_memory/learn_<map>.txt.
=================
*/
void BotMem_MapEnd( void ) {
	int i, g, b, w, base, fg0, fb0, fg1, fb1, fg2, fb2, fG, fB;

	if ( botmem_file == 0 ) {
		return;
	}
	for ( i = 0; i < botmem_nactive; i++ ) {
		b = botmem_active[i];
		if ( botmem_seen[b] != 0 ) {
			BotMem_LogTag( "open", b, botmem_goal[b], botmem_kind[b], BotMem_LevelTime() - botmem_time[b] );
		}
	}
	botmem_nactive = 0;
	for ( g = 0; g < 256; g++ ) {
		if ( botmem_good[g] != 0 || botmem_bad[g] != 0 ) {
			BotMem_LogTag( "stat", g, botmem_good[g], botmem_bad[g], 0 );
		}
	}
	/* Combat-weapon learning summary (2026-11-09, clan issue #2): one console
	   line per (bot, weapon) with counts - pooled across the health bands
	   (scale = integer success %) plus the per-band b0/b1/b2 breakdown.
	   Console only (works in log-only mode); at most 9 lines per bot. */
	for ( b = 0; b < 32; b++ ) {
		for ( w = 1; w <= 9; w++ ) {
			base = (b*9 + (w-1))*3;
			fg0 = botmem_fgood[base];
			fb0 = botmem_fbad[base];
			fg1 = botmem_fgood[base+1];
			fb1 = botmem_fbad[base+1];
			fg2 = botmem_fgood[base+2];
			fb2 = botmem_fbad[base+2];
			fG = fg0 + fg1 + fg2;
			fB = fb0 + fb1 + fb2;
			if ( fG + fB == 0 ) {
				continue;
			}
			G_Printf( "botmem flearn bot=%i w=%i good=%i/%i scale=%i%% b0=%i/%i b1=%i/%i b2=%i/%i\n",
				b, w, fG, fB, fG*100/(fG+fB), fg0, fb0, fg1, fb1, fg2, fb2 );
		}
	}
	BotMem_LogTag( "end", BotMem_LevelTime(), 0, 0, 0 );
	trap_FS_FCloseFile( botmem_file );
	botmem_file = 0;
	/* persist the accumulated per-bot learning */
	BotMem_SaveLearn();
}

/* ================= OSP 1.04b botmem learning loop ================= */

static int BotMem_StrCpy( char *dst, const char *src, int size ) {
	int i;

	for ( i = 0; i < size - 1 && src[i] != 0; i++ ) dst[i] = src[i];
	if ( size > 0 ) dst[size - 1] = 0;
	return 0;
}

static int BotMem_StrCmp( char *a, char *b ) {
	int i;

	for ( i = 0; a[i] != 0 || b[i] != 0; i++ ) {
		if ( a[i] < b[i] ) return -1;
		if ( a[i] > b[i] ) return 1;
	}
	return 0;
}

/* "bot_memory/learn_<map>.txt"; the map name (botmem_map, filled by
   BotMem_MapStart from the engine's mapname cvar) is sanitized to
   [a-zA-Z0-9], max 31 chars, so odd map names cannot break the path. */
static void BotMem_LearnPath( void ) {
	int i, n, c;
	char *p;

	p = botmem_lmap;
	memcpy( p, "bot_memory/learn_", 17 );
	n = 17;
	for ( i = 0; i < 31 && botmem_map[i] != 0; i++ ) {
		c = botmem_map[i];
		if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ) p[n] = c, n = n + 1;
	}
	p[n] = '.'; n = n + 1;
	p[n] = 't'; n = n + 1;
	p[n] = 'x'; n = n + 1;
	p[n] = 't'; n = n + 1;
	p[n] = 0;
}

static void BotMem_LoadLearn( void ) {
	int handle = 0;
	int len;
	int v2;
	int v3;
	char hdr[8];

	if ( botmem_enabled == 0 ) return;
	if ( botmem_loaded != 0 ) return;
	/* 2026-11-04: nothing to load when BOTH collection and use are off
	   (skip the pointless ~90 KB read). The loaded flag stays 0, so a cvar
	   toggle before the next G_Init still gets loaded then. */
	if ( trap_Cvar_VariableIntegerValue( "bot_learn_enable" ) == 0 &&
	     trap_Cvar_VariableIntegerValue( "bot_learn_use" ) == 0 ) {
		return;
	}
	botmem_loaded = 1;
	/* mode FS_READ: never create/truncate the file on load.
	   (FS_WRITE truncated learn.txt on every G_Init in the early build.)
	   v2 (2026-11-03): "learn2" (6 B) + 2 x 8192 x 4 pooled +
	   2 x 3072 x 4 per-(bot,classname,band) = 90118 B.
	   v1 ("learn" 5 B header, 65541 B, pooled only) is still accepted
	   (band section stays zero) - the seed tools write v1. */
	BotMem_LearnPath();
	len = trap_FS_FOpenFile( botmem_lmap, &handle, FS_READ );
	if ( handle <= 0 || len < 65541 ) {
		if ( handle > 0 ) trap_FS_FCloseFile( handle );
		return;
	}
	/* len (from FOpenFile, FS_READ) guarantees the file is at least v1-sized, so
	   the reads below are in bounds; trap_FS_Read is void in the game header
	   (the decompiled build checked its return - redundant given the len check) */
	trap_FS_Read( hdr, 5, handle );
	if ( !(hdr[0] == 'l' && hdr[1] == 'e' && hdr[2] == 'a' && hdr[3] == 'r' && hdr[4] == 'n') ) {
		trap_FS_FCloseFile( handle );
		return;
	}
	v2 = 0;
	v3 = 0;
	if ( len >= 97030 ) {
		/* v3 (2026-11-09, clan issue #2): "learn3" (6 B) + pooled + band +
		   2 x 864 x 4 combat counters = 97030 B. A v3 marker with a shorter
		   file is corrupt (handled below); a v2 marker in a v3-sized file is
		   treated as v2 (the trailing bytes are simply not read). */
		trap_FS_Read( hdr, 1, handle );
		if ( hdr[0] == '3' ) {
			v3 = 1;
		}
		else if ( hdr[0] == '2' ) {
			v2 = 1;
		}
		else {
			trap_FS_FCloseFile( handle );
			return;
		}
	}
	else if ( len >= 90118 ) {
		trap_FS_Read( hdr, 1, handle );
		if ( hdr[0] != '2' ) {
			trap_FS_FCloseFile( handle );
			return;
		}
		v2 = 1;
	}
	trap_FS_Read( botmem_bgood, 32768, handle );
	trap_FS_Read( botmem_bbad, 32768, handle );
	if ( v2 != 0 || v3 != 0 ) {
		trap_FS_Read( botmem_bg, 12288, handle );
		trap_FS_Read( botmem_bb, 12288, handle );
	}
	if ( v3 != 0 ) {
		trap_FS_Read( botmem_fgood, 3456, handle );
		trap_FS_Read( botmem_fbad, 3456, handle );
	}
	trap_FS_FCloseFile( handle );
}

static void BotMem_SaveLearn( void ) {
	int handle = 0;

	if ( botmem_enabled == 0 ) return;
	/* mode FS_WRITE: rewrite the whole file on every save
	   (FS_APPEND accumulated a block per save in the early build).
	   v3 (2026-11-09, clan issue #2): "learn3" header + pooled + band +
	   2 x 864 x 4 combat counters = 97030 B (v1/v2 files still load). */
	BotMem_LearnPath();
	/* the file handle comes via the pointer; the return value is the file
	   length for FS_READ and 0 for FS_WRITE (decompiled build: &handle) */
	trap_FS_FOpenFile( botmem_lmap, &handle, FS_WRITE );
	if ( handle <= 0 ) return;
	trap_FS_Write( "learn3", 6, handle );
	trap_FS_Write( botmem_bgood, 32768, handle );
	trap_FS_Write( botmem_bbad, 32768, handle );
	trap_FS_Write( botmem_bg, 12288, handle );
	trap_FS_Write( botmem_bb, 12288, handle );
	trap_FS_Write( botmem_fgood, 3456, handle );
	trap_FS_Write( botmem_fbad, 3456, handle );
	trap_FS_FCloseFile( handle );
}

static void BotMem_BotGoalCount( int botnum, int goal, int isgood ) {
	/* indexed directly by botnum (stable across VM restarts; see the note
	   above the BSS declarations); botnum is bounded to 32 */
	if ( botnum < 0 || botnum >= 32 ) return;
	/* goals are 0-based (levelitem numbers start at 0) */
	if ( goal < 0 || goal >= 256 ) return;
	if ( isgood != 0 ) {
		botmem_bgood[botnum*256+goal] = botmem_bgood[botnum*256+goal] + 1;
	} else {
		botmem_bbad[botnum*256+goal] = botmem_bbad[botnum*256+goal] + 1;
	}
}

/* 2026-11-03 finer buckets: in addition to the pooled per-goal counters
   (kept for seed-tool compat + fallback), outcomes are bucketed per
   (bot, classname, health band). The classname key is the index in the
   map-global table botmem_cnames2 (built from the goalmap in
   BotMem_BotLearn, first-occurrence order, stable per map/AAS);
   botmem_goalcn[goal] = that index or -1 (nameless goal). */
static void BotMem_BotGoalCountB( int botnum, int goal, int band, int isgood ) {
	int cn, idx;

	if ( botnum < 0 || botnum >= 32 ) return;
	if ( goal < 0 || goal >= 256 ) return;
	if ( band < 0 ) band = 0;
	if ( band > 2 ) band = 2;
	if ( isgood != 0 ) {
		botmem_bgood[botnum*256+goal] = botmem_bgood[botnum*256+goal] + 1;
	} else {
		botmem_bbad[botnum*256+goal] = botmem_bbad[botnum*256+goal] + 1;
	}
	cn = botmem_goalcn[goal];
	if ( cn < 0 || cn >= botmem_cnames2n ) return;
	idx = (botnum*32 + cn)*3 + band;
	if ( isgood != 0 ) {
		botmem_bg[idx] = botmem_bg[idx] + 1;
	} else {
		botmem_bb[idx] = botmem_bb[idx] + 1;
	}
}

/* Health band at decision time: 0: <=50, 1: 51-100, 2: >100.
   Health = client->ps.stats[STAT_HEALTH] (synced by ClientThink every
   frame). */
static int BotMem_HealthBand( int botnum ) {
	int h;

	if ( botnum < 0 || botnum >= MAX_CLIENTS ) return 2;
	h = level.clients[botnum].ps.stats[STAT_HEALTH];
	if ( h <= 50 ) return 0;
	if ( h <= 100 ) return 1;
	return 2;
}

/* Desired scale for (bot, classname, band): band counters if present,
   else the pooled per-goal fallback (seed-tool compat: seeds write
   pooled counters), else 1.0 (no data). Same [0,1] clamp and
   bot_memo_boost gate as the old spawn-time pass. The floor is 0.002
   (not 0): the engine weight API is multiplicative, so a scale of
   exactly 0 would zero the weight IRREVERSIBLY (0 x anything = 0);
   0.002x is effectively unselectable (base weights ~300-500 vs
   competitors ~100+) but still restorable across band changes. */
static void BotMem_DesiredScale( int bi, int cn2, int band, float *out ) {
	int g, good, bad, total, idx;

	if ( bi < 0 || bi >= 32 ) { *out = 1.0f; return; }
	if ( cn2 < 0 || cn2 >= botmem_cnames2n ) { *out = 1.0f; return; }
	if ( band < 0 ) band = 0;
	if ( band > 2 ) band = 2;
	idx = (bi*32 + cn2)*3 + band;
	good = botmem_bg[idx];
	bad = botmem_bb[idx];
	if ( good == 0 && bad == 0 ) {
		/* pooled fallback across all goals with this classname */
		good = 0;
		bad = 0;
		for ( g = 0; g < 256; g++ ) {
			if ( botmem_goalcn[g] != cn2 ) continue;
			good = good + botmem_bgood[bi*256+g];
			bad = bad + botmem_bbad[bi*256+g];
		}
	}
	total = good + bad;
	if ( total == 0 ) {
		*out = 1.0f;
		return;
	}
	*out = (float)good / (float)total;
	if ( *out < 0.0f ) *out = 0.0f;
	if ( *out > 1.0f ) *out = 1.0f;
	if ( botmem_boost > 0 && *out > 0.5f ) {
		*out = *out * (float)botmem_boost;
		if ( *out > 16.0f ) *out = 16.0f;
	}
	if ( *out < 0.002f ) *out = 0.002f;
}

/* Push the (possibly band-changed) scales to the bot's engine weight
   config. Called from BotMem_BotLearn (spawn time: botmem_last was just
   zeroed for this bot, so every application is absolute) and from
   BotMem_Frame (at most 1x/s: RATIO on the last-applied scale - the
   engine weight config is static per bot, so the QVM must re-push when
   the desired scale changes, e.g. the bot's health band changed).
   Paramount classnames are skipped (their fixed boost is set by the
   paramount pass in BotMem_BotLearn). No printing here: this runs
   every second from the frame loop. */
static void BotMem_ApplyScales( int bi, int goalstate ) {
	int cn, last, lfp;
	float wantf, lastf, ratio;

	if ( bi < 0 || bi >= 32 ) return;
	if ( goalstate == 0 ) return;
	if ( botmem_cnames2n == 0 ) return;
	for ( cn = 0; cn < botmem_cnames2n; cn++ ) {
		if ( BotMem_IsParamount( botmem_cnames2 + cn*32 ) != 0 ) continue;
		wantf = 1.0f;
		BotMem_DesiredScale( bi, cn, BotMem_HealthBand( bi ), &wantf );
		last = botmem_last[bi*32 + cn];
		if ( last == 0 ) {
			/* first application to this weight config: ABSOLUTE scale. Skip a
			   neutral 1.0 (leaves the base weight and avoids the COW copy). */
			if ( wantf == 1.0f ) {
				botmem_last[bi*32 + cn] = BotMem_FbitsI( wantf ) + 1;
				continue;
			}
			/* the engine reads the 3rd QVM arg as raw 32 bits reinterpreted as a
			   float (VMF(3) in sv_game.c), so the FLOAT itself is passed (no C
			   conversion: same type) - passing the int bit pattern into a
			   float-typed parameter would trigger an int->float conversion */
			trap_BotScaleGoalWeight( goalstate, botmem_cnames2 + cn*32, wantf );
		} else {
			/* ratio on the last-applied scale. The engine clamps the scale to
			   [0,16], so clamp the ratio to [1/16,16] and track the EFFECTIVE
			   lastf (large jumps converge at 16x/s over a few seconds). */
			lastf = BotMem_FbitsF( last - 1 );
			if ( lastf == 0.0f || (wantf >= lastf*0.985f && wantf <= lastf*1.015f) ) {
				continue;	/* unchanged (within 1.5%): keep last as-is */
			}
			ratio = wantf / lastf;
			if ( ratio > 16.0f ) ratio = 16.0f;
			if ( ratio < 0.0625f ) ratio = 0.0625f;
			trap_BotScaleGoalWeight( goalstate, botmem_cnames2 + cn*32, ratio );
			lastf = lastf * ratio;
			botmem_last[bi*32 + cn] = BotMem_FbitsI( lastf ) + 1;
			continue;
		}
		botmem_last[bi*32 + cn] = BotMem_FbitsI( wantf ) + 1;
	}
}

/* Is the classname in the current map's paramount tactic list? */
static int BotMem_IsParamount( char *name ) {
	int i;

	if ( name[0] == 0 ) return 0;
	for ( i = 0; i < botmem_parmn; i++ ) {
		if ( BotMem_StrCmp( botmem_parm + i*32, name ) == 0 ) return 1;
	}
	return 0;
}

/* Read the per-map paramount tactic list: cvar botmem_paramount_<map>
   (space-separated goal classnames, max 8, registered with empty
   default). These goals get a FIXED weight boost (botmem_parmboost)
   instead of the learned good/bad scale, so an overarching map tactic
   (e.g. "always contest the quad") is followed even when the bot's own
   history marks the goal as unsuccessful. Map name sanitized like the
   learn file. */
static void BotMem_ParamountLoad( void ) {
	int i, n, c, j;
	char *p;

	if ( botmem_enabled == 0 ) return;
	p = botmem_parmname;
	memcpy( p, "botmem_paramount_", 17 );
	n = 17;
	for ( i = 0; i < 31 && botmem_map[i] != 0; i++ ) {
		c = botmem_map[i];
		if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ) p[n] = c, n = n + 1;
	}
	p[n] = 0;
	trap_Cvar_Register( 0, botmem_parmname, "", 0 );
	trap_Cvar_VariableStringBuffer( botmem_parmname, botmem_parmval, 255 );
	botmem_parmval[255] = 0;
	memset( botmem_parm, 0, sizeof( botmem_parm ) );
	botmem_parmn = 0;
	i = 0;	/* the map-name loop above left i at the map length - restart
			the scan of the cvar value at byte 0 (bug found in test pm6: the
			first token was read from byte <maplen> onward, "item_quad"
			parsed as "quad") */
	j = 0;
	while ( i < 256 ) {
		c = botmem_parmval[i];
		if ( c == 0 ) break;
		if ( c == ' ' || c == '\t' || c == '\n' ) { i = i + 1; continue; }
		if ( j >= 8 ) break;
		n = 0;
		while ( i < 256 ) {
			c = botmem_parmval[i];
			if ( c == 0 || c == ' ' || c == '\t' || c == '\n' ) break;
			if ( n < 31 ) botmem_parm[j*32+n] = c, n = n + 1;
			i = i + 1;
		}
		botmem_parm[j*32+n] = 0;
		j = j + 1;
	}
	botmem_parmn = j;
}

/*
=================
BotMem_BotLearn

Called at the goal-choose sites after BotMem_LogDecision. Once per bot
per weight config (the cfgptr latch), it prints the goalmap (goal
number -> classname, collection side, gated on bot_learn_enable),
builds the map-global classname table + goal mapping, prints the per-
bot learn lines and applies the scales to the engine weight config
(application side, gated on bot_learn_use): the learned/paramount
scales (BotMem_ApplyScales + the paramount pass).
=================
*/
void BotMem_BotLearn( bot_state_t *bs ) {
	int goalstate, botnum, bi, g, j, k, cfgptr, en, us;
	char name[32];
	/* 192: the per-item debug line is up to ~147 chars (with the b0/b1/b2
	   band counts, 2026-11-03); 128 would overflow this function's stack
	   (64 already corrupted it once - see the 2026-10-31 diag run) */
	char msg[192];
	char *m;
	int good, bad;
	float scale;

	/* feature off: no goalmap print, no scale application (also prevents
	   stale-file scaling when the cvar is 0) */
	if ( botmem_enabled == 0 ) return;
	/* 2026-11-05: engine without the DWC botmem syscalls (stock Quake3e) -
	   log-only mode. There is no engine side to talk to (goalmap classnames,
	   learned/paramount scales, cfg latch), so skip it all; the .mem decision
	   log (BotMem_LogDecision) and the pooled outcome counting keep working,
	   and the learn file then carries pooled counts only. */
	if ( botmem_engine_ok == 0 ) return;
	goalstate = bs->gs;
	if ( goalstate == 0 ) return;
	botnum = bs->client;
	/* counters are bounded to 32 slots (see bgood/bbad decl); botnum is the
	   stable clientnum, so this is stable across the VM restart that happens
	   on every match restart (see the BSS note). Bots with
	   clientnum >= 32 simply do not learn. */
	if ( botnum < 0 || botnum >= 32 ) return;
	/* Re-apply the scale when the goalstate's item WEIGHT CONFIG changes (a
	   new bot took the slot, e.g. the auto-casper was kicked and dwcbot1 was
	   added to client 0). The engine REUSES goalstate handles (a freed index
	   is handed to the next bot), so the handle is NOT a reliable identity
	   for "same bot" - the weight-config generation counter
	   (BotGoalWeightConfigPtr) changes on every re-add and IS reliable. The
	   old once-per-botnum latch skipped the new bot's goalstate because the
	   previous occupant had already latched botmem_applied[botnum].
	   (2026-11-02) */
	cfgptr = trap_BotGoalWeightConfigPtr( goalstate );
	if ( cfgptr == 0 ) return;
	/* Skip if this exact weight config was already scaled for this bot. The
	   key is the config generation counter, not the goalstate handle (the
	   engine REUSES handles across bots, so a handle is not a reliable
	   "same bot" identity). NOTE: the latch is written at the END of this
	   function (after the scale pass), because the first
	   trap_BotScaleGoalWeight COWs the shared weight config and CHANGES the
	   counter (be_ai_goal.c BotScaleGoalWeight) - see the latch below.
	   Latching the pre-scale counter here would mismatch on the next
	   decision and re-scale every time (weights collapsing to 0). */
	if ( botmem_applied[botnum] != 0 && botmem_scaled_cfg[botnum] == cfgptr ) return;
	bi = botnum;
	/* 2026-11-04: per-feature toggles - bot_learn_enable gates the
	   collection side (goalmap print, learn lines), bot_learn_use gates the
	   application side (learned scales, boost, paramount). Read once per
	   call; a mid-session rcon toggle takes effect with the next bot (new
	   cfgptr) and, for the re-apply, live in BotMem_Frame. */
	en = trap_Cvar_VariableIntegerValue( "bot_learn_enable" );
	us = trap_Cvar_VariableIntegerValue( "bot_learn_use" );
	if ( botmem_goalmap_done == 0 ) {
		botmem_goalmap_done = 1;
		for ( k = 0; k < 256; k++ ) {
			trap_BotGoalClassName( k, name, 31 );
			name[31] = 0;
			if ( name[0] == 0 ) continue;
			/* 2026-11-03 (finer buckets): build the map-global classname
			   table (first-occurrence order, max 32) + the goal ->
			   classname index mapping. Stable per map/AAS (rebuilt per map
			   by BotMem_MapStart). */
			g = -1;
			for ( j = 0; j < botmem_cnames2n; j++ ) {
				if ( BotMem_StrCmp( botmem_cnames2 + j*32, name ) == 0 ) { g = j; break; }
			}
			if ( g < 0 ) {
				if ( botmem_cnames2n >= 32 ) continue;
				g = botmem_cnames2n;
				botmem_cnames2n = botmem_cnames2n + 1;
				BotMem_StrCpy( botmem_cnames2 + g*32, name, 32 );
			}
			botmem_goalcn[k] = g;
			/* 2026-11-04: the console print is collection output -> gated
			   on bot_learn_enable; the table build above stays unconditional
			   (the band counters and the scale lookup need it). */
			if ( en != 0 ) {
				m = msg;
				memcpy( m, "botmem goal ", 12 );
				m += 12;
				m += BotMem_Int( m, k );
				*m++ = '=';
				for ( j = 0; name[j] != 0; j++ ) *m++ = name[j];
				*m++ = '\n';
				*m = 0;
				G_Printf( "%s", msg );
			}
		}
	}
	/* 2026-11-03 (finer buckets): the per-bot classname table (botmem_cnames)
	   is replaced by the map-global botmem_cnames2 + botmem_goalcn built in
	   the goalmap pass above. The scale is band-aware (per (bot,classname,
	   health band) with pooled fallback) and applied via BotMem_ApplyScales;
	   BotMem_Frame re-pushes it at most 1x/s as the bot's band changes.
	   Zero the last-applied scales for this bot: we only get here on a new
	   weight config (cfgptr latch mismatch above), which starts at the base
	   weights, so the first application must be ABSOLUTE, not a ratio. */
	memset( botmem_last + bi*32, 0, 32 * sizeof( int ) );
	/* 2026-11-04: the learn lines are collection output -> gated on
	   bot_learn_enable */
	if ( en != 0 ) for ( j = 0; j < botmem_cnames2n; j++ ) {
		if ( BotMem_IsParamount( botmem_cnames2 + j*32 ) != 0 ) continue;
		good = 0;
		bad = 0;
		for ( g = 0; g < 256; g++ ) {
			if ( botmem_goalcn[g] != j ) continue;
			good = good + botmem_bgood[bi*256+g];
			bad = bad + botmem_bbad[bi*256+g];
		}
		if ( good + bad == 0 ) continue;	/* fresh run: no learn lines, no noise */
		scale = 1.0f;
		BotMem_DesiredScale( bi, j, BotMem_HealthBand( bi ), &scale );
		/* learn line: old format (pooled counts + scale) + b0/b1/b2 band
		   counts appended after the scale (analysis regexes stay valid) */
		m = msg;
		memcpy( m, "botmem learn bot=", 17 );
		m += 17;
		m += BotMem_Int( m, botnum );
		*m++ = ' '; *m++ = 'b'; *m++ = 'i'; *m++ = '=';
		m += BotMem_Int( m, bi );
		*m++ = ' '; *m++ = 'g'; *m++ = 's'; *m++ = '=';
		m += BotMem_Int( m, goalstate );
		*m++ = ' '; *m++ = 'i'; *m++ = 't'; *m++ = 'e'; *m++ = 'm'; *m++ = '=';
		for ( k = 0; botmem_cnames2[j*32+k] != 0 && (m - msg) < 150; k++ ) *m++ = botmem_cnames2[j*32+k];
		*m++ = ' '; *m++ = 'g'; *m++ = 'o'; *m++ = 'o'; *m++ = 'd'; *m++ = '=';
		m += BotMem_Int( m, good );
		*m++ = '/';
		m += BotMem_Int( m, bad );
		*m++ = ' '; *m++ = 's'; *m++ = 'c'; *m++ = 'a'; *m++ = 'l'; *m++ = 'e'; *m++ = '=';
		m += BotMem_Float2( m, scale );
		*m++ = ' '; *m++ = 'b'; *m++ = '0'; *m++ = '=';
		m += BotMem_Int( m, botmem_bg[(bi*32+j)*3+0] );
		*m++ = '/';
		m += BotMem_Int( m, botmem_bb[(bi*32+j)*3+0] );
		*m++ = ' '; *m++ = 'b'; *m++ = '1'; *m++ = '=';
		m += BotMem_Int( m, botmem_bg[(bi*32+j)*3+1] );
		*m++ = '/';
		m += BotMem_Int( m, botmem_bb[(bi*32+j)*3+1] );
		*m++ = ' '; *m++ = 'b'; *m++ = '2'; *m++ = '=';
		m += BotMem_Int( m, botmem_bg[(bi*32+j)*3+2] );
		*m++ = '/';
		m += BotMem_Int( m, botmem_bb[(bi*32+j)*3+2] );
		*m++ = '\n';
		*m = 0;
		G_Printf( "%s", msg );
	}
	/* apply the band-aware scales to the engine weight config (absolute:
	   botmem_last was just zeroed for this bot); paramount classnames are
	   handled by the dedicated pass below. 2026-11-04: the whole application
	   (learned scales + paramount boost) is gated on bot_learn_use. The
	   cfgptr latch below stays UNCONDITIONAL - with use off no scale call
	   runs, no COW happens, the counter is unchanged, and latching it
	   prevents a re-run of the (empty) pass on every decision. */
	if ( us != 0 ) BotMem_ApplyScales( bi, goalstate );
	/* paramount map-tactic goals: fixed boost (botmem_parmboost) on the
	   unscaled weight, regardless of this bot's learned good/bad counts.
	   Runs after the learned pass and on the untouched weight, so it
	   overrides suppression; classnames absent from the weight config are
	   no-ops in the engine. 2026-11-04: gated on bot_learn_use. */
	if ( us != 0 ) for ( j = 0; j < botmem_parmn; j++ ) {
		m = msg;
		memcpy( m, "botmem learn bot=", 17 );
		m += 17;
		m += BotMem_Int( m, botnum );
		*m++ = ' '; *m++ = 'b'; *m++ = 'i'; *m++ = '=';
		m += BotMem_Int( m, bi );
		*m++ = ' '; *m++ = 'g'; *m++ = 's'; *m++ = '=';
		m += BotMem_Int( m, goalstate );
		*m++ = ' '; *m++ = 'i'; *m++ = 't'; *m++ = 'e'; *m++ = 'm'; *m++ = '=';
		for ( k = 0; botmem_parm[j*32+k] != 0 && (m - msg) < 100; k++ ) *m++ = botmem_parm[j*32+k];
		*m++ = ' '; *m++ = 'P';
		*m++ = ' '; *m++ = 's'; *m++ = 'c'; *m++ = 'a'; *m++ = 'l'; *m++ = 'e'; *m++ = '=';
		m += BotMem_Float2( m, (float)botmem_parmboost );
		*m++ = '\n';
		*m = 0;
		G_Printf( "%s", msg );
		trap_BotScaleGoalWeight( goalstate, botmem_parm + j*32, (float)botmem_parmboost );
		/* 2026-11-03: track the applied scale for this classname so the
		   Frame re-apply (which skips paramount goals) knows the last value */
		for ( k = 0; k < botmem_cnames2n; k++ ) {
			if ( BotMem_StrCmp( botmem_cnames2 + k*32, botmem_parm + j*32 ) == 0 ) {
				botmem_last[bi*32 + k] = BotMem_FbitsI( (float)botmem_parmboost ) + 1;
				break;
			}
		}
	}
	/* Latch AFTER the scale pass: the first trap_BotScaleGoalWeight COWs the
	   shared weight config (weightconfig_owned 0->1), replacing
	   gs->itemweightconfig with a per-bot copy -> the generation counter
	   CHANGES. Latch the FINAL (post-COW) counter so the next decision for
	   this bot early-returns; a new bot taking the slot gets a fresh
	   goalstate + fresh counter, which mismatches the latch and
	   re-applies. If no scale call ran (nothing to scale) no COW happened
	   and the counter is unchanged. */
	botmem_applied[botnum] = 1;
	botmem_scaled_cfg[botnum] = trap_BotGoalWeightConfigPtr( goalstate );
}

/*
=================
BotMem_InitGame

G_InitGame hook (first statement, like the decompiled build):
registers the cvars, runs the engine capability probe, then
MapStart + LoadLearn + ParamountLoad.
=================
*/
void BotMem_InitGame( void ) {
	char probeval[32];

	/* bot_memory: master switch. Default is 0 (OFF): live servers must not
	   learn unless they explicitly +set bot_memory 1 (user decision
	   2026-11-02). */
	trap_Cvar_Register( 0, "bot_memory", "0", 0 );
	botmem_enabled = trap_Cvar_VariableIntegerValue( "bot_memory" );
	/* 2026-11-05: engine capability probe. The DWC botmem syscalls are
	   appended at the END of the engine's gameImport enum (after the last
	   stock entry, so existing QVMs are not shifted). The stock
	   trap_GetValue (the "_DWC" names in the engine's SV_GetValue) answers
	   them with the enum values. All four must be present; otherwise
	   botmem_engine_ok stays 0 and the QVM runs in log-only mode (the .mem
	   decision log + the pooled outcome counting keep working, the engine
	   side is skipped in BotMem_BotLearn). Re-probed on every VM restart
	   (BSS is zeroed per match). */
	botmem_engine_ok = 0;
	if ( trap_GetValue( probeval, 32, "trap_BotLoadGoalFuzzyLogic_DWC" ) != 0
	 && trap_GetValue( probeval, 32, "trap_BotScaleGoalWeight_DWC" ) != 0
	 && trap_GetValue( probeval, 32, "trap_BotGoalClassName_DWC" ) != 0
	 && trap_GetValue( probeval, 32, "trap_BotGoalWeightConfigPtr_DWC" ) != 0 ) {
		botmem_engine_ok = 1;
	}
	if ( botmem_enabled != 0 && botmem_engine_ok == 0 ) {
		G_Printf( "botmem: engine has no DWC botmem syscalls - log-only mode\n" );
	}
	/* bot_learn_enable (2026-11-04): learning/collection on/off - the per-map
	   .mem decision log, the good/bad/kill/tk outcome counting and the learn
	   file persistence. Evaluated at map start (the .mem file is opened only
	   when it is on), so the per-map lifecycle is: file open = collect = save.
	   Default 1 (on); subordinate to bot_memory. */
	trap_Cvar_Register( 0, "bot_learn_enable", "1", 0 );
	/* bot_learn_use (2026-11-04): use of the learned data on/off - the
	   learned-scale pass, bot_memo_boost, the paramount boost and the 1x/s
	   band re-apply. Evaluated LIVE (no BSS state): a rcon toggle takes
	   effect immediately (new spawns; existing bots on the next respawn /
	   re-apply). Default 1 (on); subordinate to bot_memory. */
	trap_Cvar_Register( 0, "bot_learn_use", "1", 0 );
	/* bot_memo_boost: when > 0, goals the bot learns to like (scale > 0.5)
	   get their scale multiplied by this factor (capped at 16), so a proven
	   goal can dominate the proximity-dominated goal choice (see the
	   2026-10-31 A/B: suppression alone cannot flip the pick, boosting can) */
	trap_Cvar_Register( 0, "bot_memo_boost", "0", 0 );
	botmem_boost = trap_Cvar_VariableIntegerValue( "bot_memo_boost" );
	/* botmem_paramount_boost: fixed weight boost for paramount map-tactic
	   goals (see BotMem_ParamountLoad); default 16 = the engine max */
	trap_Cvar_Register( 0, "botmem_paramount_boost", "16", 0 );
	botmem_parmboost = trap_Cvar_VariableIntegerValue( "botmem_paramount_boost" );
	if ( botmem_parmboost < 1 ) botmem_parmboost = 1;
	if ( botmem_parmboost > 16 ) botmem_parmboost = 16;
	/* botmem_fight_min (2026-11-09, clan issue #2): minimum samples
	   (good+bad) before a combat weapon can be rejected by the gate;
	   default 3 (min 1: with 0 the gate would divide by total == 0).
	   Subordinate to bot_memory. */
	trap_Cvar_Register( 0, "botmem_fight_min", "3", 0 );
	botmem_fmin = trap_Cvar_VariableIntegerValue( "botmem_fight_min" );
	if ( botmem_fmin < 1 ) botmem_fmin = 1;
	/* botmem_fight_threshold (2026-11-09, clan issue #2): reject a combat
	   weapon when its success rate (integer %) is below this; default 25.
	   Subordinate to bot_memory. */
	trap_Cvar_Register( 0, "botmem_fight_threshold", "25", 0 );
	botmem_fthresh = trap_Cvar_VariableIntegerValue( "botmem_fight_threshold" );
	if ( botmem_fthresh < 0 ) botmem_fthresh = 0;
	if ( botmem_fthresh > 100 ) botmem_fthresh = 100;
	/* MapStart first: it reads the mapname cvar into botmem_map, which
	   LoadLearn needs for the per-map learn file name (learn_<map>.txt) and
	   ParamountLoad for the per-map tactic cvar name */
	BotMem_MapStart();
	BotMem_LoadLearn();
	BotMem_ParamountLoad();
}

/* ================= end OSP 1.04b botmem ================= */

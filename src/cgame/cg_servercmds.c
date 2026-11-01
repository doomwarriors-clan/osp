/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_servercmds.c -- reliably sequenced text commands sent by the server
// these are processed at snapshot transition time, so there will definately
// be a valid snapshot this frame

#include "cg_local.h"
#include "../../ui/menudef.h" // bk001205 - for Q3_ui as well

typedef struct {
	const char *order;
	int taskNum;
} orderTask_t;

static const orderTask_t validOrders[] = {
	{ VOICECHAT_GETFLAG,						TEAMTASK_OFFENSE },
	{ VOICECHAT_OFFENSE,						TEAMTASK_OFFENSE },
	{ VOICECHAT_DEFEND,							TEAMTASK_DEFENSE },
	{ VOICECHAT_DEFENDFLAG,					TEAMTASK_DEFENSE },
	{ VOICECHAT_PATROL,							TEAMTASK_PATROL },
	{ VOICECHAT_CAMP,								TEAMTASK_CAMP },
	{ VOICECHAT_FOLLOWME,						TEAMTASK_FOLLOW },
	{ VOICECHAT_RETURNFLAG,					TEAMTASK_RETRIEVE },
	{ VOICECHAT_FOLLOWFLAGCARRIER,	TEAMTASK_ESCORT }
};

static const int numValidOrders = sizeof(validOrders) / sizeof(orderTask_t);

#ifdef MISSIONPACK // bk001204
static int CG_ValidOrder(const char *p) {
	int i;
	for (i = 0; i < numValidOrders; i++) {
		if (Q_stricmp(p, validOrders[i].order) == 0) {
			return validOrders[i].taskNum;
		}
	}
	return -1;
}
#endif

/*
=================
CG_ParseScores

=================
*/
static void CG_ParseScores( void ) {
	int		i, powerups;

	cg.numScores = atoi( CG_Argv( 1 ) );
	if ( cg.numScores > MAX_CLIENTS ) {
		cg.numScores = MAX_CLIENTS;
	}

	cg.teamScores[0] = atoi( CG_Argv( 2 ) );
	cg.teamScores[1] = atoi( CG_Argv( 3 ) );

	memset( cg.scores, 0, sizeof( cg.scores ) );
	for ( i = 0 ; i < cg.numScores && i < 18 ; i++ ) {	// OSP limit 18
		//
		cg.scores[i].client = atoi( CG_Argv( i * 14 + 4 ) );
		cg.scores[i].score = atoi( CG_Argv( i * 14 + 5 ) );
		cg.scores[i].ping = atoi( CG_Argv( i * 14 + 6 ) );
		cg.scores[i].time = atoi( CG_Argv( i * 14 + 7 ) );
		cg.scores[i].scoreFlags = atoi( CG_Argv( i * 14 + 8 ) );
		powerups = atoi( CG_Argv( i * 14 + 9 ) );
		cg.scores[i].accuracy = atoi(CG_Argv(i * 14 + 10));
		cg.scores[i].impressiveCount = atoi(CG_Argv(i * 14 + 11));
		cg.scores[i].excellentCount = atoi(CG_Argv(i * 14 + 12));
		cg.scores[i].guantletCount = atoi(CG_Argv(i * 14 + 13));
		cg.scores[i].defendCount = atoi(CG_Argv(i * 14 + 14));
		cg.scores[i].assistCount = atoi(CG_Argv(i * 14 + 15));
		cg.scores[i].perfect = atoi(CG_Argv(i * 14 + 16));
		cg.scores[i].captures = atoi(CG_Argv(i * 14 + 17));

		if ( cg.scores[i].client < 0 || cg.scores[i].client >= MAX_CLIENTS ) {
			cg.scores[i].client = 0;
		}
		cgs.clientinfo[ cg.scores[i].client ].score = cg.scores[i].score;
		cgs.clientinfo[ cg.scores[i].client ].powerups = powerups;

		cg.scores[i].team = cgs.clientinfo[cg.scores[i].client].team;
	}
#ifdef MISSIONPACK
	CG_SetScoreSelection(NULL);
#endif

}

/*
=================
CG_ParseTeamInfo

=================
*/
// OSP: custom locations (the location argument is a position), and in
// multiview the location is missing and a seventh argument packs three more values ("a_b_c")
static void CG_ParseTeamInfo( void ) {
	int		i;
	int		client;
	int		mv;

	mv = 0;
	numSortedTeamPlayers = atoi( CG_Argv( 1 ) );
	if ( cg.mvActive ) {
		mv = 1;
	}
	// the count and every client number come from the server: clamp the count to the
	// overlay table and to what the command actually carries, reject bad client numbers
	if ( numSortedTeamPlayers < 0 || numSortedTeamPlayers > TEAM_MAXOVERLAY ||
		numSortedTeamPlayers * 6 + 2 > trap_Argc() ) {
		numSortedTeamPlayers = 0;
		return;
	}

	for ( i = 0 ; i < numSortedTeamPlayers ; i++ ) {
		client = atoi( CG_Argv( i * 6 + 2 ) );
		if ( client < 0 || client >= MAX_CLIENTS ) {
			numSortedTeamPlayers = 0;
			return;
		}

		sortedTeamPlayers[i] = client;

		if ( cg.customLocationsEnabled != 0 && cg.mvActive == 0 ) {
			CG_CustomLocationsSetLocation( CG_Argv( i * 6 + 3 ), cgs.clientinfo[ client ].customLocation );
		} else if ( !cg.mvActive ) {
			cgs.clientinfo[ client ].location = atoi( CG_Argv( i * 6 + 3 ) );
			if ( cgs.clientinfo[ client ].location < 0 || cgs.clientinfo[ client ].location >= MAX_LOCATIONS ) {
				cgs.clientinfo[ client ].location = 0;
			}
		}
		cgs.clientinfo[ client ].health = atoi( CG_Argv( i * 6 + 4 - mv ) );
		cgs.clientinfo[ client ].armor = atoi( CG_Argv( i * 6 + 5 - mv ) );
		cgs.clientinfo[ client ].curWeapon = atoi( CG_Argv( i * 6 + 6 - mv ) );
		if ( cgs.clientinfo[ client ].curWeapon < 0 || cgs.clientinfo[ client ].curWeapon >= WP_NUM_WEAPONS ) {
			cgs.clientinfo[ client ].curWeapon = WP_NONE;
		}
		cgs.clientinfo[ client ].powerups = atoi( CG_Argv( i * 6 + 7 - mv ) );
		if ( mv ) {
			char	*p;
			char	buf[32];
			char	*q;

			Q_strncpyz( buf, CG_Argv( i * 6 + 7 ), sizeof( buf ) );
			p = strchr( buf, '_' );
			if ( p ) {
				*p = 0;
				p++;
				q = strchr( p, '_' );
				if ( q ) {
					*q = 0;
					q++;
					cgs.clientinfo[ client ].teamTask = atoi( q );
				}
				cgs.clientinfo[ client ].score = atoi( p );
			}
			cgs.clientinfo[ client ].ammo = atoi( buf );
		}
	}
}


/*
================
CG_ParseServerinfo

This is called explicitly when the gamestate is first received,
and whenever the server updates any serverinfo flagged cvars
================
*/
void CG_ParseServerinfo( void ) {
	const char	*info;
	char	*mapname;

	info = CG_ConfigString( CS_SERVERINFO );
	cgs.gametype = atoi( Info_ValueForKey( info, "g_gametype" ) );
	cgs.dmflags = atoi( Info_ValueForKey( info, "dmflags" ) );
	cgs.teamflags = atoi( Info_ValueForKey( info, "teamflags" ) );
	cgs.fraglimit = atoi( Info_ValueForKey( info, "fraglimit" ) );
	cgs.capturelimit = atoi( Info_ValueForKey( info, "capturelimit" ) );
	cgs.timelimit = atoi( Info_ValueForKey( info, "timelimit" ) );
	cgs.maxclients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cgs.mapname, sizeof( cgs.mapname ), "maps/%s.bsp", mapname );
	// OSP: no g_gametype cvar set, no team names
}

/*
==================
CG_ParseWarmup
==================
*/
static void CG_ParseWarmup( void ) {
	const char	*info;
	int			warmup;

	info = CG_ConfigString( CS_WARMUP );

	warmup = atoi( info );
	cg.warmupCount = -1;

	if ( warmup == 0 && cg.warmup ) {

	} else if ( warmup > 0 && cg.warmup <= 0 ) {
#ifdef MISSIONPACK
		if (cgs.gametype >= GT_CTF && cgs.gametype <= GT_HARVESTER) {
			trap_S_StartLocalSound( cgs.media.countPrepareTeamSound, CHAN_ANNOUNCER );
		} else
#endif
		{
			trap_S_StartLocalSound( cgs.media.countPrepareSound, CHAN_ANNOUNCER );
		}
	}

	cg.warmup = warmup;
}

/*
================
CG_SetConfigValues

Called on load to set the initial values from configure strings
================
*/
void CG_SetConfigValues( void ) {
	const char *s;

	// OSP score configstrings carry a second number (team counts), the
	// flag status two more characters
	sscanf( CG_ConfigString( CS_SCORES1 ), "%i %i", &cgs.scores1, &cgs.osp_teamcount1 );
	sscanf( CG_ConfigString( CS_SCORES2 ), "%i %i", &cgs.scores2, &cgs.osp_teamcount2 );
	cgs.levelStartTime = atoi( CG_ConfigString( CS_LEVEL_START_TIME ) );
	if( cgs.gametype == GT_CTF ) {
		s = CG_ConfigString( CS_FLAGSTATUS );
		cgs.redflag = s[0] - '0';
		cgs.blueflag = s[1] - '0';
		if ( strlen( s ) > 3 ) {
			cgs.ospFlagStatus = 1;
			cgs.ospRedflag = s[2] - '0';
			cgs.ospBlueflag = s[3] - '0';
		}
	}
#ifdef MISSIONPACK
	else if( cgs.gametype == GT_1FCTF ) {
		s = CG_ConfigString( CS_FLAGSTATUS );
		cgs.flagStatus = s[0] - '0';
	}
#endif
	cg.warmup = atoi( CG_ConfigString( CS_WARMUP ) );
}

/*
=====================
CG_ShaderStateChanged
=====================
*/
void CG_ShaderStateChanged(void) {
	char originalShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	char timeOffset[16];
	const char *o;
	char *n,*t;

	o = CG_ConfigString( CS_SHADERSTATE );
	while (o && *o) {
		n = strstr(o, "=");
		if (n && *n) {
			strncpy(originalShader, o, n-o);
			originalShader[n-o] = 0;
			n++;
			t = strstr(n, ":");
			if (t && *t) {
				strncpy(newShader, n, t-n);
				newShader[t-n] = 0;
			} else {
				break;
			}
			t++;
			o = strstr(t, "@");
			if (o) {
				strncpy(timeOffset, t, o-t);
				timeOffset[o-t] = 0;
				o++;
				trap_R_RemapShader( originalShader, newShader, timeOffset );
			}
		} else {
			break;
		}
	}
}

/*
================
CG_ConfigStringModified

================
*/
// OSP configstrings (values and names as in the qagame's g_local.h)
#define	CS_OSP_PHYSICS			806	// the CPM_UpdateSettings mode
#define	CS_OSP_CUSTOMCLIENT		807	// server_customclient
#define	CS_OSP_PMOVEFIXED		809	// pmove_fixed
#define	CS_OSP_MAXPACKETSMIN	810
#define	CS_OSP_MAXPACKETSMAX	811
#define	CS_OSP_TIMENUDGEMIN		812
#define	CS_OSP_TIMENUDGEMAX		813
#define	CS_OSP_CUSTOMMODE		815
#define	CS_OSP_MOTD				816	// 816..823
#define	CS_OSP_AUTH				872	// server_ospauth
#define	CS_OSP_FREEZETAG		873	// server_freezetag: 0, 1 or 2

static void CG_ConfigStringModified( void ) {
	const char	*str;
	int		num;

	num = atoi( CG_Argv( 1 ) );

	// get the gamestate from the client system, which will have the
	// new configstring already integrated
	trap_GetGameState( &cgs.gameState );

	// look up the individual string that was modified
	str = CG_ConfigString( num );

	// do something with it if necessary
	if ( num == CS_MUSIC ) {
		CG_StartMusic();
	} else if ( num == CS_SERVERINFO ) {
		CG_ParseServerinfo();
		cg.osp_unk_1e83c = 1;
	} else if ( num == CS_WARMUP ) {
		CG_ParseWarmup();
		cg.osp_unk_1e83c = 1;
	} else if ( num == CS_SCORES1 ) {
		sscanf( str, "%i %i", &cgs.scores1, &cgs.osp_teamcount1 );
		cg.osp_unk_1e83c = 1;
	} else if ( num == CS_SCORES2 ) {
		sscanf( str, "%i %i", &cgs.scores2, &cgs.osp_teamcount2 );
		cg.osp_unk_1e83c = 1;
	} else if ( num == CS_LEVEL_START_TIME ) {
		cgs.levelStartTime = atoi( str );
		cg.osp_unk_1e83c = 1;
	} else if ( num == CS_VOTE_TIME ) {
		cgs.voteTime = atoi( str );
		cgs.voteModified = qtrue;
	} else if ( num == CS_VOTE_YES ) {
		cgs.voteYes = atoi( str );
		cgs.voteModified = qtrue;
	} else if ( num == CS_VOTE_NO ) {
		cgs.voteNo = atoi( str );
		cgs.voteModified = qtrue;
	} else if ( num == CS_VOTE_STRING ) {
		Q_strncpyz( cgs.voteString, str, sizeof( cgs.voteString ) );
	// OSP: no team voting in OSP (CS_TEAMVOTE_* handling removed)
	} else if ( num == CS_INTERMISSION ) {
		CG_OSPWStatsUp_f();
		cg.intermissionStarted = atoi( str );
		cg.osp_unk_1e83c = 1;
		if ( cgs.clientinfo[cg.snap->ps.clientNum].team != TEAM_SPECTATOR ||
			( CG_OSPIsGameTypeCA( cgs.gametype ) && cgs.clientinfo[cg.snap->ps.clientNum].rt != TEAM_SPECTATOR ) ) {
			CG_OSPWStatsDown_f();
		}
	} else if ( num >= CS_MODELS && num < CS_MODELS+MAX_MODELS ) {
		cgs.gameModels[ num-CS_MODELS ] = trap_R_RegisterModel( str );
	} else if ( num >= CS_SOUNDS && num < CS_SOUNDS+MAX_MODELS ) {
		if ( str[0] != '*' ) {	// player specific sounds don't register here
			cgs.gameSounds[ num-CS_SOUNDS] = trap_S_RegisterSound( str, qfalse );
		}
	} else if ( num >= CS_PLAYERS && num < CS_PLAYERS+MAX_CLIENTS ) {
		CG_NewClientInfo( num - CS_PLAYERS );
		CG_BuildSpectatorString();
		cg.osp_unk_1e83c = 1;
	} else if ( num >= CS_OSP_MOTD && num < CS_OSP_MOTD + 8 ) {
	} else if ( num == CS_FLAGSTATUS ) {
		if( cgs.gametype == GT_CTF ) {
			// format is rb where its red/blue, 0 is at base, 1 is taken, 2 is dropped
			cgs.redflag = str[0] - '0';
			cgs.blueflag = str[1] - '0';
			cg.osp_unk_1e83c = 1;
			if ( strlen( str ) > 3 ) {
				cgs.ospFlagStatus = 1;
				cgs.ospRedflag = str[2] - '0';
				cgs.ospBlueflag = str[3] - '0';
			}
		}
	} else if ( num == CS_OSP_PMOVEFIXED ) {
		cgs.allow_pmove = atoi( str );
		if ( cgs.allow_pmove == 0 && pmove_fixed.integer != 0 ) {
			CG_Printf( "^3Enhanced client sampling is disabled on this server (pmove_fixed = 0)\n" );
			trap_Cvar_Set( pmove_fixed.string, "0" );	// sic: the cvar's value, not its name
		}
	} else if ( num == CS_OSP_MAXPACKETSMIN ) {
		cgs.serverConfigMinimumMaxpackets = atoi( str );
		if ( cgs.serverConfigMinimumMaxpackets && cgs.serverConfigMinimumMaxpackets > cl_maxpackets.integer ) {
			CG_Printf( "^3Minimum maxpackets set to %d.  Adjusting...\n", cgs.serverConfigMinimumMaxpackets );
			trap_Cvar_Set( "cl_maxpackets", va( "%d", cgs.serverConfigMinimumMaxpackets ) );
		}
	} else if ( num == CS_OSP_MAXPACKETSMAX ) {
		cgs.serverConfigMaximumMaxpackets = atoi( str );
		if ( cgs.serverConfigMaximumMaxpackets && cgs.serverConfigMaximumMaxpackets < cl_maxpackets.integer ) {
			CG_Printf( "^3Maximum maxpackets set to %d.  Adjusting...\n", cgs.serverConfigMaximumMaxpackets );
			trap_Cvar_Set( "cl_maxpackets", va( "%d", cgs.serverConfigMaximumMaxpackets ) );
		}
	} else if ( num == CS_OSP_TIMENUDGEMIN ) {
		cgs.serverConfigMinimumTimenudge = atoi( str );
		if ( cgs.serverConfigMinimumTimenudge && cgs.serverConfigMinimumTimenudge > cl_timenudge.integer ) {
			CG_Printf( "^3Minimum timenudge set to %d.  Adjusting...\n", cgs.serverConfigMinimumTimenudge );
			trap_Cvar_Set( "cl_timenudge", va( "%d", cgs.serverConfigMinimumTimenudge ) );
		}
	} else if ( num == CS_OSP_TIMENUDGEMAX ) {
		cgs.serverConfigMaximumTimenudge = atoi( str );
		if ( cgs.serverConfigMaximumTimenudge && cgs.serverConfigMaximumTimenudge < cl_timenudge.integer ) {
			CG_Printf( "^3Maximum timenudge set to %d.  Adjusting...\n", cgs.serverConfigMaximumTimenudge );
			trap_Cvar_Set( "cl_timenudge", va( "%d", cgs.serverConfigMaximumTimenudge ) );
		}
	} else if ( num == CS_OSP_AUTH ) {
		cgs.configString_0x368 = atoi( str ) & 1;
	} else if ( num == CS_OSP_CUSTOMMODE ) {
		cgs.clanBaseTeamDM = atoi( str ) ? qtrue : qfalse;
	} else if ( num == CS_OSP_CUSTOMCLIENT ) {
		cgs.custom_client_2 = atoi( str );
		if ( !( cgs.custom_client_2 & 4 ) && ( cg_altPlasma.integer || cg_altLightning.integer || cg_altGrenades.integer ) ) {
			CG_Printf( "^3Alternate weapon graphics disabled.\n" );
			trap_Cvar_Set( "cg_altPlasma", "0" );
			trap_Cvar_Set( "cg_altGrenades", "0" );
		}
		if ( !( cgs.custom_client_2 & 8 ) && cg_enableOSPHUD.integer && !( cgs.server_mode & 1 ) ) {
			CG_Printf( "^3The OSP-based HUD has been disabled.\n" );
			trap_Cvar_Set( "cg_enableOSPHUD", "0" );
		}
		if ( ( cgs.custom_client_2 & 16 ) && cg_drawTimer.integer ) {
			CG_Printf( "^3The timer has been disabled.\n" );
		}
		CG_OSPCvarsRestrictValues();
	} else if ( num == CS_OSP_PHYSICS ) {
		cgs.server_mode = ( atoi( str ) & 1 ) ? 1 : ( ( atoi( str ) & 2 ) ? 2 : 0 );
		CPM_UpdateSettings( atoi( str ) );
		if ( cgs.server_mode & 1 ) {
			cg.hmiActive = 0;
		}
	} else if ( num == CS_OSP_FREEZETAG ) {
		cgs.gameTypeFreeze = ( atoi( str ) == 0 ) ? 0 : ( ( atoi( str ) == 2 ) ? 2 : 1 );
	}
	else if ( num == CS_SHADERSTATE ) {
		CG_ShaderStateChanged();
	}
		
}


/*
=======================
CG_AddToTeamChat

=======================
*/
// OSP: ch_FilterLocationsTeamchat cuts the second "(...)" (the location) out
// of the message, the line width follows cf_Teamchat's font size and the team overlay width,
// ch_InverseTeamChat keeps the newest line on top
// OSP: global (static in id), HMI_TeamchatLines calls it
void CG_AddToTeamChat( char *str ) {
	int len;
	char *p, *ls;
	int lastcolor;
	int chatHeight;
	int maxWidth;
	int w, h;
	char *loc;

	if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT) {
		chatHeight = cg_teamChatHeight.integer;
	} else {
		chatHeight = TEAMCHAT_HEIGHT;
	}

	if (chatHeight <= 0 || cg_teamChatTime.integer <= 0) {
		// team chat disabled, dump into normal chat
		cgs.teamChatPos = cgs.teamLastChatPos = 0;
		return;
	}

	if ( ch_FilterLocationsTeamchat.integer ) {
		char *s, *e;
		int n;

		s = strchr( str, '(' );
		if ( !s ) {
		} else {
			s = strchr( s + 1, '(' );
			if ( !s ) {
			} else {
				e = strchr( s, ')' );
				if ( !e ) {
				} else {
					s--;
					e++;
					n = strlen( e );
					loc = s + 2;
					while ( n-- ) {
						*s++ = *e++;
					}
					*s = 0;
				}
			}
		}
	}

	len = 0;

	p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
	*p = 0;

	lastcolor = '7';

	CG_OSPGetClientFontSize( &cf_Teamchat, &w, &h );
	maxWidth = ( SCREEN_WIDTH - ( cg_drawTeamOverlay.integer == 2 ? cg.teamOverlayWidth : 0 ) ) / w;

	ls = NULL;
	while (*str) {
		if (len > maxWidth - 1) {
			if (ls) {
				str -= (p - ls);
				str++;
				p -= (p - ls);
			}
			*p = 0;

			cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;

			cgs.teamChatPos++;
			if ( ch_InverseTeamChat.integer ) {
				Q_strncpyz( cgs.teamChatMsgs[cgs.teamChatPos % chatHeight], cgs.teamChatMsgs[( cgs.teamChatPos - 1 ) % chatHeight], sizeof( cgs.teamChatMsgs[0] ) );
				p = cgs.teamChatMsgs[( cgs.teamChatPos - 1 ) % chatHeight];
			} else {
				p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
			}
			*p = 0;
			*p++ = Q_COLOR_ESCAPE;
			*p++ = lastcolor;
			len = 0;
			ls = NULL;
		}

		if ( Q_IsColorString( str ) ) {
			*p++ = *str++;
			lastcolor = *str;
			*p++ = *str++;
			continue;
		}
		if (*str == ' ') {
			ls = p;
		}
		*p++ = *str++;
		len++;
	}
	*p = 0;

	cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;
	cgs.teamChatPos++;

	if (cgs.teamChatPos - cgs.teamLastChatPos > chatHeight)
		cgs.teamLastChatPos = cgs.teamChatPos - chatHeight;
}

/*
===============
CG_MapRestart

The server has issued a map_restart, so the next snapshot
is completely new and should not be interpolated to.

A tournement restart will clear everything, but doesn't
require a reload of all the media
===============
*/
static void CG_MapRestart( void ) {
	int		i;

	if ( cg_showmiss.integer ) {
		CG_Printf( "CG_MapRestart\n" );
	}

	CG_InitLocalEntities();
	CG_InitMarkPolys();
	CG_ClearParticles ();

	// make sure the "3 frags left" warnings play again
	cg.fraglimitWarnings = 0;

	cg.timelimitWarnings = 0;

	// OSP clears the reward stack and the announcer queue
	cg.rewardStack = 0;
	cg.soundBufferIn = 0;
	cg.soundBufferOut = 0;
	for ( i = 0 ; i < MAX_SOUNDBUFFER ; i++ ) {
		cg.soundBuffer[i] = 0;
	}
	cg.soundTime = cg.time + 500;

	cg.intermissionStarted = qfalse;

	CG_OSPWStatsUp_f();

	cgs.voteTime = 0;

	cg.mapRestart = qtrue;

	CG_StartMusic();

	trap_S_ClearLoopingSounds(qtrue);

	// we really should clear more parts of cg here and stop sounds

	// play the "fight" sound if this is a restart without warmup
	if ( cg.warmup == 0 /* && cgs.gametype == GT_TOURNAMENT */) {
		trap_S_StartLocalSound( cgs.media.countFightSound, CHAN_ANNOUNCER );
		CG_CenterPrint( "^1FIGHT!", 20, GIANTCHAR_WIDTH*2 );	// OSP text and position
	}
#ifdef MISSIONPACK
	if (cg_singlePlayerActive.integer) {
		trap_Cvar_Set("ui_matchStartTime", va("%i", cg.time));
		if (cg_recordSPDemo.integer && cg_recordSPDemoName.string && *cg_recordSPDemoName.string) {
			trap_SendConsoleCommand(va("set g_synchronousclients 1 ; record %s \n", cg_recordSPDemoName.string));
		}
	}
#endif
	// OSP resends its client settings (no cg_thirdPerson reset)
	if ( cgs.isOSPv1 ) {
		CG_OSPUpdateUserInfo( qtrue );
	}
}

// OSP: no voice chats in OSP (voiceChatLists, headModelVoiceChat,
// voiceChatBuffer and their functions removed)

/*
=================
CG_RemoveChatEscapeChar
=================
*/
// OSP also strips ^X / ^x RRGGBB colours, maps ^8 to ^3 and ^9 to ^7 and drops
// every other unknown colour code; non-static (CG_Obituary, CG_LoadingClient call it)
#define OSP_IS_HEX( c ) ( ( ( c ) >= '0' && ( c ) <= '9' ) || ( ( c ) >= 'A' && ( c ) <= 'F' ) || ( ( c ) >= 'a' && ( c ) <= 'f' ) )

void CG_RemoveChatEscapeChar( char *text ) {
	int i, l;

	l = 0;
	for ( i = 0; text[i]; i++ ) {
		if (text[i] == '\x19')
			continue;
		if ( text[i] == '^' ) {
			if ( text[i+1] == 'X' || text[i+1] == 'x' ) {
				i++;
				if ( ( text + i )[1] && OSP_IS_HEX( ( text + i )[1] ) && ( text + i )[2] && OSP_IS_HEX( ( text + i )[2] ) &&
					( text + i )[3] && OSP_IS_HEX( ( text + i )[3] ) && ( text + i )[4] && OSP_IS_HEX( ( text + i )[4] ) &&
					( text + i )[5] && OSP_IS_HEX( ( text + i )[5] ) && ( text + i )[6] && OSP_IS_HEX( ( text + i )[6] ) ) {
					i += 6;
					continue;
				}
			} else {
				char c = text[i+1];

				if ( c ) {
					if ( c == '8' ) {
						i++;
						text[l++] = '^';
						text[l++] = '3';
						continue;
					}
					if ( c == '9' ) {
						i++;
						text[l++] = '^';
						text[l++] = '7';
						continue;
					}
					if ( ( c > '7' || c < '0' ) && c != '^' ) {
						i++;
						continue;
					}
				}
			}
		}
		text[l++] = text[i];
	}
	text[l] = '\0';
}

/*
=================
CG_ServerCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
// OSP commands (auth, stats, multiview, stuff, client record/screenshot),
// colour code filtering of printed text, custom team chat locations, Q_stricmp for most keywords
static void CG_ServerCommand( void ) {
	const char	*cmd;
	char		text[MAX_STRING_CHARS];	// OSP (id: MAX_SAY_TEXT)

	cmd = CG_Argv(0);

	if ( !cmd[0] ) {
		// server claimed the command
		return;
	}

	if ( !strcmp( cmd, "psohtua" ) ) {
		CG_OSPClientAuth_f();
		return;
	}

	if ( !strcmp( cmd, "cp" ) ) {
		if ( strstr( CG_Argv(1), "Multi-view demo created with" ) ) {
			return;
		}
		CG_CenterPrint( CG_Argv(1), SCREEN_HEIGHT * 0.25, BIGCHAR_WIDTH );
		return;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CG_ConfigStringModified();
		return;
	}

	if ( !strcmp( cmd, "print" ) ) {
		Q_strncpyz( text, CG_Argv(1), MAX_STRING_CHARS );
		CG_RemoveChatEscapeChar( text );
		CG_Printf( "%s", text );
		return;
	}

	if ( !strcmp( cmd, "chat" ) ) {
		if ( !cg_teamChatsOnly.integer ) {
			if ( !cg_nochatbeep.integer ) {
				trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
			}
			Q_strncpyz( text, CG_Argv(1), MAX_STRING_CHARS );
			CG_RemoveChatEscapeChar( text );
			CG_Printf( "%s\n", text );
		}
		return;
	}

	if ( !strcmp( cmd, "tchat" ) ) {
		char	*msg = text;

		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
		Q_strncpyz( text, CG_Argv(1), MAX_STRING_CHARS );
		CG_RemoveChatEscapeChar( text );
		if ( cg.customLocationsEnabled ) {
			msg = CG_CustomLocationsTeamChatCode( text );
		}
		CG_AddToTeamChat( msg );
		if ( !ch_TeamchatOnly.integer || cgs.gametype == GT_TOURNAMENT ) {
			CG_Printf( "%s\n", msg );
		}
		return;
	}
	if ( !Q_stricmp( cmd, "scores" ) ) {
		CG_ParseScores();
		return;
	}

	if ( !Q_stricmp( cmd, "tinfo" ) ) {
		cg.teamOverlayChanged = 1;
		CG_ParseTeamInfo();
		return;
	}

	if ( !Q_stricmp( cmd, "map_restart" ) ) {
		CG_MapRestart();
		return;
	}

  if ( Q_stricmp (cmd, "remapShader") == 0 ) {
		if (trap_Argc() == 4) {
			trap_R_RemapShader(CG_Argv(1), CG_Argv(2), CG_Argv(3));
		}
	}

	if ( !Q_stricmp( cmd, "statsinfo" ) ) {
		CG_OSPShowStatsInfo();
		return;
	}

	if ( !Q_stricmp( cmd, "viewlist" ) ) {
		CG_OSPParseViewList();
		return;
	}

	if ( !Q_stricmp( cmd, "sset" ) ) {
		CG_OSPPrintSSet();
		return;
	}

	if ( !Q_stricmp( cmd, "viewactive" ) ) {
		int		oldView = cg.mvCurrent;
		char	buf[64];

		trap_Argv( 1, buf, sizeof( buf ) );
		cg.mvCurrent = atoi( buf );
		if ( oldView != cg.mvCurrent ) {
			CG_OSPPlaySound( NULL, 0, trap_S_RegisterSound( "sound/misc/menu2.wav", qfalse ) );
		}
		return;
	}

	if ( !Q_stricmp( cmd, "stuff" ) ) {
		CG_ServerCommandStuff();
		return;
	}

	if ( !Q_stricmp( cmd, "crecord" ) ) {
		CG_OSPClientRecord();
		return;
	}

	if ( !Q_stricmp( cmd, "cscreenshot" ) ) {
		CG_OSPClientScreenshot();
		return;
	}

	if ( !strcmp( cmd, "xstats1" ) ) {
		CG_OSPPrintXStats();
		return;
	}

	if ( !strcmp( cmd, "astats" ) ) {
		CG_OSPAstats( qtrue );
		return;
	}

	if ( !strcmp( cmd, "astatsb" ) ) {
		CG_OSPAstats( qfalse );
		return;
	}

	if ( !strcmp( cmd, "bstats" ) ) {
		CG_OSPBstats( qtrue );
		return;
	}

	if ( !strcmp( cmd, "bstatsb" ) ) {
		CG_OSPBstats( qfalse );
		return;
	}

	// loaddeferred can be both a servercmd and a consolecmd
	if ( !Q_stricmp( cmd, "loaddefered" ) ) {	// FIXME: spelled wrong, but not changing for demo
		CG_LoadDeferredPlayers();
		return;
	}

	// clientLevelShot is sent before taking a special screenshot for
	// the menu system during development
	if ( !Q_stricmp( cmd, "clientLevelShot" ) ) {
		cg.levelShot = qtrue;
		return;
	}

	CG_Printf( "Unknown client game command: %s\n", cmd );
}


/*
====================
CG_ExecuteNewServerCommands

Execute all of the server commands that were received along
with this this snapshot.
====================
*/
void CG_ExecuteNewServerCommands( int latestSequence ) {
	while ( cgs.serverCommandSequence < latestSequence ) {
		if ( trap_GetServerCommand( ++cgs.serverCommandSequence ) ) {
			CG_ServerCommand();
		}
	}
}

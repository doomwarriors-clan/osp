/*
 * cg_osp_cmds.c: OSP console commands (+action, hudstyle, myname, decal editing, wstats) and
 * OSP server commands (clientauth reply, stuff, crecord / cscreenshot, statsinfo, xstats1,
 * sset, astats, bstats).
 */
#include "cg_local.h"

// the +wstats window: index + 1 of its ospPrintContext slot (0: no window) and the pending
// "getstatsinfo" request
int			wstatsWndId = 0;
qboolean	wstatsEnabled = qfalse;

// weapon names of the stats prints
char		*weaponShortNames[10] = { "", "GAUNTLET", "MG", "SG", "GL", "RL", "LG", "RG", "PG", "BFG" };
char		*weaponNames[11] = { "None", "Gauntlet", "MachineGun", "Shotgun", "G.Launcher", "R.Launcher", "LightningGun", "Railgun", "Plasmagun", "BFG", "Unknown" };

// the PROMODE HUD style (hudstyle / hudnext / hudprev)
int			hudStyle = 0;

// the "statsinfo" values and the +action keys held down
int			statsInfo[22];
int			actionKeyDown[12];


// +action<n>: releases action key n and executes the cvar "action<n>up"
void CG_OSPActionUp_f( void ) {
	const char	*arg;
	char	buf[MAX_CVAR_VALUE_STRING];
	char	*cvar;
	int		num;

	arg = CG_Argv( 1 );
	if ( *arg >= '0' && *arg <= '9' ) {
		num = atoi( arg );
		actionKeyDown[num] = 0;
	}
	cvar = va( "action%sup", arg );
	trap_Cvar_VariableStringBuffer( cvar, buf, MAX_CVAR_VALUE_STRING );
	trap_SendConsoleCommand( buf );
}

// +action<n>: executes the cvar "action<n>down" once per key press
void CG_OSPActionDown_f( void ) {
	const char	*arg;
	char	buf[MAX_CVAR_VALUE_STRING];
	char	*cvar;
	int		num;

	arg = CG_Argv( 1 );
	if ( *arg >= '0' && *arg <= '9' ) {
		num = atoi( arg );
		if ( actionKeyDown[num] ) {
			return;
		}
		actionKeyDown[num] = 1;
	}
	cvar = va( "action%sdown", arg );
	trap_Cvar_VariableStringBuffer( cvar, buf, MAX_CVAR_VALUE_STRING );
	trap_SendConsoleCommand( buf );
}

// hudstyle <n>: selects the PROMODE HUD style
void CG_OSPHUDStyle_f( void ) {
	int		style;
	char	buf[128];

	if ( !( cgs.server_mode & 1 ) ) {
		return;
	}
	trap_Argv( 1, buf, 128 );
	style = atoi( buf );
	hudStyle = CG_OSPSetHUDStyle( style );
}

// hudnext: next PROMODE HUD style
void CG_OSPHUDNext_f( void ) {
	if ( !( cgs.server_mode & 1 ) ) {
		return;
	}
	hudStyle = CG_OSPSetHUDStyle( hudStyle + 1 );
	CG_Printf( "HUD style is now %d\n", hudStyle );
}

// hudprev: previous PROMODE HUD style
void CG_OSPHUDPrev_f( void ) {
	if ( !( cgs.server_mode & 1 ) ) {
		return;
	}
	hudStyle = CG_OSPSetHUDStyle( hudStyle - 1 );
	CG_Printf( "HUD style is now %d\n", hudStyle );
}

// myname: prints the own name with its colours
void CG_OSPMyName_f( void ) {
	CG_Printf( "Your name is: '%s^7'\n", cgs.clientinfo[cg.clientNum].name );
}

// clientversion
void CG_OSPClientVersion_f( void ) {
	CG_Printf( "^3OSP Client Version:^7 %s\n", "1.03" );
}

// currenttime: prints the local time and date (also at startup)
void CG_OSPPrintTime_f( void ) {
	qtime_t	qtime;
	char	*months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	trap_RealTime( &qtime );
	CG_Printf( "\nCurrent time: ^3%02d:%02d:%02d (%02d %s %d)\n\n", qtime.tm_hour, qtime.tm_min, qtime.tm_sec, qtime.tm_mday, months[qtime.tm_mon], qtime.tm_year + 1900 );
}

// clientauth <n> (server command): answers the server's authentication request with
// "htuatneilc <secret>" made from the own name and the request
void CG_OSPClientAuth_f( void ) {
	char	str[64];

	trap_Argv( 1, str, 64 );
	if ( !atoi( str ) ) {
		CG_Printf( "\n^3 >>>>> Warning: ^7Detected invalid authentication request! (NULL buf) <<<<<\n" );
		return;
	}
	BG_OSPAuthInit();
	BG_OSPAuthUpdateName( cgs.clientinfo[cg.clientNum].name );
	BG_OSPAuthUpdateName( str );
	memset( str, 0, 64 );
	BG_OSPAuthGetSecret( str );
	trap_SendClientCommand( va( "htuatneilc %s", str ) );
}

// a demo / screenshot name sent by the server: trims blanks and refuses anything that could
// break out of a quoted console argument, expand a cvar or leave the target directory
static qboolean CG_OSPSafeFileName( char *name, int maxLen ) {
	char	*p;
	int		len;

	while ( *name == ' ' ) {
		memmove( name, name + 1, strlen( name ) );
	}
	len = strlen( name );
	while ( len > 0 && name[len - 1] == ' ' ) {
		name[--len] = 0;
	}
	if ( !len || len > maxLen || strstr( name, ".." ) ) {
		return qfalse;
	}
	for ( p = name; *p; p++ ) {
		if ( *p < ' ' || *p > '~' || *p == '"' || *p == ';' || *p == '\\' || *p == '/' || *p == ':' ||
			*p == '*' || *p == '?' || *p == '<' || *p == '>' || *p == '|' || *p == '$' || *p == '`' ) {
			return qfalse;
		}
	}
	return qtrue;
}

// a relative path ending in .txt below the mod directory (the stats condump)
static qboolean CG_OSPSafeTextPath( const char *path ) {
	const char	*p;
	int			len;

	len = strlen( path );
	if ( len < 5 || len >= MAX_QPATH || Q_stricmp( path + len - 4, ".txt" ) || *path == '/' || strstr( path, ".." ) ) {
		return qfalse;
	}
	for ( p = path; *p; p++ ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) || ( *p >= '0' && *p <= '9' ) ||
			*p == '_' || *p == '-' || *p == '.' || *p == '/' ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

// copies the "stuff" text: '' becomes ', a single ' becomes "
static void CG_OSPStuffUnquote( const char *in, int len, char *out ) {
	int		i;

	i = 0;
	while ( *in && i < len ) {
		if ( *in == '\'' ) {
			in++;
			if ( *in == '\'' ) {
				*out++ = '\'';
			} else {
				*out++ = '"';
			}
			in++;
			i++;
		} else {
			*out++ = *in++;
			i++;
		}
	}
	*out = 0;
}

// one ';'-separated piece of a stuff command: only the commands the OSP server actually sends
// are accepted, every argument is validated and the console line is rebuilt from the tokens
static void CG_OSPStuffOne( char *text ) {
	char	tok[4][MAX_QPATH];
	char	*p;
	int		argc;

	p = text;
	for ( argc = 0; argc < 4; argc++ ) {
		Q_strncpyz( tok[argc], COM_ParseExt( &p, qfalse ), sizeof( tok[argc] ) );
		if ( !tok[argc][0] ) {
			break;
		}
	}
	if ( !argc ) {
		return;
	}

	if ( argc == 1 && ( !Q_stricmp( tok[0], "stoprecord" ) || !Q_stricmp( tok[0], "clear" ) || !Q_stricmp( tok[0], "currenttime" ) ) ) {
		trap_SendConsoleCommand( va( "%s\n", tok[0] ) );
		return;
	}
	if ( argc <= 2 && ( !Q_stricmp( tok[0], "record" ) || !Q_stricmp( tok[0], "screenshot" ) || !Q_stricmp( tok[0], "screenshotJPEG" ) ) ) {
		if ( argc == 1 ) {
			trap_SendConsoleCommand( va( "%s\n", tok[0] ) );
		} else if ( CG_OSPSafeFileName( tok[1], MAX_QPATH - 1 ) ) {
			trap_SendConsoleCommand( va( "%s \"%s\"\n", tok[0], tok[1] ) );
		}
		return;
	}
	if ( argc == 2 && !Q_stricmp( tok[0], "condump" ) && CG_OSPSafeTextPath( tok[1] ) ) {
		trap_SendConsoleCommand( va( "condump \"%s\"\n", tok[1] ) );
		return;
	}
	if ( argc == 3 && ( !Q_stricmp( tok[0], "set" ) || !Q_stricmp( tok[0], "seta" ) ) ) {
		if ( !Q_stricmp( tok[1], "g_synchronousclients" ) && ( !strcmp( tok[2], "0" ) || !strcmp( tok[2], "1" ) ) ) {
			trap_Cvar_Set( "g_synchronousclients", tok[2] );
			return;
		}
		if ( !Q_stricmp( tok[1], "name" ) && CG_OSPSafeFileName( tok[2], 35 ) ) {	// MAX_NETNAME - 1
			trap_Cvar_Set( "name", tok[2] );
			return;
		}
	}
	CG_Printf( "^3Ignored server stuff command: ^7%s\n", text );
}

// stuff <cmd> (server command). OSP used this to run arbitrary console text sent by the
// server (autorecord, autoscreenshot, stats condump, name restore), which gave any server full
// control of the client console. Now only those known commands are executed, see CG_OSPStuffOne.
void CG_ServerCommandStuff( void ) {
	char	cmd[MAX_STRING_CHARS];
	char	out[MAX_STRING_CHARS];
	char	*s;
	char	*next;

	if ( cg.demoPlayback ) {
		return;
	}
	trap_Args( cmd, sizeof( cmd ) );
	cmd[MAX_STRING_CHARS - 1] = 0;
	CG_OSPStuffUnquote( cmd, strlen( cmd ), out );

	for ( s = out; s && *s; s = next ) {
		next = s;
		while ( *next && *next != ';' && *next != '\n' && *next != '\r' ) {
			next++;
		}
		if ( *next ) {
			*next++ = 0;
		} else {
			next = NULL;
		}
		CG_OSPStuffOne( s );
	}
}

// crecord <name> (server command): records a demo under the first free name
// demos/<name>[-nnn].dm3 with synchronous clients
void CG_OSPClientRecord( void ) {
	fileHandle_t	f = 0;
	char			name[MAX_OSPATH];
	char			fname[MAX_OSPATH];
	int				count = 0;

	trap_Args( name, sizeof( name ) );
	name[MAX_OSPATH - 1] = 0;
	if ( !CG_OSPSafeFileName( name, MAX_QPATH - 1 ) ) {
		CG_Printf( "^3Ignored server file name: ^7%s\n", name );
		return;
	}
	do {
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		if ( !count ) {
			Com_sprintf( fname, sizeof( fname ), "%s", name );
		} else {
			Com_sprintf( fname, sizeof( fname ), "%s-%03d", name, count );
		}
		trap_FS_FOpenFile( va( "demos\\%s.dm3", fname ), &f, FS_READ );
		count++;
	} while ( f );
	trap_SendConsoleCommand( va( "set g_synchronousclients 1;record \"%s\" ;set g_synchronousclients 0", fname ) );
}

// cscreenshot <name> (server command): takes a screenshot under the first free name
// screenshots/<name>[-nnn].tga
void CG_OSPClientScreenshot( void ) {
	fileHandle_t	f;
	char			name[MAX_OSPATH];
	char			fname[MAX_OSPATH];
	int				count;

	trap_Args( name, sizeof( name ) );
	name[MAX_OSPATH - 1] = 0;
	if ( !CG_OSPSafeFileName( name, MAX_QPATH - 1 ) ) {
		CG_Printf( "^3Ignored server file name: ^7%s\n", name );
		return;
	}
	count = 0;
	f = 0;
	do {
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		if ( !count ) {
			Com_sprintf( fname, sizeof( fname ), "%s", name );
		} else {
			Com_sprintf( fname, sizeof( fname ), "%s-%03d", name, count );
		}
		trap_FS_FOpenFile( va( "screenshots\\%s.tga", fname ), &f, FS_READ );
		count++;
	} while ( f );
	trap_SendConsoleCommand( va( "screenshot \"%s\"", fname ) );
}

// unreferenced
int CG_OSPStub519( void ) {
	return 0;
}

// credits: the OSP credits in seven sliding windows
void CG_OSPCredits_f( void ) {
	char	str[10][MAX_OSP_SLIDING_CHARS];

	strcpy( str[0], "     ^BOSP Tourney DM/CA/CTF " );
	strcpy( str[1], "  ^3Orange Smoothie Productions " );
	strcpy( str[2], " ^3http://www.OrangeSmoothie.org " );
	CG_OSPDrawLeftSlidingWindow( 1.5f, 1.5f, 12.0f, 0.0f, 3, MAX_OSP_SLIDING_CHARS, 20, 20, str[0], 180.0f, colorYellow, colorOrange );

	strcpy( str[0], "    ^BJason \"Rhea\" Keimig  " );
	strcpy( str[1], "  Lead Design/Development  " );
	strcpy( str[2], "  ^2rhea@OrangeSmoothie.org" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 0.4f, 3, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 240.0f, colorWhite, colorBlue );

	strcpy( str[0], "    ^BChris \"SHoD\" Farcy" );
	strcpy( str[1], "MapPack Design/Development" );
	strcpy( str[2], " ^2shod@OrangeSmoothie.org" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 1.6f, 3, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 290.0f, colorBlue, colorOrange );

	strcpy( str[0], " ^BKevin \"arQon\" Blenkinsopp" );
	strcpy( str[1], "MA/Client/Server Development" );
	strcpy( str[2], "    ^2arQon@promode.org" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 1.2f, 3, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 330.0f, NULL, NULL );

	strcpy( str[0], "^BSander \"Dessignator\" Brandenburg" );
	strcpy( str[1], "       Client Development" );
	strcpy( str[2], " ^2dessignator@OrangeSmoothie.org" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 0.8f, 3, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 370.0f, colorGreen, colorRed );

	strcpy( str[0], "^BPhaNToM, Khaile, HighlandeR and D!ABLO" );
	strcpy( str[1], "           CPM Development" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 2.0f, 2, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 400.0f, colorRed, colorBlack );

	strcpy( str[0], "       ^BHarvey \"Papa\" Morris" );
	strcpy( str[1], "Q3Comp + Client/Server Development" );
	strcpy( str[2], "    ^2papa@reactivesoftware.com" );
	CG_OSPDrawLeftSlidingWindow( 0.7f, 0.9f, 12.0f, 2.4f, 3, MAX_OSP_SLIDING_CHARS, 10, 10, str[0], 440.0f, colorOrange, colorGreen );
}

// +wstats: asks the server for the own weapon stats ("statsinfo" answer)
void CG_OSPWStatsDown_f( void ) {
	if ( !wstatsWndId && !wstatsEnabled ) {
		trap_SendClientCommand( "getstatsinfo" );
		wstatsEnabled = qtrue;
	}
}

// statsinfo (server command): the +wstats window with the own match stats (per game type),
// weapon accuracies, damage and pickups; it stays until -wstats (4 seconds in demos)
void CG_OSPShowStatsInfo( void ) {
	if ( !cg.demoPlayback && ( wstatsWndId > 0 || !wstatsEnabled ) ) {
		return;
	} else {
		int			i;
		int			weapon;
		char		args[MAX_STRING_CHARS];
		int			row;
		char		strings[MAX_OSP_SLIDING_LINES][MAX_OSP_SLIDING_CHARS];
		int			argIdx;
		int			weaponMask;
		qboolean	damageActivity = qfalse;
		float		efficiency;
		int			score;
		float		scrollTime;
		float		y;

		wstatsEnabled = qfalse;
		for ( i = 0; i < 22; i++ ) {
			trap_Argv( i + 1, args, sizeof( args ) );
			statsInfo[i] = atoi( args );
			if ( !i && !statsInfo[i] ) {
				return;
			}
		}
		score = ( !CG_OSPIsGameTypeCA( cgs.gametype ) && !cgs.gameTypeFreeze ) ? statsInfo[1] : statsInfo[3];
		weaponMask = statsInfo[21];
		if ( statsInfo[4] + score == 0 ) {
			efficiency = 0;
		} else {
			efficiency = 100.0f * score / ( statsInfo[4] + (float)score );
		}
		if ( efficiency < 0 ) {
			efficiency = 0;
		}

		if ( cgs.gametype == GT_TOURNAMENT ) {
			strcpy( strings[0], "^B^3Score  ^2Klls ^1Dths Sui ^3Effcny  ^2WINS ^1LOSSES" );
			strcpy( strings[1], va( "%5d  %4d %4d %3d %6.1f  ^3%4d^7 %6d", statsInfo[1], statsInfo[3], statsInfo[4], statsInfo[5], efficiency,
				statsInfo[10] & cgs.stats_mask, statsInfo[11] & cgs.stats_mask ) );
		} else if ( cgs.gametype == GT_TEAM ) {
			if ( !cgs.gameTypeFreeze ) {
				strcpy( strings[0], "^B^3Score NET  ^2Klls ^1Dths Sui ^1TmKlls ^3Effcny" );
				strcpy( strings[1], va( "%5d ^5%3d^7  %4d %4d %3d %6d %6.1f", statsInfo[1], statsInfo[1] - statsInfo[4], statsInfo[3], statsInfo[4],
					statsInfo[5], statsInfo[6], efficiency ) );
			} else {
				strcpy( strings[0], "^B^3Score WINS  ^2Klls Thws ^1Dths Sui ^1TmKlls ^3Effcny" );
				strcpy( strings[1], va( "%5d ^5%4d^7  %4d %4d %4d %3d %6d %6.1f", statsInfo[1], statsInfo[10] & cgs.stats_mask, statsInfo[3],
					statsInfo[11] & cgs.stats_mask, statsInfo[4], statsInfo[5], statsInfo[6], efficiency ) );
			}
		} else if ( cgs.gametype == GT_CTF ) {
			int		min;
			float	sec;

			min = statsInfo[16] / 1000 / 60;
			sec = ( statsInfo[16] - 60000.0f * min ) / 1000.0f;
			strcpy( strings[0], "^B^3Score  ^2Klls ^1Dths ^3Caps   ^2Ftime Asst Dfns Rtrn" );
			strcpy( strings[1], va( "%5d  %4d %4d ^3%4d^7 %2d:%04.1f %4d ^5%4d^7 %4d", statsInfo[1], statsInfo[3], statsInfo[4], statsInfo[12], min, sec,
				statsInfo[13], statsInfo[14], statsInfo[15], efficiency ) );
		} else if ( cgs.gametype == 5 ) {
			strcpy( strings[0], "^B^3Score  ^2Klls ^1Dths ^3Effcny ^5DmgScr  ^2WINS" );
			strcpy( strings[1], va( "%5d  %4d %4d %6.1f ^3%6d^5  %4d", statsInfo[1], statsInfo[3], statsInfo[4], efficiency, statsInfo[8] / 100,
				statsInfo[10] & cgs.stats_mask ) );
		} else {
			strcpy( strings[0], "^B^3Score  ^2Klls ^1Dths Sui  ^3Effcny" );
			strcpy( strings[1], va( "%5d  %4d %4d %3d  %6.1f", statsInfo[1], statsInfo[3], statsInfo[4], statsInfo[5], efficiency ) );
		}
		strcpy( strings[2], " " );
		strcpy( strings[3], "^5Weapon       ^3Accrcy ^7Hits/Atts ^2Klls ^1Dths ^3PkUp ^1Drop" );
		strcpy( strings[4], "^7-------------------------------------------------" );

		row = 5;
		argIdx = 23;
		for ( weapon = 1; weapon <= 9; weapon++ ) {
			if ( weaponMask & ( 1 << weapon ) ) {
				int		atts;
				int		hits;
				int		kills;
				int		deaths;

				hits = atoi( CG_Argv( argIdx++ ) );
				atts = atoi( CG_Argv( argIdx++ ) );
				kills = atoi( CG_Argv( argIdx++ ) );
				deaths = atoi( CG_Argv( argIdx++ ) );
				strcpy( strings[row], va( "%-12s: ", weaponNames[weapon] ) );
				if ( ( atts & cgs.stats_mask ) || ( hits & cgs.stats_mask ) ) {
					strcat( strings[row], va( "^3%5.1f ^7%4d/%-4d ", !( atts & cgs.stats_mask ) ? 0 : 100 * ( hits & cgs.stats_mask ) / (float)( atts & cgs.stats_mask ),
						hits & cgs.stats_mask, atts & cgs.stats_mask ) );
					damageActivity = qtrue;
				} else {
					strcat( strings[row], "                " );
					if ( kills || deaths ) {
						damageActivity = qtrue;
					}
				}
				if ( weapon > 2 ) {
					strcat( strings[row], va( "^2%4d ^1%4d ^3%4d ^1%4d", kills, deaths, atts >> cgs.stats_shift, hits >> cgs.stats_shift ) );
				} else {
					strcat( strings[row], va( "^2%4d ^1%4d", kills, deaths ) );
				}
				row++;
			}
		}

		if ( row == 5 ) {
			strcpy( strings[5], "^3No additional weapon info available." );
			strcpy( strings[6], " " );
			row += 2;
		} else if ( CG_OSPIsStatsHidden( qtrue, qtrue ) ) {
			strcpy( strings[row++], " " );
			strcpy( strings[row++], "^3Damage Given: ^7XX     ^2Armor : ^7XX" );
			strcpy( strings[row++], "^3Damage Recvd: ^7XX     ^2Health: ^7XX" );
		} else {
			char	armorStr[64];
			char	healthStr[64];

			armorStr[0] = 0;
			healthStr[0] = 0;
			if ( statsInfo[17] )
				strcpy( healthStr, va( "^2(^7%d ^5MH^2)", statsInfo[17] ) );
			if ( statsInfo[18] )
				strcpy( armorStr, va( "^2(^7%d ^2GA%s", statsInfo[18], ( statsInfo[19] || statsInfo[20] ) ? "^7" : "^2)" ) );
			if ( statsInfo[20] )
				strcat( armorStr, va( "%s%d ^3YA%s", !statsInfo[18] ? "^2(^7" : " ", statsInfo[20], statsInfo[19] ? "^7" : "^2)" ) );
			if ( statsInfo[19] )
				strcat( armorStr, va( "%s%d ^1RA^2)", ( !statsInfo[18] && !statsInfo[20] ) ? "^2(^7" : " ", statsInfo[19] ) );
			if ( damageActivity ) {
				strcpy( strings[row++], " " );
				strcpy( strings[row++], va( "^3Damage Given: ^7%-6d ^2Armor : ^7%d %s", statsInfo[8], statsInfo[10] >> cgs.stats_shift, armorStr ) );
				strcpy( strings[row++], va( "^3Damage Recvd: ^7%-6d ^2Health: ^7%d %s", statsInfo[9], statsInfo[11] >> cgs.stats_shift, healthStr ) );
				if ( cgs.gametype == GT_TEAM ) {
					strcpy( strings[row++], va( "^1Team Damage : ^7%d", statsInfo[7] ) );
				}
			} else {
				strcpy( strings[row++], " " );
				strcpy( strings[row++], va( "^2Armor Taken : ^7%d %s", statsInfo[10] >> cgs.stats_shift, armorStr ) );
				strcpy( strings[row++], va( "^2Health Taken: ^7%d %s", statsInfo[11] >> cgs.stats_shift, healthStr ) );
			}
		}

		scrollTime = cg.intermissionStarted ? 0.45f : cg_statScrollTime.value;
		y = cg.intermissionStarted ? 435.0f : 420.0f;
		wstatsWndId = CG_OSPDrawLeftSlidingWindow( scrollTime, scrollTime, cg.demoPlayback ? 4.0f : 9999.0f, 0, row, MAX_OSP_SLIDING_CHARS, 10, 10, strings[0], y, NULL,
			( statsInfo[2] == TEAM_RED ) ? colorRed : ( ( statsInfo[2] == TEAM_BLUE ) ? colorBlue : NULL ) ) + 1;
	}
}

// -wstats: lets the stats window slide out
void CG_OSPWStatsUp_f( void ) {
	if ( wstatsWndId > 0 ) {
		ospPrintContext[wstatsWndId - 1].hideBeforeRealtime = trap_Milliseconds() + ospPrintContext[wstatsWndId - 1].timeAppearance;
		ospPrintContext[wstatsWndId - 1].timeHiding = ospPrintContext[wstatsWndId - 1].hideBeforeRealtime;
		ospPrintContext[wstatsWndId - 1].hideBeforeCGTime = cg.time + ospPrintContext[wstatsWndId - 1].timeAppearance;
	}
	wstatsWndId = 0;
	wstatsEnabled = qfalse;
}

// decalinc: grows the edited decal (up to 800)
void CG_OSPDecalInc_f( void ) {
	if ( !cgs.decalEditing ) {
		return;
	}
	cgs.decals[cgs.decalSelected].size += ( cgs.decals[cgs.decalSelected].size < 800 );
}

// decaldec: shrinks the edited decal (down to 10)
void CG_OSPDecalDec_f( void ) {
	if ( !cgs.decalEditing ) {
		return;
	}
	cgs.decals[cgs.decalSelected].size -= ( cgs.decals[cgs.decalSelected].size > 10 );
}

// decalrotclock: turns the edited decal by 0.5 degrees
void CG_OSPDecalRotClock_f( void ) {
	if ( !cgs.decalEditing ) {
		return;
	}
	cgs.decals[cgs.decalSelected].rotation += 0.5f;
	if ( cgs.decals[cgs.decalSelected].rotation > 360.0f ) {
		cgs.decals[cgs.decalSelected].rotation -= 360.0f;
	}
}

// decalrotcounter: turns the edited decal back by 0.5 degrees
void CG_OSPDecalRotCounter_f( void ) {
	if ( !cgs.decalEditing ) {
		return;
	}
	cgs.decals[cgs.decalSelected].rotation -= 0.5f;
	if ( cgs.decals[cgs.decalSelected].rotation < 0.0f ) {
		cgs.decals[cgs.decalSelected].rotation += 360.0f;
	}
}

// decaledit: toggles decal editing (spectators only); the first time it loads the custom
// decal graphics (no-mip shaders "decal1".."decal17")
void CG_OSPDecalEdit_f( void ) {
	int		i;
	int		max;

	if ( !cgs.decalEditing ) {
		if ( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR ) {
			CG_Printf( "^3Cannot edit decals as a player!\n" );
			return;
		}
		if ( !cgs.decalOSPShaderCopyCount ) {
			max = 17;
			CG_Printf( "Locating custom decal graphics\n" );
			for ( i = 0; i < max; i++ ) {
				cgs.decalOSPShader[i] = trap_R_RegisterShaderNoMip( va( "decal%d", i + 1 ) );
				if ( !cgs.decalOSPShader[i] ) {
					break;
				}
				cgs.decalOSPShaderCopyCount++;
			}
			if ( !cgs.decalOSPShaderCopyCount ) {
				CG_Printf( "\n^3*** No custom decals found.  Decal editting disabled.\n\n" );
				return;
			}
			CG_Printf( "^3>>> Loaded %d custom decal graphics\n", cgs.decalOSPShaderCopyCount );
			cgs.numDecals = 0;
		}
		if ( !cgs.numDecals ) {
			cgs.numDecals = 1;
			cgs.decalSelected = 0;
		}
		cgs.decalEditing = qtrue;
		cgs.decals[cgs.decalSelected].enabled = 1;
		CG_Printf( "^3Decal editing ENABLED (%d labels, editing label %d)\n", cgs.numDecals, cgs.decalSelected );
	} else {
		cgs.decalEditing = qfalse;
		CG_Printf( "^3Decal editing DISABLED\n" );
	}
}

// decalnext: edits the next decal
void CG_OSPDecalNext_f( void ) {
	if ( cgs.decalEditing ) {
		cgs.decalSelected = ( cgs.decalSelected + 1 ) % cgs.numDecals;
	}
}

// decalprev: edits the previous decal
void CG_OSPDecalPrev_f( void ) {
	if ( cgs.decalEditing ) {
		cgs.decalSelected = ( cgs.decalSelected ? cgs.decalSelected : cgs.numDecals ) - 1;
	}
}

// decalgfxnext: next custom graphic for the edited decal
void CG_OSPDecalGFXNext_f( void ) {
	int		shader = cgs.decals[cgs.decalSelected].shader;

	if ( cgs.decalEditing ) {
		shader = ( shader + 1 ) % cgs.decalOSPShaderCopyCount;
		cgs.decals[cgs.decalSelected].shader = shader;
	}
}

// decalgfxprev: previous custom graphic for the edited decal
void CG_OSPDecalGFXPrev_f( void ) {
	int		shader = cgs.decals[cgs.decalSelected].shader;

	if ( cgs.decalEditing ) {
		shader = ( shader ? shader : cgs.decalOSPShaderCopyCount ) - 1;
		cgs.decals[cgs.decalSelected].shader = shader;
	}
}

// decaladd: adds a (disabled) decal, at most 32 (OSP bug, kept for compatibility: the "already defined" print
// has no argument for its %d)
void CG_OSPDecalAdd_f( void ) {
	if ( cgs.decalEditing ) {
		if ( cgs.numDecals >= MAX_OSP_DECALS ) {
			CG_Printf( "^3%d decals already defined.\n" );
		} else {
			CG_Printf( "^3Decal ^7%d^3 added (disabled).\n", cgs.numDecals );
			cgs.numDecals++;
		}
	}
}

// decaldisable: hides the edited decal
void CG_OSPDecalDisable_f( void ) {
	if ( cgs.decalEditing ) {
		cgs.decals[cgs.decalSelected].enabled = 0;
	}
}

// decalenable: shows the edited decal
void CG_OSPDecalEnable_f( void ) {
	if ( cgs.decalEditing ) {
		cgs.decals[cgs.decalSelected].enabled = 1;
	}
}

// decaldump: writes the enabled decals to decals/decal-<mapname>.txt (the format of the
// server's decal files)
void CG_OSPDecalDump_f( void ) {
	int				i;
	char			buf[256];
	int				count;
	fileHandle_t	f;
	char			*mapname;

	if ( !cgs.numDecals ) {
		CG_Printf( "^3decaldump: No decals defined!\n" );
		return;
	}
	mapname = Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" );
	if ( !mapname ) {
		CG_Printf( "^3decaldump: Couldn't locate mapname! -- aborted\n" );
		return;
	}
	count = trap_FS_FOpenFile( va( "decals/decal-%s.txt", mapname ), &f, FS_WRITE );
	for ( count = i = 0; i < cgs.numDecals; i++ ) {
		if ( cgs.decals[i].enabled ) {
			count++;
			Q_strncpyz( buf, va( "%d %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", cgs.decals[i].shader, cgs.decals[i].size, cgs.decals[i].angle,
				cgs.decals[i].origin[0], cgs.decals[i].origin[1], cgs.decals[i].origin[2], cgs.decals[i].dir[0], cgs.decals[i].dir[1], cgs.decals[i].dir[2] ), 256 );
			trap_FS_Write( buf, strlen( buf ), f );
		}
	}
	trap_FS_FCloseFile( f );
	CG_Printf( "^3%d decals defined.  Saved in decals/decal-%s.txt\n", count, mapname );
}

// a hex digit of an OSP ^X / ^x RRGGBB colour code
#define OSP_IS_HEX_DIGIT( c ) ( ( c ) && ( ( ( c ) >= '0' && ( c ) <= '9' ) || ( ( c ) >= 'A' && ( c ) <= 'F' ) || ( ( c ) >= 'a' && ( c ) <= 'f' ) ) )

// copies a player name for the stats tables without colour codes (^n, ^X / ^xRRGGBB) and
// unprintable characters; returns the length of the copy
int CG_OSPStatsNameCopy( const char *in, char *out, int outSize ) {
	const char	*s = in;
	char		*start = out;

	while ( *s && out - start < outSize - 1 ) {
		if ( *s == '^' ) {
			if ( ( s[1] == 'X' || s[1] == 'x' ) && OSP_IS_HEX_DIGIT( s[2] ) && OSP_IS_HEX_DIGIT( s[3] ) && OSP_IS_HEX_DIGIT( s[4] )
				&& OSP_IS_HEX_DIGIT( s[5] ) && OSP_IS_HEX_DIGIT( s[6] ) && OSP_IS_HEX_DIGIT( s[7] ) ) {
				s += 8;
			} else {
				s += !s[1] ? 1 : 2;
			}
		} else if ( *s < ' ' || *s > '~' ) {
			s++;
		} else {
			*out++ = *s++;
		}
	}
	*out = 0;
	return out - start;
}

// xstats1 (server command): prints a player's weapon accuracies, damage and pickups
void CG_OSPPrintXStats( void ) {
	int			i;
	int			idx = 1;
	int			weaponMask;
	char		name[MAX_NAME_LENGTH];
	qboolean	damageActivity = qfalse;
	int			clientNum;

	clientNum = atoi( CG_Argv( idx++ ) );
	weaponMask = atoi( CG_Argv( idx++ ) );
	// the client number comes from the server
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	Q_strncpyz( name, cgs.clientinfo[clientNum].name, sizeof( name ) );
	CG_OSPStatsNameCopy( cgs.clientinfo[clientNum].name, name, sizeof( name ) );
	CG_Printf( "%s", va( "\n^7Accuracy info for: ^3%s^7\n\n", name ) );
	CG_Printf( "Weapon        Accrcy Hits/Atts Kills Deaths Pickup Drops\n" );
	CG_Printf( "--------------------------------------------------------\n" );
	if ( !weaponMask ) {
		CG_Printf( "No weapon info available.\n" );
	} else {
		for ( i = 1; i <= 9; i++ ) {
			if ( weaponMask & ( 1 << i ) ) {
				int		atts;
				int		hits;
				int		kills;
				int		deaths;

				hits = atoi( CG_Argv( idx++ ) );
				atts = atoi( CG_Argv( idx++ ) );
				kills = atoi( CG_Argv( idx++ ) );
				deaths = atoi( CG_Argv( idx++ ) );
				CG_Printf( "%s", va( "^3%-12s: ", weaponNames[i] ) );
				if ( ( atts & cgs.stats_mask ) || ( hits & cgs.stats_mask ) ) {
					CG_Printf( "%s", va( "^7%6.1f ^5%4d/%-4d ", !( atts & cgs.stats_mask ) ? 0 : 100 * ( hits & cgs.stats_mask ) / (float)( atts & cgs.stats_mask ),
						hits & cgs.stats_mask, atts & cgs.stats_mask ) );
					damageActivity = qtrue;
				} else {
					CG_Printf( "                 " );
					if ( kills || deaths ) {
						damageActivity = qtrue;
					}
				}
				if ( i > 2 ) {
					CG_Printf( "%s", va( "^2%5d ^1%6d ^3%6d ^1%5d\n", kills, deaths, atts >> cgs.stats_shift, hits >> cgs.stats_shift ) );
				} else {
					CG_Printf( "%s", va( "^2%5d ^1%6d\n", kills, deaths ) );
				}
			}
		}
		if ( CG_OSPIsStatsHidden( qtrue, qtrue ) ) {
			CG_Printf( "\n^3Damage Given: ^7XX  ^2Armor Taken : ^7XX\n" );
			CG_Printf( "^3Damage Recvd: ^7XX  ^2Health Taken: ^7XX\n" );
		} else {
			// register: matches OSP's frame layout
			register int	ga;
			register int	ra;
			register int	ya;
			register int	mh;
			register int	armorTaken;
			register int	healthTaken;
			register int	damageGiven;
			register int	damageRcvd;
			char	armorStr[64];
			char	healthStr[64];

			damageGiven = atoi( CG_Argv( idx + 2 ) );
			damageRcvd = atoi( CG_Argv( idx + 3 ) );
			armorTaken = atoi( CG_Argv( idx ) );
			healthTaken = atoi( CG_Argv( idx + 1 ) );
			mh = atoi( CG_Argv( idx + 4 ) );
			ga = atoi( CG_Argv( idx + 5 ) );
			ra = atoi( CG_Argv( idx + 6 ) );
			ya = atoi( CG_Argv( idx + 7 ) );
			armorStr[0] = 0;
			healthStr[0] = 0;
			if ( mh )
				strcpy( healthStr, va( "^2(^7%d ^5MH^2)", mh ) );
			if ( ga )
				strcpy( armorStr, va( "^2(^7%d ^3GA%s", ga, ( ra || ya ) ? "^7," : "^2)" ) );
			if ( ya )
				strcat( armorStr, va( "%s%d ^3YA%s", !ga ? "^2(^7" : " ", ya, ra ? "^7," : "^2)" ) );
			if ( ra )
				strcat( armorStr, va( "%s%d ^1RA^2)", ( !ya && !ga ) ? "^2(^7" : " ", ra ) );
			if ( damageActivity ) {
				CG_Printf( "\n^3Damage Given: ^7%-6d ^2Armor Taken : ^7%d %s\n", damageGiven, armorTaken, armorStr );
				CG_Printf( "^3Damage Recvd: ^7%-6d ^2Health Taken: ^7%d %s\n", damageRcvd, healthTaken, healthStr );
			} else {
				CG_Printf( "\n^2Armor Taken : ^7%d %s\n", armorTaken, armorStr );
				CG_Printf( "^2Health Taken: ^7%d %s\n\n", healthTaken, healthStr );
			}
		}
	}
}

// sset (server command): prints the server settings, four "name value" pairs per line
void CG_OSPPrintSSet( void ) {
	int		i;
	char	buf[512];
	char	*prefix;
	int		k;

	for ( k = 0, buf[0] = 0, prefix = "", i = 1; i < trap_Argc(); i += 2 ) {
		// the arguments come from the server: bounded appends, and no more than one
		// setting name / value per column
		Q_strcat( buf, sizeof( buf ), va( "%s%20.20s ", prefix, CG_Argv( i ) ) );
		Q_strcat( buf, sizeof( buf ), va( "%-3.32s", CG_Argv( i + 1 ) ) );
		prefix = "  ";
		if ( ++k == 3 ) {
			CG_Printf( "%s\n", buf );
			k = 0;
			prefix = "";
			buf[0] = 0;
		}
	}
	if ( buf[0] ) {
		CG_Printf( "%s\n", buf );
	}
}

// astats / astatsb (server commands): the players' accuracies with one weapon; the best
// (top) or worst values are highlighted
void CG_OSPAstats( qboolean top ) {
	int		idx = 1;
	int		i;
	int		best;
	int		count;

	count = atoi( CG_Argv( idx++ ) );
	best = atoi( CG_Argv( idx++ ) );
	// the count comes from the server: no more entries than players, nor than arguments
	if ( count <= 0 || count > MAX_CLIENTS || idx + count * 5 > trap_Argc() ) {
		return;
	}
	CG_Printf( "\n^3  Acc Hits/Atts Kills Deaths\n" );
	CG_Printf( "----------------------------------------------------------\n" );
	for ( i = 0; i < count; i++ ) {
		int		atts;
		float	acc;
		int		hits;
		char	*color;
		char	name[MAX_NAME_LENGTH];
		int		clientNum;
		int		kills;
		int		deaths;

		clientNum = atoi( CG_Argv( idx++ ) );
		hits = atoi( CG_Argv( idx++ ) ) & cgs.stats_mask;
		atts = atoi( CG_Argv( idx++ ) ) & cgs.stats_mask;
		kills = atoi( CG_Argv( idx++ ) );
		deaths = atoi( CG_Argv( idx++ ) );
		if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
			continue;
		}
		acc = ( atts > 0 ) ? 100 * hits / (float)atts : 0;
		color = ( ( top ? acc : best + 0.999f ) >= ( top ? best : acc ) ) ? "^3" : "^7";
		CG_OSPStatsNameCopy( cgs.clientinfo[clientNum].name, name, 30 );
		CG_Printf( "%s", va( "%s%5.1f ^5%4d/%-4d ^2%5d ^1%6d %s%s\n", color, acc, hits, atts, kills, deaths, color, name ) );
	}
	CG_Printf( "\n\n" );
}

// bstats / bstatsb (server commands): the best (top) or worst accuracy of every weapon
void CG_OSPBstats( qboolean top ) {
	int		idx = 1;
	int		weapon;

	weapon = atoi( CG_Argv( idx++ ) );
	if ( !weapon ) {
		CG_Printf( "\n^3No qualifying %sshot info available.\n\n", top ? "top" : "bottom" );
		return;
	}
	CG_Printf( "\n^2%sst Match Accuracies:\n", top ? "Be" : "Wor" );
	CG_Printf( "^3WP   Acc Hits/Atts Kills Deaths\n" );
	CG_Printf( "-------------------------------------------------------------\n" );
	while ( weapon ) {
		int		atts;
		int		hits;
		char	name[MAX_NAME_LENGTH];
		int		clientNum;
		int		kills;
		int		deaths;
		float	acc;

		clientNum = atoi( CG_Argv( idx++ ) );
		hits = atoi( CG_Argv( idx++ ) ) & cgs.stats_mask;
		atts = atoi( CG_Argv( idx++ ) ) & cgs.stats_mask;
		kills = atoi( CG_Argv( idx++ ) );
		deaths = atoi( CG_Argv( idx++ ) );
		// weapon and client number come from the server
		if ( weapon < 0 || weapon >= 10 || clientNum < 0 || clientNum >= MAX_CLIENTS || idx > trap_Argc() ) {
			break;
		}
		acc = ( atts > 0 ) ? 100 * hits / (float)atts : 0;
		CG_OSPStatsNameCopy( cgs.clientinfo[clientNum].name, name, 30 );
		CG_Printf( "%s", va( "^3%s ^7%5.1f ^5%4d/%-4d ^2%5d ^1%6d ^7%s\n", weaponShortNames[weapon], acc, hits, atts, kills, deaths, name ) );
		weapon = atoi( CG_Argv( idx++ ) );
	}
	CG_Printf( "\n\n" );
}

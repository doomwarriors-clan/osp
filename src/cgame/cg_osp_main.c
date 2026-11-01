/*
 * cg_osp_main.c: the +modifN / -modifN key bindings, client font sizes (cf_* cvars), cvarTable
 * helpers, CTF location colours, the multiview-aware sound helpers, user info, MoTD,
 * multiview (viewlist, window drawing), map decals and decal editor info, stats hiding,
 * restricted renderer cvars.
 */
#include "cg_local.h"

// OSP: CG_OSPGetCvarName, CG_OSPGetCvarDefault, CG_OSPCvarSet and
// CG_OSPCvarReset step through cg_main.c's cvarTable with id's 16-byte entry, although the
// table has OSP's 20-byte entries: this file saw the old declaration.
typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
} cvarTable_t;

extern cvarTable_t	cvarTable[];
extern int			cvarTableSize;

// OSP: the bss of this file is laid out in reverse
// order of declaration, so the global at its top is declared first.
// 272 unreferenced
// bytes, the size of a vmCvar_t: a cvar that is declared but never registered.
vmCvar_t	ospUnusedCvar;

// client font sizes "<w>x<h>" (or "<w>*<h>") of the cf_* cvars, parsed when the cvar changes
typedef struct {
	vmCvar_t	*cvar;
	int			modificationCount;
	int			w;
	int			h;
} ospClientFont_t;

ospClientFont_t ospClientFonts[] = {
	{ &cf_Statusbar, -1, 0, 0 },
	{ &cf_Pickupstring, -1, 0, 0 },
	{ &cf_Teamchat, -1, 0, 0 },
	{ &cf_Teamoverlay, -1, 0, 0 },
	{ &cf_CrosshairNames, -1, 0, 0 },
	{ &cf_TeamCrosshairHealth, -1, 0, 0 },
	{ &cf_Vote, -1, 0, 0 },
	{ &cf_Following, -1, 0, 0 },
	{ &cf_AmmoStatusbar, -1, 0, 0 },
	{ &cf_Fragmsg, -1, 0, 0 },
	{ &cf_Scores, -1, 0, 0 },
	{ &cf_WeaponSwitch, -1, 0, 0 },
	{ &cf_WeaponName, -1, 0, 0 }
};

int		ospClientFontsSize = sizeof( ospClientFonts ) / sizeof( ospClientFonts[0] );

// OSP: an unreferenced int 0 of this file
int		ospUnusedInt = 0;

// +modifN / -modifN: per binding (N = 1..5) a command for the key press and one for the
// release ("+modif1 1" runs it, "+modif1 0 <command>" sets it, "+modif1 0 ?" shows it)
int		modifState[5];			// key held
char	modifStrings[10][256];	// [2 * N] press command, [2 * N + 1] release command

/*
CG_OSPModifDown: +modifN: run the press command once per press, or set / show it
*/
void CG_OSPModifDown( int layout ) {
	int i = layout * 2;

	if ( !modifState[layout] && ( trap_Argc() < 2 || ( trap_Argc() >= 2 && atoi( CG_Argv( 1 ) ) ) ) ) {
		if ( modifStrings[i][0] ) {
			trap_SendConsoleCommand( modifStrings[i] );
		} else {
			CG_Printf( "^3*** %s not set.\n", CG_Argv( 0 ) );
		}
		modifState[layout] = qtrue;
	} else if ( trap_Argc() >= 2 && !atoi( CG_Argv( 1 ) ) ) {
		if ( !Q_stricmp( CG_Argv( 1 ), "?" ) ) {
			if ( modifStrings[i][0] ) {
				CG_Printf( "^5%s is set to: ^7%s\n", CG_Argv( 0 ), modifStrings[i] );
			} else {
				CG_Printf( "^3*** %s not set.\n", CG_Argv( 0 ) );
			}
		} else {
			trap_Args( modifStrings[i], 256 );
		}
	}
}

/*
CG_OSPModifUp: -modifN: run the release command once per release, or set / show it
*/
void CG_OSPModifUp( int layout ) {
	int i = layout * 2 + 1;

	if ( modifState[layout] && ( trap_Argc() < 2 || ( trap_Argc() >= 2 && atoi( CG_Argv( 1 ) ) ) ) ) {
		if ( modifStrings[i][0] ) {
			trap_SendConsoleCommand( modifStrings[i] );
		} else {
			CG_Printf( "^3*** %s not set.\n", CG_Argv( 0 ) );
		}
		modifState[layout] = qfalse;
	} else if ( trap_Argc() >= 2 ) {
		if ( !Q_stricmp( CG_Argv( 1 ), "?" ) ) {
			if ( modifStrings[i][0] ) {
				CG_Printf( "^5%s is set to: ^7%s\n", CG_Argv( 0 ), modifStrings[i] );
			} else {
				CG_Printf( "^3*** %s not set.\n", CG_Argv( 0 ) );
			}
		} else if ( trap_Argc() >= 2 && !atoi( CG_Argv( 1 ) ) ) {
			trap_Args( modifStrings[i], 256 );
		}
	}
}

// +modif1 .. -modif5
void CG_OSPModif1Down_f( void ) {
	CG_OSPModifDown( 0 );
}

void CG_OSPModif1Up_f( void ) {
	CG_OSPModifUp( 0 );
}

void CG_OSPModif2Down_f( void ) {
	CG_OSPModifDown( 1 );
}

void CG_OSPModif2Up_f( void ) {
	CG_OSPModifUp( 1 );
}

void CG_OSPModif3Down_f( void ) {
	CG_OSPModifDown( 2 );
}

void CG_OSPModif3Up_f( void ) {
	CG_OSPModifUp( 2 );
}

void CG_OSPModif4Down_f( void ) {
	CG_OSPModifDown( 3 );
}

void CG_OSPModif4Up_f( void ) {
	CG_OSPModifUp( 3 );
}

void CG_OSPModif5Down_f( void ) {
	CG_OSPModifDown( 4 );
}

void CG_OSPModif5Up_f( void ) {
	CG_OSPModifUp( 4 );
}

/*
CG_OSPGetClientFontSize: the character size of a cf_* cvar ("<w>x<h>"); 16x16 and qfalse when
the cvar is not a font cvar or its value is malformed
*/
qboolean CG_OSPGetClientFontSize( const vmCvar_t *cvar, int *w, int *h ) {
	const char *p;
	int fw, fh;
	int i;

	for ( i = 0; i < ospClientFontsSize; i++ ) {
		if ( ospClientFonts[i].cvar == cvar ) {
			if ( ospClientFonts[i].modificationCount != cvar->modificationCount ) {
				fw = fh = 0;
				p = cvar->string;
				while ( *p >= '0' && *p <= '9' ) {
					fw = fw * 10 + ( *p - '0' );
					p++;
				}
				if ( *p != 'x' && *p != '*' ) {
					break;
				}
				p++;
				while ( *p >= '0' && *p <= '9' ) {
					fh = fh * 10 + ( *p - '0' );
					p++;
				}
				if ( fw == 0 || fh == 0 ) {
					break;
				}
				*w = ospClientFonts[i].w = fw;
				*h = ospClientFonts[i].h = fh;
				ospClientFonts[i].modificationCount = ospClientFonts[i].cvar->modificationCount;
			} else {
				*w = ospClientFonts[i].w;
				*h = ospClientFonts[i].h;
			}
			return qtrue;
		}
	}
	*w = 16;
	*h = 16;
	return qfalse;
}

/*
CG_OSPGetCvarName: the name of a registered cvar
*/
char *CG_OSPGetCvarName( vmCvar_t *cvar ) {
	cvarTable_t *cv;
	int i;

	for ( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar == cvar ) {
			return cv->cvarName;
		}
	}
	return NULL;
}

/*
CG_OSPGetCvarDefault: the default value of a registered cvar
*/
char *CG_OSPGetCvarDefault( vmCvar_t *cvar ) {
	cvarTable_t *cv;
	int i;

	for ( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar == cvar ) {
			return cv->defaultString;
		}
	}
	return NULL;
}

/*
CG_OSPCvarSet: set a registered cvar by its vmCvar_t
*/
void CG_OSPCvarSet( vmCvar_t *cvar, const char *value ) {
	cvarTable_t *cv;
	int i;

	for ( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar == cvar ) {
			trap_Cvar_Set( cv->cvarName, value );
			return;
		}
	}
	CG_Printf( "^1couldn't find cvar\n" );
}

/*
CG_OSPCvarReset: reset a registered cvar to its default value
*/
void CG_OSPCvarReset( vmCvar_t *cvar ) {
	cvarTable_t *cv;
	int i;

	for ( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar == cvar ) {
			trap_Cvar_Set( cv->cvarName, cv->defaultString );
			return;
		}
	}
	CG_Printf( "^1couldn't find cvar\n" );
}

// the location names (CS_LOCATIONS), in CTF with "blue ..." / "red ..." turned into colours
char	ctfLocations[MAX_LOCATIONS][17];

/*
CG_GetCTFLocation: the (coloured) name of a location
*/
const char *CG_GetCTFLocation( int loc ) {
	return ctfLocations[loc];
}

/*
CG_InitCTFLocations: copy the location configstrings; in CTF "blue <name>" becomes "^5<name>"
and "red <name>" "^1<name>"
*/
void CG_InitCTFLocations( void ) {
	char *p;
	int i;
	char prefix[5];

	for ( i = 1; i < MAX_LOCATIONS; i++ ) {
		Q_strncpyz( ctfLocations[i], CG_ConfigString( CS_LOCATIONS + i ), 17 );
		if ( cgs.gametype == GT_CTF ) {
			p = ctfLocations[i];
			Q_strncpyz( prefix, ctfLocations[i], 5 );
			prefix[4] = 0;
			if ( !Q_stricmp( "blue", prefix ) ) {
				*p++ = '^';
				*p++ = '5';
				while ( p[3] ) {
					*p = p[3];
					p++;
				}
				*p = 0;
			} else {
				prefix[3] = 0;
				if ( !Q_stricmp( "red", prefix ) ) {
					*p++ = '^';
					*p++ = '1';
					while ( p[2] ) {
						*p = p[2];
						p++;
					}
					*p = 0;
				}
			}
		}
	}
}

/*
CG_OSPDynamicMem_f: cg_dynamicmem: print the dynamic memory statistics
*/
void CG_OSPDynamicMem_f( void ) {
	malloc_stats();
}

/*
CG_OSPEventSound: play a sound for the viewed client (the current multiview window's client)
*/
void CG_OSPEventSound( const char *name ) {
	if ( cg.mvActive ) {
		int saved = cg.osp_unk_1e964;

		cg.osp_unk_1e964 = 0;
		trap_S_StartSound( NULL, cg.mvClientNums[cg.mvCurrent], CHAN_AUTO, trap_S_RegisterSound( name, qfalse ) );
		cg.osp_unk_1e964 = saved;
	} else {
		trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, trap_S_RegisterSound( name, qfalse ) );
	}
}

/*
CG_OSPEventCustomSound: EV_GENERAL_SOUND for the viewed client: a game sound, or a client's
custom sound ("*...")
*/
void CG_OSPEventCustomSound( entityState_t *es, int clientNum ) {
	if ( cgs.gameSounds[es->eventParm] ) {
		if ( cg.mvActive ) {
			int saved = cg.osp_unk_1e964;

			cg.osp_unk_1e964 = 0;
			trap_S_StartSound( NULL, cg.mvClientNums[cg.mvCurrent], CHAN_AUTO, cgs.gameSounds[es->eventParm] );
			cg.osp_unk_1e964 = saved;
		} else {
			trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[es->eventParm] );
		}
	} else {
		const char *s = CG_ConfigString( CS_SOUNDS + es->eventParm );

		if ( cg.mvActive ) {
			int saved = cg.osp_unk_1e964;

			cg.osp_unk_1e964 = 0;
			trap_S_StartSound( NULL, cg.mvClientNums[cg.mvCurrent], CHAN_AUTO, CG_CustomSound( clientNum, s ) );
			cg.osp_unk_1e964 = saved;
		} else {
			trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( clientNum, s ) );
		}
	}
}

/*
CG_OSPPlaySound: play an item sound on the entity, or for the viewed client
*/
void CG_OSPPlaySound( entityState_t *es, int entityNum, sfxHandle_t sfx ) {
	if ( cg.mvActive ) {
		int saved = cg.osp_unk_1e964;

		cg.osp_unk_1e964 = 0;
		trap_S_StartSound( NULL, cg.mvClientNums[cg.mvCurrent], CHAN_ITEM, sfx );
		cg.osp_unk_1e964 = saved;
	} else if ( es ) {
		trap_S_StartSound( NULL, entityNum, CHAN_ITEM, sfx );
	} else {
		trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_ITEM, sfx );
	}
}

/*
CG_OSPPlayGameSound: play a game sound (CS_SOUNDS index) for the viewed client (unused)
*/
void CG_OSPPlayGameSound( int index ) {
	if ( cg.mvActive ) {
		int saved = cg.osp_unk_1e964;

		cg.osp_unk_1e964 = 0;
		trap_S_StartSound( NULL, cg.mvClientNums[cg.mvCurrent], CHAN_AUTO, cgs.gameSounds[index] );
		cg.osp_unk_1e964 = saved;
	} else {
		trap_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[index] );
	}
}

/*
CG_OSPNormalizeText: copy at most size characters of a text without colour codes and newlines
*/
void CG_OSPNormalizeText( char *src, int size, char *dst ) {
	int i = 0;

	while ( *src && i < size ) {
		if ( *src == Q_COLOR_ESCAPE ) {
			src += 2;
			i += 2;
		} else if ( *src == '\n' ) {
			src++;
			i++;
		} else {
			*dst++ = *src++;
			i++;
		}
	}
	*dst = 0;
}

/*
an unreferenced empty function
*/
void CG_OSPStub709( void ) {
}

/*
CG_OSPUpdateUserInfo: send the client settings to the server ("uinfo", or "uvinfo" to have
them printed)
*/
void CG_OSPUpdateUserInfo( qboolean arg ) {
	int flags;
	char *cmd;

	if ( cg.demoPlayback ) {
		return;
	}
	cgs.nextCvarsUpdateTime = 0;
	flags = 0;
	if ( !arg ) {
		cmd = "uinfo";
	} else {
		cmd = "uvinfo";
	}
	if ( cg_followpowerup.integer ) {
		flags |= 1;
	}
	if ( cg_followkiller.integer ) {
		flags |= 2;
	}
	if ( cg_followviewcam.integer ) {
		flags |= 4;
	}
	trap_SendClientCommand( va( "%s %d %d %d %d %d %d %d\n", cmd, cl_maxpackets.integer, snaps.integer,
		cl_timenudge.integer, cg.customLocationsEnabled, flags, cg_autoAction.integer,
		cg_useScreenShotJPEG.integer ? 1 : 0 ) );
}

/*
CG_OSPMoTD_f: motd: show the server's message of the day in a sliding window
*/
void CG_OSPMoTD_f( void ) {
	if ( cgs.numberOfStringsMotd > 0 ) {
		CG_OSPDrawLeftSlidingWindow( 0.5f, 1.2f, 12.0f, 1.0f, cgs.numberOfStringsMotd, 64, 10, 10, cgs.motd[0], 400, NULL, NULL );
	}
}

/*
CG_OSPParseViewList: "viewlist <count> <clientNum>...": the multiview windows
*/
void CG_OSPParseViewList( void ) {
	int i;
	int oldCount = cg.mvActive;

	// count and client numbers come from the server: clamp the count to the window table
	// (and to what the command carries) and reject client numbers outside the client table
	cg.mvActive = atoi( CG_Argv( 1 ) );
	if ( cg.mvActive < 0 || cg.mvActive > sizeof( cg.mvClientNums ) / sizeof( cg.mvClientNums[0] ) || cg.mvActive + 2 > trap_Argc() ) {
		cg.mvActive = 0;
	}
	for ( i = 0; i < cg.mvActive; i++ ) {
		cg.mvClientNums[i] = atoi( CG_Argv( i + 2 ) );
		if ( cg.mvClientNums[i] < 0 || cg.mvClientNums[i] >= MAX_CLIENTS ) {
			cg.mvActive = 0;
			break;
		}
	}
	CG_RegisterItemVisuals( 6 );
	if ( cg.mvActive == 0 || cg.mvCurrent < 0 || cg.mvCurrent >= cg.mvActive ) {
		cg.mvCurrent = 0;
	}
	cg.osp_unk_1e964 = 0;
	if ( oldCount != cg.mvActive ) {
		cg.mvCursorTime = cg.time + 5000;
	}
}

void CG_OSPDrawMultiviewClient( int window, int clientNum );

/*
CG_OSPDrawMultiview: draw every multiview window, the current one last
*/
void CG_OSPDrawMultiview( void ) {
	int i;
	int clientNum;
	int curWindow = 0;
	int curClient = 0;

	for ( i = 0; i < cg.mvActive; i++ ) {
		clientNum = cg.mvClientNums[i];
		if ( i == cg.mvCurrent ) {
			curWindow = i;
			curClient = clientNum;
		} else {
			CG_OSPDrawMultiviewClient( i, clientNum );
		}
	}
	if ( cg.mvActive ) {
		CG_OSPDrawMultiviewClient( curWindow, curClient );
	}
}

void CG_OSPDrawMultiviewWindow( centity_t *cent, int clientNum, int window );

/*
CG_OSPDrawMultiviewClient: draw the window of a client: find its player entity and draw it
with the snapshot's client number, team and weapon set to that client
*/
void CG_OSPDrawMultiviewClient( int window, int clientNum ) {
	int i;
	int savedClientNum;
	int savedTeam;
	int savedWeapon;

	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( cg_entities[i].currentState.clientNum == clientNum && cg_entities[i].currentState.eType == ET_PLAYER ) {
			savedClientNum = cg.snap->ps.clientNum;
			savedTeam = cg.snap->ps.persistant[PERS_TEAM];
			savedWeapon = cg.snap->ps.weapon;
			cg.snap->ps.clientNum = clientNum;
			cg.snap->ps.persistant[PERS_TEAM] = cgs.clientinfo[clientNum].team;
			cg.snap->ps.weapon = cgs.clientinfo[clientNum].curWeapon;
			CG_OSPDrawMultiviewWindow( &cg_entities[i], i, window + 1 );
			cg.snap->ps.clientNum = savedClientNum;
			cg.snap->ps.persistant[PERS_TEAM] = savedTeam;
			cg.snap->ps.weapon = savedWeapon;
			return;
		}
	}
}

// cg_predict.c's cg_pmove (global in OSP): the multiview cursor follows the view angles
extern pmove_t	cg_pmove;

/*
CG_OSPDrawMultiviewWindow: draw one multiview window (1-based): the window rectangle (2 windows
one above the other, more in two columns), the scene from the player's eyes, the frame of the
current window, the cursor (moved with the mouse: clicking a window sends "viewframe"), and
the player's info box (name and score, powerups, weapon and ammo, armor, ping, health)
*/
void CG_OSPDrawMultiviewWindow( centity_t *cent, int clientNum, int window ) {
	int			i;
	clientInfo_t	*ci;
	int			ch;
	int			n;
	int			count;
	refdef_t	refdef;
	int			cw;
	float		bx;
	float		by;
	vec4_t		color;
	int			isCurrent;
	float		fov_x;
	float		x;
	float		y;
	float		h;
	float		zoomFov;
	float		w;
	int			ping;
	int			mx, my;
	int			health;
	int			armor;
	float		f;
	int			fontW, fontH;
	int			pingColor;
	int			score;
	char		name[MAX_QPATH];
	float		fx;
	float		fov_y;

	isCurrent = qfalse;
	mx = 0;
	my = 0;
	if ( window - 1 == cg.mvCurrent ) {
		if ( cg.mvActive > 1 ) {
			isCurrent = qtrue;
		}
		cg.osp_unk_1e964 = 0;
	} else {
		cg.osp_unk_1e964 = 0;
	}

	if ( cg.mvActive == 2 ) {
		x = isCurrent * 2;
		y = isCurrent * 2 + ( window - 1 ) * 480 / 2;
		w = 640 - isCurrent * 4;
		h = ( 480 - isCurrent * 4 ) / 2;
	} else {
		x = isCurrent * 2 + 640 * ( 1 - window % 2 ) / 2;
		if ( window % 2 && window == cg.mvActive ) {
			w = 640 - isCurrent * 4;
		} else {
			w = ( 640 - isCurrent * 4 ) / 2;
		}
		h = ( 480 - isCurrent * 4 ) / ( cg.mvActive / 2 + cg.mvActive % 2 );
		n = window / 2 + window % 2;
		y = isCurrent * 2 + ( n - 1 ) * h;
	}
	bx = x + 5;
	by = y + h - 10;
	ci = &cgs.clientinfo[clientNum];

	memset( &refdef, 0, sizeof( refdef ) );
	memcpy( refdef.areamask, cg.snap->areamask, sizeof( cg.refdef.areamask ) );

	if ( isCurrent ) {
		CG_DrawPic( x - 2, y - 2, w + 4, h + 4, cgs.media.selectShader );
	}

	if ( cg.mvActive > 1 ) {
		mx = 640 * ( 65536 - cg_pmove.cmd.angles[YAW] ) / 65536;
		if ( m_pitch.value < 0 ) {
			my = 480 * ( 65536 - cg_pmove.cmd.angles[PITCH] ) / 65536;
		} else {
			my = 480 * cg_pmove.cmd.angles[PITCH] / 65536;
		}
		if ( mx != cg.mvCursorX || my != cg.mvCursorY ) {
			cg.mvCursorTime = cg.time + 5000;
			cg.mvCursorX = mx;
			cg.mvCursorY = my;
		}
		if ( mx >= x && mx < x + w && my >= y && my < y + h && !isCurrent && cg.mvViewframeTime < cg.time ) {
			trap_SendClientCommand( va( "viewframe %d\n", window - 1 ) );
			cg.mvViewframeTime = cg.time + 500;
		}
	}

	CG_AdjustFrom640( &x, &y, &w, &h );
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	// CG_CalcFov
	fov_x = cg_fov.value;
	if ( fov_x < 1 ) {
		fov_x = 1;
	} else if ( fov_x > 120 ) {
		fov_x = 120;
	}
	zoomFov = cg_zoomFov.value;
	if ( zoomFov < 1 ) {
		zoomFov = 1;
	} else if ( zoomFov > 120 ) {
		zoomFov = 120;
	}
	if ( cg.zoomed ) {
		f = ( cg.time - cg.zoomTime ) / (float)ZOOM_TIME;
		if ( f > 1.0 ) {
			fov_x = zoomFov;
		} else {
			fov_x = fov_x + f * ( zoomFov - fov_x );
		}
	} else {
		f = ( cg.time - cg.zoomTime ) / (float)ZOOM_TIME;
		if ( f > 1.0 ) {
			fov_x = fov_x;
		} else {
			fov_x = zoomFov + f * ( fov_x - zoomFov );
		}
	}
	refdef.fov_x = fov_x;
	fx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
	fov_y = atan2( refdef.height, fx ) * 360 / M_PI;
	refdef.fov_y = fov_y;

	refdef.rdflags = cg.refdef.rdflags;
	refdef.time = cg.time;
	VectorCopy( cent->lerpOrigin, refdef.vieworg );
	refdef.vieworg[2] += DEFAULT_VIEWHEIGHT;
	trap_R_ClearScene();
	AnglesToAxis( cent->lerpAngles, refdef.viewaxis );
	if ( !cg.hyperspace ) {
		CG_AddPacketEntities();
		CG_AddMarks();
		CG_AddParticles();
		CG_AddLocalEntities();
	}
	if ( window - 1 == cg.mvCurrent ) {
		trap_S_Respatialize( cg.snap->ps.clientNum, refdef.vieworg, refdef.viewaxis, 0 );
	}
	trap_R_RenderScene( &refdef );

	// the info box
	CG_OSPGetClientFontSize( &cf_TeamCrosshairHealth, &fontW, &fontH );
	ch = 3 * fontH / 4;
	cw = 3 * fontW / 4;
	color[3] = 0.7f;
	if ( ci->team == TEAM_RED ) {
		color[0] = 1;
		color[1] = 0;
		color[2] = 0;
		score = cgs.scores1;
	} else if ( ci->team == TEAM_BLUE ) {
		color[0] = 0;
		color[1] = 0;
		color[2] = 1;
		score = cgs.scores2;
	} else {
		color[0] = 0.5f;
		color[1] = 0.5f;
		color[2] = 0.5f;
		score = 0;
	}

	count = 0;
	n = 0;
	for ( i = 0; i < PW_NUM_POWERUPS; i++ ) {
		if ( ci->powerups & ( 1 << i ) ) {
			count++;
		}
	}
	CG_OSPNormalizeText( ci->name, strlen( ci->name ), name );
	if ( cgs.gametype != GT_TEAM ) {
		n = count - 2 + strlen( va( "%s ^3(%d)", name, cgs.clientinfo[cg.snap->ps.clientNum].score ) );
	} else {
		n = count - 6 + strlen( va( "%s ^3(^7%d^3/%d)", name, cgs.clientinfo[cg.snap->ps.clientNum].score, score ) );
	}
	CG_OSPSetColor( color );
	CG_OSPDrawPic( bx - 3, by - 3, cw * n + 4, ch + 2, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );

	// powerups, name and score
	count = 0;
	n = 0;
	for ( i = 0; i < PW_NUM_POWERUPS; i++ ) {
		if ( ci->powerups & ( 1 << i ) ) {
			gitem_t	*item = BG_FindItemForPowerup( i );

			if ( !item ) {
				continue;
			}
			CG_OSPDrawPic( bx + count, by - ( n * ch + 2 ), cw, ch, trap_R_RegisterShader( item->icon ) );
			count += 10;
		}
	}
	if ( cgs.gametype != GT_TEAM ) {
		CG_OSPDrawString( bx + count, by - ( n * ch + 2 ), va( "%s ^3(%d)", ci->name, cgs.clientinfo[cg.snap->ps.clientNum].score ), cw, ch, NULL, 0, 0 );
	} else {
		CG_OSPDrawString( bx + count, by - ( n * ch + 2 ), va( "%s ^3(^7%d^3/%d)", ci->name, cgs.clientinfo[cg.snap->ps.clientNum].score, score ), cw, ch, NULL, 0, 0 );
	}
	n++;

	// weapon and ammo, armor, ping, health
	CG_OSPSetColor( color );
	CG_OSPDrawPic( bx - 3, by - ( 3 * ch + 4 ), 4 * cw + 6, 3 * ch + 1, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );
	if ( ci->curWeapon < MAX_WEAPONS && cg_weapons[ci->curWeapon].weaponIcon ) {
		CG_OSPDrawPic( bx, by - ( n * ch + 2 ), cw, ch, cg_weapons[ci->curWeapon].weaponIcon );
	} else {
		CG_OSPDrawPic( bx, by - ( n * ch + 2 ), cw, ch, cgs.media.deferShader );
	}
	CG_OSPDrawString( bx + 10, by - ( n * ch + 2 ), va( "%d", ci->ammo ), cw, ch, NULL, 0, 0 );
	n++;

	armor = ( ci->armor < 0 || ci->armor > 300 ) ? 0 : ci->armor;
	health = ( ci->health < 0 || ci->health > 300 ) ? 0 : ci->health;
	ping = ci->teamTask < 0 ? 0 : ( ci->teamTask > 999 ? 999 : ci->teamTask );
	if ( ping < 80 ) {
		i = 0;
		pingColor = 7;
	} else if ( ping < 120 ) {
		i = 1;
		pingColor = 2;
	} else if ( ping < 180 ) {
		i = 2;
		pingColor = 3;
	} else if ( ping < 400 ) {
		i = 3;
		pingColor = 6;
	} else {
		i = 4;
		pingColor = 1;
	}
	CG_GetColorForHealth( health, armor, color );
	CG_OSPDrawPic( bx, by - ( n * ch + 2 ), cw, ch, ( &cgs.media.armorIconGreen )[cg.snap->ps.stats[STAT_ARMOR_TYPE]] );
	CG_OSPDrawString( bx + 10, by - ( n * ch + 2 ), va( "%d", armor ), cw, ch, color, 0, 0 );
	CG_OSPDrawString( bx + 50, by - ( n * ch + 2 ), va( "^%dPing: %d", pingColor, ping ), cw + i, ch + i, colorWhite, 0, 0 );
	trap_R_SetColor( NULL );
	n++;
	CG_OSPDrawPic( bx, by - ( n * ch + 2 ), cw, ch, cg_items[6].icon );
	CG_OSPDrawString( bx + 10, by - ( n * ch + 2 ), va( "%d", health ), cw, ch, color, 0, 0 );
	trap_R_SetColor( NULL );

	if ( cg.mvActive > 1 && cg.mvCursorTime > cg.time ) {
		CG_DrawPic( mx - 16, my - 16, 32, 32, cgs.media.cursor );
	}
}

/*
CG_OSPAddDecals: add the map decals to the scene (spectators, warmup and intermission only);
in decaledit mode first move the selected decal to the surface in the view centre
*/
void CG_OSPAddDecals( void ) {
	int i;
	vec3_t start;
	vec3_t end;
	trace_t tr;
	vec3_t angles;
	float rot;

	if ( !cgs.numDecals || ( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR && !cg.intermissionStarted && !cg.warmup ) ) {
		return;
	}
	if ( cgs.decalEditing ) {
		VectorCopy( cg.refdef.vieworg, start );
		VectorMA( start, 131072, cg.refdef.viewaxis[0], end );
		CG_Trace( &tr, start, NULL, NULL, end, cg.snap->ps.clientNum, MASK_SHOT );
		vectoangles( tr.plane.normal, angles );
		rot = cgs.decals[cgs.decalSelected].rotation;
		if ( angles[PITCH] == -270 ) {
			rot += -cg.snap->ps.viewangles[YAW];
		} else if ( angles[PITCH] == -90 ) {
			rot += cg.snap->ps.viewangles[YAW] + 180;
		} else if ( angles[YAW] == 0 || angles[YAW] == 270 ) {
			rot += -90;
		} else if ( angles[YAW] == 180 || angles[YAW] == 90 ) {
			rot += 90;
		} else {
			rot += 180;
		}
		cgs.decals[cgs.decalSelected].angle = rot;
		VectorCopy( tr.endpos, cgs.decals[cgs.decalSelected].origin );
		VectorCopy( tr.plane.normal, cgs.decals[cgs.decalSelected].dir );
	}
	for ( i = 0; i < cgs.numDecals; i++ ) {
		if ( cgs.decals[i].enabled ) {
			CG_ImpactMark( cgs.decalOSPShader[cgs.decals[i].shader], cgs.decals[i].origin, cgs.decals[i].dir,
				cgs.decals[i].angle, 1, 1, 1, 1, qtrue, cgs.decals[i].size, qtrue, qtrue );
		}
	}
}

/*
CG_OSPDrawDecalInfo: decaledit mode (spectators only): the decal graphics with the selected
decal's one framed, and the list of the 32 decal slots
*/
void CG_OSPDrawDecalInfo( void ) {
	int i;
	int y;
	int cw, ch;
	int x;
	int n;

	if ( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR ) {
		cgs.decalEditing = qfalse;
		return;
	}
	cw = 8;
	ch = 10;
	x = 4;
	n = 8;
	y = 20 * ( cgs.decalOSPShaderCopyCount > n ? 0 : 1 ) + 14;
	for ( i = 0; i < n; i++ ) {
		if ( i == cgs.decals[cgs.decalSelected].shader ) {
			CG_DrawPic( x + 21 * i - 1, y - 1, 20, 20, cgs.media.selectShader );
		} else if ( i + n == cgs.decals[cgs.decalSelected].shader ) {
			CG_DrawPic( x + 21 * i - 1, y + 20 - 1, 20, 20, cgs.media.selectShader );
		}
		if ( i < cgs.decalOSPShaderCopyCount ) {
			CG_DrawPic( x + 21 * i, y, 18, 18, cgs.blender180ShaderCopy[i] );
		}
		if ( i + n < cgs.decalOSPShaderCopyCount ) {
			CG_DrawPic( x + 21 * i, y + 20, 18, 18, cgs.blender180ShaderCopy[i + n] );
		}
	}
	y = 60;
	for ( i = 0; i < MAX_OSP_DECALS; i++ ) {
		if ( cgs.decals[i].enabled ) {
			if ( i == cgs.decalSelected ) {
				vec4_t color;

				color[3] = 0.6f;
				color[0] = 0.7f;
				color[1] = 1.0f;
				color[2] = 0.6f;
				CG_OSPSetColor( color );
				CG_OSPDrawPic( x - 3, y - 1, 22 * cw + 5, ch + 3, cgs.media.teamStatusBar );
				CG_OSPSetColor( NULL );
			}
			CG_OSPDrawStringWithShadow( x, y, va( "%sDecal %2d: ENABLED", ( i == cgs.decalSelected ) ? "^B^3" : "^7", i ), cw, ch, colorWhite, 25 );
			CG_DrawPic( x + 20 * cw, y, 12, 12, cgs.blender180ShaderCopy[cgs.decals[i].shader] );
		} else if ( i < cgs.numDecals ) {
			CG_OSPDrawStringWithShadow( x, y, va( "%sDecal %2d: DISABLED", ( i == cgs.decalSelected ) ? "^B^3" : "^1", i ), cw, ch, colorWhite, 25 );
		} else {
			CG_OSPDrawStringWithShadow( x, y, va( "Decal %2d: OFF", i ), cw, ch, shaftColor, 25 );
		}
		y += 12;
	}
}

/*
CG_OSPIsStatsHidden: the match stats are hidden from a playing client during the match (with
checkGametype: only in tournament)
*/
qboolean CG_OSPIsStatsHidden( qboolean checkGametype, qboolean checkWarmup ) {
	if ( checkGametype )
		return cgs.gametype == GT_TOURNAMENT && CG_OSPIsStatsHidden( qfalse, checkWarmup );
	if ( checkWarmup && cg.warmup ) {
		return qfalse;
	}
	if ( cg.intermissionStarted ) {
		return qfalse;
	}
	if ( cg.demoPlayback ) {
		return qfalse;
	}
	if ( cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR ) {
		return qtrue;
	}
	return qfalse;
}

/*
CG_OSPCvarsRestrictValues: force the allowed values of renderer / client cvars (not on a local
server), vid_restart when one changed
*/
void CG_OSPCvarsRestrictValues( void ) {
	qboolean changed;

	if ( cg_trueLightning.integer > 1 ) {
		trap_Cvar_Set( "cg_trueLightning", "0.5" );
	}
	if ( !cgs.localServer ) {
		changed = qfalse;
		if ( r_fullbright.integer ) {
			trap_Cvar_Set( "r_fullbright", "0" );
			changed = qtrue;
		}
		if ( r_lodCurveError.integer != 250 ) {
			trap_Cvar_Set( "r_lodcurveerror", "250" );
			changed = qtrue;
		}
		if ( r_subdivisions.integer > 80 ) {
			trap_Cvar_Set( "r_subdivisions", "80" );
			changed = qtrue;
		}
		if ( r_znear.integer != 4 ) {
			trap_Cvar_Set( "r_znear", "4" );
			changed = qtrue;
		}
		if ( !( cgs.custom_client_2 & 32 ) && ( com_maxfps.integer > 125 || com_maxfps.integer <= 0 ) ) {
			trap_Cvar_Set( "com_maxfps", "125" );
		}
		if ( r_shownormals.integer ) {
			trap_Cvar_Set( "r_shownormals", "0" );
			changed = qtrue;
		}
		if ( r_showtris.integer ) {
			trap_Cvar_Set( "r_showtris", "0" );
			changed = qtrue;
		}
		if ( cg_shadows.integer > 1 ) {
			trap_Cvar_Set( "cg_shadows", "1" );
			changed = qtrue;
		}
		if ( cg_shadows.integer < 0 ) {
			trap_Cvar_Set( "cg_shadows", "0" );
			changed = qtrue;
		}
		if ( changed ) {
			trap_SendConsoleCommand( "vid_restart;\n" );
		}
	}
}

/*
CG_OSPDrawScoreboard: the id scoreboard (called by the OSP HUD)
*/
qboolean CG_OSPDrawScoreboard( void ) {
	return CG_DrawScoreboard();
}

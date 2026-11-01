/*
 * cg_osp_hud.c: the OSP HUD (cg_enableOSPHUD): intermission, team overlay (ch_Teamoverlay),
 * crosshair names, scores box, weapon bars, Q3Comp status bar.
 */
#include "cg_local.h"

// the team overlay columns parsed from ch_Teamoverlay by
// CG_OSPPrepareTeamOverlay. It is this file's whole bss
// (116 bytes): the 80-byte text ends where cg_osp_customloc.c's bss starts.
typedef struct {
	int		powerupOffset;		// x of the powerup icons (p), drawn right to left
	int		nameOffset;			// x of the name (n)
	int		healthOffset;		// x of the health (h)
	int		armorOffset;		// x of the armor (a)
	int		ammoOffset;			// x of the weapon's ammo icon (w)
	int		locationOffset;		// x of the location (l)
	int		width;				// overlay width in pixels
	int		textOffset;			// x of the literal text (other characters)
	int		nameLen;			// name length in characters (the number before n, default 12)
	char	text[80];			// the literal text, without leading and trailing spaces
} ospTeamOverlay_t;

ospTeamOverlay_t	teamOverlay;

// OSP: this file's data is the initialised
// locals, statics and jump tables of its functions in function order, plus two file-scope tables
// (ospCrosshairColors, shaftColor; cg_osp_main.c uses shaftColor too)

/*
CG_OSPDrawIntermission: the intermission screen: the custom server pictures, the scoreboard, on
OSP servers the local time and the server version, and the OSP logo
*/
void CG_OSPDrawIntermission( void ) {
	int		i;

	if ( cgs.gametype == GT_SINGLE_PLAYER ) {
		CG_OSPDrawCenterString();
		return;
	}
	for ( i = 0; i < cgs.custom_gfx_number; i++ ) {
		if ( cgs.custom_gfx[i][4] >= 0 && cgs.custom_gfx[i][5] >= 0 && cgs.custom_gfx[i][6] > 0 && cgs.custom_gfx[i][7] > 0 && cgs.media.customShader[i] ) {
			CG_DrawPic( cgs.custom_gfx[i][4], cgs.custom_gfx[i][5], cgs.custom_gfx[i][6], cgs.custom_gfx[i][7], cgs.media.customShader[i] );
		}
	}
	cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_OSPDrawScoreboard();
	if ( cgs.isOSPv1 ) {
		qtime_t	qtime;
		char	*days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		char	*months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

		trap_RealTime( &qtime );
		CG_OSPDrawStringWithShadow( 8, 462, va( "%02d:%02d:%02d - %s - %02d %s %d", qtime.tm_hour, qtime.tm_min, qtime.tm_sec, days[qtime.tm_wday], qtime.tm_mday, months[qtime.tm_mon], qtime.tm_year + 1900 ), 10, 10, colorWhite, 0 );
		CG_OSPDrawStringWithShadow( 8, 450, va( "^B%s", CG_ConfigString( 814 ) ), 10, 10, colorYellow, 0 );
	}
	CG_OSPSetColor( NULL );
	CG_DrawPic( 590, 430, 50, 50, cgs.media.blender180Shader );
}

/*
CG_OSPDrawSpectator: the spectator line, and in tournament / freeze tag the waiting hint; frees
the team overlay's CHS buffer (6) when it is not shown
*/
void CG_OSPDrawSpectator( void ) {
	if ( !cg.mvActive ) {
		CG_OSPDrawStringWithShadow( 248, 440, "^BSPECTATOR", 18, 18, colorYellow, 0 );
		if ( cgs.gametype == GT_TOURNAMENT || ( cgs.gameTypeFreeze && CG_OSPGetPresistantTeam() != TEAM_SPECTATOR ) ) {
			CG_DrawBigString( 200, 460, "Waiting to play", 1.0f );
		} else if ( cgs.gametype >= GT_TEAM && ( !cgs.gameTypeFreeze || CG_OSPGetPresistantTeam() == TEAM_SPECTATOR ) ) {
			CG_DrawBigString( 8, 460, "press ESC and use the START menu to play", 1.0f );
		}
	}
	if ( !cgs.gameTypeFreeze || CG_OSPGetPresistantTeam() == TEAM_SPECTATOR ) {
		CHS_Free( 6 );
	}
}

/*
CG_OSPHUDRoutine: the OSP HUD (cg_enableOSPHUD): the status bar of ch_StatusbarFormat, the
lagometer, team info and overlay, the weapon bars of ch_Weaponswitch, pickup / holdable items,
the scores box, the upper right column, the attacker, crosshair, vote, warmup, scoreboard and
the HMI menu
*/
void CG_OSPHUDRoutine( void ) {
	qboolean	isTeamGame;
	qboolean	isAlive;
	float		pos1;
	qboolean	isSpectator;
	float		drew;
	float		pos;

	isTeamGame = cgs.gametype >= GT_TEAM ? qtrue : qfalse;
	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		CG_OSPDrawIntermission();
		CG_OSPDrawVote();
		return;
	}
	isSpectator = cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ? qtrue : qfalse;
	isAlive = cg.snap->ps.stats[STAT_HEALTH] > 0 && !isSpectator ? qtrue : qfalse;
	if ( isSpectator ) {
		CG_OSPDrawSpectator();
	}
	drew = pos = 480.0f;
	if ( isAlive ) {
		switch ( ch_StatusbarFormat.integer ) {
		case 0:
			CG_DrawStatusBar();
			drew = 432.0f;
			break;
		default:
			CG_DrawStatusBar();
			drew = 432.0f;
			break;
		case 1:
			drew = CG_OSPDrawStatusBar( pos );
			break;
		case 2:
			drew = CG_OSPDrawStatusBar( pos );
			CG_OSPDrawStatusBarAmmo1( 480.0f );
			break;
		case 3:
			drew = CG_OSPDrawStatusBar( pos );
			CG_OSPDrawStatusBarAmmo2( 480.0f );
			break;
		case 4:
			CG_OSPDrawCPMStatusbar0();
			break;
		case 5:
			CG_OSPDrawCPMStatusbarBars();
			break;
		case 6:
			CG_OSPDrawCPMStatusbar3();
			break;
		case 7:
			CG_OSPDrawCPMStatusbar4();
			break;
		case 8:
			CG_OSPDrawCPMStatusbar5();
			break;
		case 9:
			CG_OSPDrawQ3CompStatusbar();
			drew = 424.0f;
			break;
		case 10:
			CG_OSPDrawQ3CompStatusbar();
			drew = 424.0f;
			break;
		case 11:
			CG_OSPDrawQ3CompStatusbar();
			drew = 424.0f;
			break;
		case 12:
			CG_OSPDrawQ3CompStatusbar();
			drew = 424.0f;
			break;
		}
	}
	if ( cg_lagometer.integer == 1 ) {
		CG_DrawLagometer( 428 - ( cg_drawPing.integer > 0 ? 14 : 0 ) );
	}
	pos1 = pos = ( drew < 432.0f ? drew : 432.0f ) - ( cg_lagometer.integer == 1 && cg_drawPing.integer > 0 ? 14 : 0 );
	if ( isTeamGame ) {
		pos = CG_OSPDrawTeamInfo( pos );
	}
	if ( isTeamGame && cg_drawTeamOverlay.integer == 2 && ( isAlive || ( ( CG_OSPIsGameTypeCA( cgs.gametype ) || cgs.gameTypeFreeze ) && CG_OSPGetPresistantTeam() != TEAM_SPECTATOR ) ) ) {
		pos1 = pos = CG_OSPDrawTeamOverlay( pos1, qtrue, qfalse );
	}
	if ( isAlive ) {
		if ( isTeamGame && cg_drawTeamOverlay.integer == 3 ) {
			pos = CG_OSPDrawTeamOverlay( pos, qfalse, qfalse );
		}
		if ( ch_Weaponswitch.integer == 0 || ch_Weaponswitch.integer == 1 ) {
			CG_OSPDrawWeaponBarOld( pos );
		} else if ( ch_Weaponswitch.integer == 4 || ch_Weaponswitch.integer == 5 ) {
			CG_OSPDrawWeaponBar_4_5( pos );
		} else if ( ch_Weaponswitch.integer == 9 ) {
			CG_OSPDrawWeaponBarOld( 380.0f );
		}
		if ( ch_Pickupitem.integer ) {
			pos = CG_OSPDrawPickupItem( pos );
		}
	} else if ( isTeamGame && cg_drawTeamOverlay.integer == 3 && ( CG_OSPIsGameTypeCA( cgs.gametype ) || cgs.gameTypeFreeze ) && CG_OSPGetPresistantTeam() != TEAM_SPECTATOR ) {
		pos = CG_OSPDrawTeamOverlay( pos, qfalse, qfalse );
	}
	pos = CG_OSPHUDDrawScores( pos1 );
	if ( ch_StatusbarFormat.integer >= 9 ) {
		pos = ( pos > pos1 ? pos1 : pos ) - 3.0f;
	}
	if ( !isSpectator ) {
		pos = CG_OSPDrawHoldableItem( pos );
	}
	if ( isAlive && ch_Weaponswitch.integer != 6 && ch_Weaponswitch.integer != 7 ) {
		CG_OSPDrawWeaponBar_2_3( pos );
	}
	pos = 0;
	if ( isTeamGame && cg_drawTeamOverlay.integer == 1 && ( isAlive || ( ( CG_OSPIsGameTypeCA( cgs.gametype ) || cgs.gameTypeFreeze ) && CG_OSPGetPresistantTeam() != TEAM_SPECTATOR ) ) ) {
		pos = CG_OSPDrawTeamOverlay( pos, qtrue, qtrue );
	}
	if ( cg_drawSnapshot.integer ) {
		pos = CG_DrawSnapshot( pos );
	}
	if ( cg_drawFPS.integer ) {
		pos = CG_DrawFPS( pos );
	}
	if ( cg_drawTimer.integer ) {
		pos = CG_DrawTimer( pos );
	}
	if ( ch_drawSpeed.integer == 1 ) {
		pos = CG_OSPDrawSpeed( pos );
	}
	if ( cg_lagometer.integer > 1 ) {
		pos = CG_DrawLagometer( pos );
	} else if ( cg_lagometer.integer <= 0 && cg_drawPing.integer != 0 ) {
		pos = CG_OSPDrawPing( pos );
	}
	if ( isAlive ) {
		if ( ch_Weaponswitch.integer < 6 ) {
		} else if ( ch_Weaponswitch.integer == 6 && ch_StatusbarFormat.integer != 7 ) {
			CG_OSPDrawWeaponBar_6( pos );
		} else if ( ch_Weaponswitch.integer == 7 ) {
			CG_OSPDrawWeaponBar_7( 48.0f );
		} else if ( ch_Weaponswitch.integer == 8 || ( ch_StatusbarFormat.integer == 7 && ch_Weaponswitch.integer == 8 ) ) {
			switch ( ch_StatusbarFormat.integer ) {
			default:
			case 4:
				CG_OSPDrawWeaponBarCPM1( 380 );
				cpmDrawWeaponSelect = qfalse;
				break;
			case 5:
				CG_OSPDrawWeaponBarCPM1( 380 );
				cpmDrawWeaponSelect = qfalse;
				break;
			case 6:
				CG_OSPDrawWeaponBarCPM2( 285, 425 );
				cpmDrawWeaponSelect = qfalse;
				break;
			case 7:
				CG_OSPDrawWeaponBarCPM2( 110, 425 );
				cpmDrawWeaponSelect = qfalse;
				break;
			}
		}
	}
	if ( cg_drawAttacker.integer && ( ch_Weaponswitch.integer != 7 || !isAlive ) ) {
		pos = CG_DrawAttacker( pos );
	}
	if ( isAlive ) {
		CG_DrawAmmoWarning();
	}
	if ( !cg.hmiActive ) {
		CG_OSPDrawCrosshair();
		CG_OSPDrawCrosshairNames();
	}
	CG_DrawReward();
	CG_OSPDrawVote();
	if ( !CG_OSPDrawSpectatorInfo() ) {
		CG_DrawWarmup();
	}
	if ( !cg_ignore.integer ) {
		if ( !CG_OSPDrawScoreboard() ) {
			CG_OSPDrawCenterString();
		}
	} else {
		if ( !CG_DrawScoreboard() ) {
			CG_OSPDrawCenterString();
		}
	}
	if ( cg.hmiActive ) {
		HMI_Draw();
	}
}

/*
CG_OSPDrawStatusBar: ch_StatusbarFormat 1..3: one line of ammo (with the ammo icon), health and
armor (with the CPM armor icon) in cf_Statusbar digits at the bottom; returns the new y
*/
float CG_OSPDrawStatusBar( float y ) {
	int			fontW;
	int			value;
	playerState_t	*ps;
	int			fontH;
	int			offsetW;
	centity_t	*cent;
	float		h;
	vec4_t		hcolor;
	int			color;
	qhandle_t	icon;
	static float colors[4][4] = {
		{ 1.0f, 0.69f, 0.0f, 1.0f },	// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	if ( !cg_drawStatus.integer ) {
		return y;
	}
	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;
	CG_OSPGetClientFontSize( &cf_Statusbar, &fontW, &fontH );
	h = fontH;
	value = ps->ammo[cent->currentState.weapon];
	offsetW = value < 0 ? -2 * fontW : ( value < 10 ? -fontW : ( value < 100 ? -fontW / 2 : 0 ) );
	y -= h;
	if ( ch_TeamBackground.integer ) {
		CG_DrawTeamBackground( 0, y, 640, h, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );
	}
	if ( cg.predictedPlayerState.powerups[PW_REDFLAG] )
		CG_DrawFlagModel( 320 - 12 * fontW / 2 + offsetW + fontW * 8, y, fontW, fontH, TEAM_RED, qfalse );
	else if ( cg.predictedPlayerState.powerups[PW_BLUEFLAG] )
		CG_DrawFlagModel( 320 - 12 * fontW / 2 + offsetW + fontW * 8, y, fontW, fontH, TEAM_BLUE, qfalse );
	if ( cent->currentState.weapon ) {
		value = ps->ammo[cent->currentState.weapon];
		if ( value > -1 ) {
			if ( cg.predictedPlayerState.weaponstate == WEAPON_FIRING && cg.predictedPlayerState.weaponTime > 100 ) {
				color = 2;
			} else if ( value >= 0 ) {
				color = 0;
			} else {
				color = 1;
			}
			trap_R_SetColor( colors[color] );
			CG_OSPDrawStatusBarString( 320 - 12 * fontW / 2 + offsetW, y, 3, fontW, fontH, value );
			trap_R_SetColor( NULL );
			icon = cg_weapons[cg.predictedPlayerState.weapon].ammoIcon;
			if ( icon ) {
				CG_DrawPic( 320 - 12 * fontW / 2 + offsetW + 3 * fontW, y, fontW, fontH, icon );
			}
		}
	}
	value = ps->stats[STAT_HEALTH];
	if ( value > 100 ) {
		trap_R_SetColor( colors[3] );
	} else if ( value > 25 ) {
		trap_R_SetColor( colors[0] );
	} else if ( value > 0 ) {
		color = ( cg.time >> 8 ) & 1;
		trap_R_SetColor( colors[color] );
	} else {
		trap_R_SetColor( colors[1] );
	}
	CG_OSPDrawStatusBarString( 320 - 12 * fontW / 2 + offsetW + 5 * fontW, y, 3, fontW, fontH, value );
	CG_ColorForHealth( hcolor );
	trap_R_SetColor( hcolor );
	value = ps->stats[STAT_ARMOR];
	if ( value > 0 ) {
		trap_R_SetColor( colors[0] );
		CG_OSPDrawStatusBarString( 320 - 12 * fontW / 2 + offsetW + 9 * fontW, y, 3, fontW, fontH, value );
		if ( cgs.server_mode & 2 ) {
			qhandle_t	armorIcon;

			armorIcon = ( &cgs.media.armorIconGreen )[ps->stats[STAT_ARMOR_TYPE]];
			if ( armorIcon ) {
				CG_DrawPic( 320 - 12 * fontW / 2 + offsetW + 9 * fontW + 3 * fontW + 3, 452, 24, 24, armorIcon );
			}
		}
	}
	trap_R_SetColor( NULL );
	return y;
}

/*
CG_OSPDrawStatusBarString: draws val right-aligned in len (1..5) big digits of w x h at x, y,
clamped to what fits
*/
void CG_OSPDrawStatusBarString( int x, int y, int len, int w, int h, int val ) {
	char	*ptr;
	int		bufLen;
	int		shaderIndex;
	char	buf[16];

	if ( len < 1 ) {
		return;
	}
	if ( len > 5 ) {
		len = 5;
	}
	switch ( len ) {
	case 1:
		val = val > 9 ? 9 : val;
		val = val < 0 ? 0 : val;
		break;
	case 2:
		val = val > 99 ? 99 : val;
		val = val < -9 ? -9 : val;
		break;
	case 3:
		val = val > 999 ? 999 : val;
		val = val < -99 ? -99 : val;
		break;
	case 4:
		val = val > 9999 ? 9999 : val;
		val = val < -999 ? -999 : val;
		break;
	}
	Com_sprintf( buf, 16, "%i", val );
	bufLen = strlen( buf );
	if ( bufLen > len ) {
		bufLen = len;
	}
	x += w * ( len - bufLen ) + 2;
	for ( ptr = buf; *ptr && bufLen; bufLen-- ) {
		if ( *ptr == '-' ) {
			shaderIndex = 10;
		} else {
			shaderIndex = *ptr - '0';
		}
		CG_DrawPic( x, y, w, h, cgs.media.numberShaders[shaderIndex] );
		x += w;
		ptr++;
	}
}

/*
CG_OSPDrawPickupItem: ch_Pickupitem: the last picked up item's icon and name (cf_Pickupstring),
fading out over 3 seconds; returns the new y
*/
float CG_OSPDrawPickupItem( float y ) {
	int		item;
	int		h;
	int		w;
	float	*color;

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		return y;
	}
	CG_OSPGetClientFontSize( &cf_Pickupstring, &w, &h );
	y -= h;
	item = cg.itemPickup;
	if ( item ) {
		color = CG_FadeColor( cg.itemPickupTime, 3000 );
		if ( color ) {
			CG_RegisterItemVisuals( item );
			trap_R_SetColor( color );
			CG_DrawPic( 8, y, w, h, cg_items[item].icon );
			CG_OSPDrawString( w + 8 + w / 3, y, bg_itemlist[item].pickup_name, w, h, color, 0, 0 );
			trap_R_SetColor( NULL );
		}
	}
	return y;
}

/*
CG_OSPDrawTeamInfo: the team chat lines (cg_teamChatHeight, cf_Teamchat) above y, with the
team background (ch_TeamBackground) and ch_InverseTeamChat; returns the new y
*/
float CG_OSPDrawTeamInfo( float y ) {
	int		i;
	int		maxChars;
	int		chatHeight;
	int		h;
	int		w;
	int		len;
	vec4_t	hcolor;
	int		x;
	int		chatYOffset;

	if ( cg_teamChatHeight.integer <= 0 ) {
		return y;
	}
	if ( cg_teamChatHeight.integer < 8 ) {
		chatHeight = cg_teamChatHeight.integer;
	} else {
		chatHeight = 8;
	}
	x = 0;
	CG_OSPGetClientFontSize( &cf_Teamchat, &w, &h );
	if ( cgs.teamLastChatPos != cgs.teamChatPos ) {
		if ( cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer ) {
			cgs.teamLastChatPos++;
		}
		chatYOffset = ( cgs.teamChatPos - cgs.teamLastChatPos ) * h;
		maxChars = 0;
		for ( i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++ ) {
			len = CG_DrawStrlen( cgs.teamChatMsgs[i % chatHeight] );
			if ( len > maxChars ) {
				maxChars = len;
			}
		}
		maxChars *= w;
		maxChars += 2 * w;
		if ( ch_TeamBackground.integer ) {
			if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
				hcolor[0] = 1.0f;
				hcolor[1] = 0.0f;
				hcolor[2] = 0.0f;
				hcolor[3] = 0.33f;
			} else if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
				hcolor[0] = 0.0f;
				hcolor[1] = 0.0f;
				hcolor[2] = 1.0f;
				hcolor[3] = 0.33f;
			} else {
				hcolor[0] = 0.0f;
				hcolor[1] = 1.0f;
				hcolor[2] = 0.0f;
				hcolor[3] = 0.33f;
			}
			trap_R_SetColor( hcolor );
			CG_DrawPic( x, y - chatYOffset, 640, chatYOffset, cgs.media.teamStatusBar );
			trap_R_SetColor( NULL );
		}
		hcolor[0] = hcolor[1] = hcolor[2] = 1.0f;
		hcolor[3] = 1.0f;
		for ( i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++ ) {
			maxChars = ( 640 - ( cg_drawTeamOverlay.integer == 2 ? cg.teamOverlayWidth : 0 ) ) / w;
			CG_OSPDrawString( w, ch_InverseTeamChat.integer ? y - ( chatHeight + 1 - cgs.teamChatPos + i ) * h : y - ( cgs.teamChatPos - i ) * h,
				cgs.teamChatMsgs[i % chatHeight], w, h, NULL, maxChars, 0 );
		}
	}
	return y - chatHeight * h;
}

/*
CG_OSPDrawWeaponBarOld: ch_Weaponswitch 0 / 1 / 2 / 9: a horizontal row of the owned weapons
(cf_WeaponSwitch) above pos_y, the selected one framed, with the weapon name (cf_WeaponName)
in style 1; returns the new y
*/
float CG_OSPDrawWeaponBarOld( float pos_y ) {
	int			weaponNum;
	int			w;
	int			x;
	int			h;
	int			y;
	int			weapons;
	int			count;
	float		*color;
	const char	*name;
	int			width;

	if ( ch_Weaponswitch.integer == 1 || ch_Weaponswitch.integer == 9 ) {
		color = CG_FadeColor( cg.weaponSelectTime, 1400 );
		if ( !color ) {
			return pos_y;
		}
	} else if ( ch_Weaponswitch.integer == 2 ) {
		color = colorWhite;
	} else {
		return pos_y;
	}
	CG_OSPGetClientFontSize( &cf_WeaponSwitch, &w, &h );
	trap_R_SetColor( color );
	cg.itemPickupTime = 0;
	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	count = 0;
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weapons & ( 1 << weaponNum ) ) {
			count++;
		}
	}
	x = 320 - count * ( w + 4 ) / 2;
	y = pos_y - h - 4.0f;
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( !( weapons & ( 1 << weaponNum ) ) ) {
			continue;
		}
		CG_RegisterWeapon( weaponNum );
		CG_DrawPic( x, y, w, h, cg_weapons[weaponNum].weaponIcon );
		if ( weaponNum == cg.weaponSelect ) {
			CG_DrawPic( x - 2, y - 2, w + 4, h + 4, cgs.media.selectShader );
		}
		if ( !cg.snap->ps.ammo[weaponNum] ) {
			CG_DrawPic( x, y, w, h, cgs.media.noammoShader );
		}
		x += w + 4;
	}
	if ( ch_Weaponswitch.integer == 1 && cg_weapons[cg.weaponSelect].item ) {
		int		nameH;
		int		nameW;

		if ( CG_OSPGetClientFontSize( &cf_WeaponName, &nameW, &nameH ) ) {
			name = cg_weapons[cg.weaponSelect].item->pickup_name;
			if ( name ) {
				width = CG_DrawStrlen( name ) * nameW;
				x = ( 640 - width ) / 2;
				CG_OSPDrawString( x, y - nameH - 2, name, nameW, nameH, color, 0, 0 );
				pos_y -= nameH;
			}
		}
	}
	trap_R_SetColor( NULL );
	return pos_y - h - 4.0f;
}

/*
CG_OSPDrawWeaponSelect: a vertical column of the owned weapons (cf_WeaponSwitch) from x, y down,
the selected one framed, in color; returns the y below it
*/
float CG_OSPDrawWeaponSelect( float x, float y, const float *color ) {
	int		weaponNum;
	int		h;
	int		w;
	int		weapons;

	CG_OSPGetClientFontSize( &cf_WeaponSwitch, &w, &h );
	trap_R_SetColor( color );
	if ( ch_Weaponswitch.integer == 2 ) {
		cg.itemPickupTime = 0;
	}
	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( !( weapons & ( 1 << weaponNum ) ) ) {
			continue;
		}
		CG_RegisterWeapon( weaponNum );
		CG_DrawPic( x, y, w, h, cg_weapons[weaponNum].weaponIcon );
		if ( weaponNum == cg.weaponSelect ) {
			CG_DrawPic( x - 2, y - 2, w + 4, h + 4, cgs.media.selectShader );
		}
		if ( !cg.snap->ps.ammo[weaponNum] ) {
			CG_DrawPic( x, y, w, h, cgs.media.noammoShader );
		}
		y += h + 2;
	}
	trap_R_SetColor( NULL );
	return y;
}

/*
CG_OSPDrawHoldableItem: the holdable item's icon at the right edge above y; returns the new y
*/
float CG_OSPDrawHoldableItem( float y ) {
	int		value;

	value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
	if ( value > 0 && value < bg_numItems ) {
		CG_RegisterItemVisuals( value );
		y -= 48.0f;
		CG_DrawPic( 592, y, 48, 48, cg_items[value].icon );
	}
	return y;
}

/*
CG_OSPCountTeammates: counts the valid team mates of the sorted team list and their longest
name (at most 12 characters) for the team overlay (cg.numTeammates / 1e848)
*/
void CG_OSPCountTeammates( void ) {
	clientInfo_t	*ci;
	int				i;
	int				len;
	int				maxLen;
	int				count;

	// OSP bug, kept for compatibility: compares the boolean "team == TEAM_FREE" with TEAM_RED and TEAM_BLUE, so it
	// never returns
	if ( ( cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_FREE ) == TEAM_RED && ( cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_FREE ) == TEAM_BLUE ) {
		return;
	}
	maxLen = count = 0;
	for ( i = 0; i < numSortedTeamPlayers; i++ ) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cgs.clientinfo[cg.snap->ps.clientNum].team ) {
			count++;
			len = CG_DrawStrlen( ci->name );
			if ( len > maxLen ) {
				maxLen = len;
			}
		}
	}
	cg.numTeammates = count;
	cg.teammateNameLen = maxLen < 12 ? maxLen : 12;
}

/*
CG_OSPPrepareTeamOverlay: parses ch_Teamoverlay into the column offsets of teamOverlay (w: the
character width, right: the overlay is at the right edge). Format characters: p powerups, <n>n
name of n characters (default 12), h health, a armor, w ammo (weapon icon), l location
(cg_MaxlocationWidth characters), space; anything else is copied as text. A bad setting is reset
to "p12n h/aw l"
*/
void CG_OSPPrepareTeamOverlay( int w, qboolean right ) {
	qboolean	isNamePresent = qfalse;
	char		*p = ch_Teamoverlay.string;
	int			nameLen = 0;
	int			numberOfPowerups, numberOfNames, numberOfHealths, numberOfArmors, numberOfAmmos, numberOfLocations;
	int			i;
	char		string[128];
	char		*ptr;

	numberOfPowerups = numberOfNames = numberOfHealths = numberOfArmors = numberOfAmmos = numberOfLocations = 0;
	for ( i = 0; i < 127; i++ ) {
		string[i] = ' ';
	}
	string[127] = 0;

	if ( strlen( ch_Teamoverlay.string ) > 20 ) {
		trap_Cvar_Set( CG_OSPGetCvarName( &ch_Teamoverlay ), "p12n h/aw l" );
		CG_Printf( "Rejected \"%s\" setting, too long\n", CG_OSPGetCvarName( &ch_Teamoverlay ) );
		memset( &teamOverlay, 0, sizeof( teamOverlay ) );
		return;
	}

	i = 0;
	memset( &teamOverlay, 0, sizeof( teamOverlay ) );
	while ( *p ) {
rescan:
		switch ( *p ) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			nameLen = 0;
			while ( *p >= '0' && *p <= '9' ) {
				nameLen = 10 * nameLen + ( *p - '0' );
				p++;
			}
			goto rescan;
		case 'p':
		case 'P':
			teamOverlay.powerupOffset = i;
			i += w;
			numberOfPowerups++;
			break;
		case 'n':
		case 'N':
			teamOverlay.nameOffset = i;
			teamOverlay.nameLen = nameLen ? nameLen : 12;
			i += teamOverlay.nameLen * w;
			isNamePresent = qtrue;
			numberOfNames++;
			break;
		case 'h':
		case 'H':
			teamOverlay.healthOffset = i;
			i += 3 * w;
			numberOfHealths++;
			break;
		case 'a':
		case 'A':
			teamOverlay.armorOffset = i;
			i += 3 * w;
			numberOfArmors++;
			break;
		case 'w':
		case 'W':
			teamOverlay.ammoOffset = i;
			i += w;
			numberOfAmmos++;
			break;
		case 'l':
		case 'L':
			teamOverlay.locationOffset = i;
			i += cg_MaxlocationWidth.integer * w;
			numberOfLocations++;
			break;
		case ' ':
			i += w;
			break;
		default:
			string[i / w] = *p;
			i += w;
			break;
		}
		p++;
		nameLen = 0;
	}

	if ( numberOfPowerups > 1 || numberOfNames > 1 || numberOfHealths > 1 || numberOfArmors > 1 ||
		numberOfAmmos > 1 || numberOfLocations > 1 ) {
		trap_Cvar_Set( CG_OSPGetCvarName( &ch_Teamoverlay ), "p12n h/aw l" );
		CG_Printf( "Rejected \"%s\" setting, used an item more than once\n", CG_OSPGetCvarName( &ch_Teamoverlay ) );
		memset( &teamOverlay, 0, sizeof( teamOverlay ) );
		return;
	}
	if ( !isNamePresent ) {
		trap_Cvar_Set( CG_OSPGetCvarName( &ch_Teamoverlay ), "p12n h/aw l" );
		CG_Printf( "Rejected \"%s\" setting - useless\n", CG_OSPGetCvarName( &ch_Teamoverlay ) );
		memset( &teamOverlay, 0, sizeof( teamOverlay ) );
		return;
	}

	string[i / w] = 0;

	// the text without its leading and trailing spaces
	for ( ptr = string; *ptr && *ptr == ' '; ptr++ ) {
	}
	p = ptr;
	teamOverlay.textOffset = ( ptr - string ) * w;
	ptr = ptr + strlen( ptr ) - 1;
	while ( ptr > p ) {
		if ( *ptr == ' ' ) {
			*ptr = 0;
			ptr--;
		} else {
			break;
		}
	}
	Q_strncpyz( teamOverlay.text, p, sizeof( teamOverlay.text ) );

	teamOverlay.width = i;
	cg.teamOverlayWidth = i;

	if ( numberOfPowerups ) {
		teamOverlay.powerupOffset += right ? SCREEN_WIDTH - i : 0;
		if ( !teamOverlay.powerupOffset )
			teamOverlay.powerupOffset++;
	}
	if ( numberOfNames )
		teamOverlay.nameOffset += right ? SCREEN_WIDTH - i : 0;
	if ( numberOfHealths )
		teamOverlay.healthOffset += right ? SCREEN_WIDTH - i : 0;
	if ( numberOfArmors )
		teamOverlay.armorOffset += right ? SCREEN_WIDTH - i : 0;
	if ( numberOfAmmos )
		teamOverlay.ammoOffset += right ? SCREEN_WIDTH - i : 0;
	if ( numberOfLocations )
		teamOverlay.locationOffset += right ? SCREEN_WIDTH - i : 0;
	if ( teamOverlay.textOffset )
		teamOverlay.textOffset += right ? SCREEN_WIDTH - i : 0;
}

/*
CG_OSPDrawTeamOverlay: OSP's team overlay (cf_Teamoverlay font, ch_Teamoverlay columns): one line
per team mate on the team colour background, buffered as CHS item 6 and redrawn only when
cg.teamOverlayChanged or a setting changed. upper: drawn downwards from y (returns the bottom),
otherwise upwards (returns the top)
*/
float CG_OSPDrawTeamOverlay( float y, qboolean right, qboolean upper ) {
	clientInfo_t	*ci;
	int				k;
	int				w;
	int				px;
	int				h;
	int				i;
	int				team;
	char			buf[16];
	const char		*location;
	vec4_t			healthColor;
	int				height;
	int				x;
	static int		teamOverlayPosY = 0;
	static vec4_t	colorTeamOverlay = { 0, 0, 0, 0.33f };
	static int		modCfCount = -1;
	static int		modChCount = -1;
	static int		modMaxLocationWidth = -1;
	static int		modDrawTeamOverlay = -1;

	team = CG_OSPGetPresistantTeam();
	if ( ( team != TEAM_RED && team != TEAM_BLUE ) || !numSortedTeamPlayers ) {
		CHS_Free( 6 );
		return y;
	}

	CG_OSPGetClientFontSize( &cf_Teamoverlay, &w, &h );
	if ( cf_Teamoverlay.modificationCount != modCfCount || ch_Teamoverlay.modificationCount != modChCount ||
		cg_MaxlocationWidth.modificationCount != modMaxLocationWidth ||
		cg_drawTeamOverlay.modificationCount != modDrawTeamOverlay ) {
		modChCount = ch_Teamoverlay.modificationCount;
		modCfCount = cf_Teamoverlay.modificationCount;
		modMaxLocationWidth = cg_MaxlocationWidth.modificationCount;
		modDrawTeamOverlay = cg_drawTeamOverlay.modificationCount;
		CG_OSPPrepareTeamOverlay( w, right );
		cg.teamOverlayChanged = qtrue;
	}
	if ( !cg.teamOverlayChanged ) {
		CHS_Show( 6 );
		return teamOverlayPosY;
	}
	cg.teamOverlayChanged = qfalse;

	if ( right ) {
		x = SCREEN_WIDTH - teamOverlay.width;
	} else {
		x = 0;
	}
	height = numSortedTeamPlayers * h;
	if ( upper ) {
		teamOverlayPosY = y + height;
	} else {
		y -= height;
		teamOverlayPosY = y;
	}

	CHS_Begin( 6, 0, 0, 0, 0 );
	CHS_Show( 6 );

	if ( team == TEAM_RED ) {
		colorTeamOverlay[0] = 1.0f;
		colorTeamOverlay[2] = 0.0f;
	} else {
		colorTeamOverlay[0] = 0.0f;
		colorTeamOverlay[2] = 1.0f;
	}
	CG_OSPSetColor( colorTeamOverlay );
	CG_OSPDrawPic( x, y, teamOverlay.width, height, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );

	for ( i = 0; i < numSortedTeamPlayers; i++ ) {
		ci = &cgs.clientinfo[sortedTeamPlayers[i]];
		if ( ci->infoValid && ( ci->team == team || ci->rt == team ) ) {
			if ( teamOverlay.nameOffset ) {
				CG_OSPDrawString( teamOverlay.nameOffset, y, ci->name, w, h, NULL, teamOverlay.nameLen, 0 );
			}
			if ( teamOverlay.healthOffset ) {
				CG_GetColorForHealth( ci->health, ci->armor, healthColor );
				Com_sprintf( buf, sizeof( buf ), "%3i", ci->health );
				CG_OSPDrawString( teamOverlay.healthOffset, y, buf, w, h, healthColor, 0, 0 );
			}
			if ( teamOverlay.armorOffset ) {
				Com_sprintf( buf, sizeof( buf ), "%3i", ci->armor );
				CG_OSPDrawString( teamOverlay.armorOffset, y, buf, w, h, healthColor, 0, 0 );
			}
			if ( teamOverlay.textOffset ) {
				CG_OSPDrawString( teamOverlay.textOffset, y, teamOverlay.text, w, h, NULL, 0, 0 );
			}
			if ( teamOverlay.ammoOffset ) {
				if ( cg_weapons[ci->curWeapon].weaponIcon ) {
					CG_OSPDrawPic( teamOverlay.ammoOffset, y, w, h, cg_weapons[ci->curWeapon].weaponIcon );
				} else {
					CG_OSPDrawPic( teamOverlay.ammoOffset, y, w, h, cgs.media.deferShader );
				}
			}
			if ( cg_MaxlocationWidth.integer != 0 && teamOverlay.locationOffset != 0 ) {
				if ( cg.customLocationsEnabled ) {
					location = CG_CustomLocationsGetName( ci->customLocation );
				} else if ( ch_ColorLocations.integer && cgs.gametype == GT_CTF )
					location = CG_GetCTFLocation( ci->location );
				else
					location = CG_ConfigString( CS_LOCATIONS + ci->location );
				if ( !location || !*location ) {
					location = "unknown";
				}
				CG_OSPDrawString( teamOverlay.locationOffset, y, location, w, h, NULL, cg_MaxlocationWidth.integer, 0 );
			}
			if ( teamOverlay.powerupOffset ) {
				px = teamOverlay.powerupOffset;
				for ( k = 0; k < PW_NUM_POWERUPS; k++ ) {
					if ( cgs.gameTypeFreeze != 0 && ci->health <= 0 ) {
						CG_OSPDrawPic( px, y, w, h, cgs.media.noammoShader );
						px -= w;
						break;
					}
					if ( ci->powerups & ( 1 << k ) ) {
						gitem_t	*item;

						item = BG_FindItemForPowerup( k );
						if ( !item ) {
							continue;
						}
						CG_OSPDrawPic( px, y, w, h, trap_R_RegisterShader( item->icon ) );
						px -= w;
					}
				}
			}
			y += h;
		}
	}
	CHS_End();
	return teamOverlayPosY;
}

/*
CG_OSPDrawCrosshairNames: the name of the player under the crosshair (cf_CrosshairNames, fading
out in its last 200 ms), centred at y 170 or, with ch_CrosshairNamesLeft, at the left above the
team chat; team mates also get "health/armor" (ch_TeamCrosshairHealth, cf_TeamCrosshairHealth)
*/
void CG_OSPDrawCrosshairNames( void ) {
	clientInfo_t	*ci;
	float			len;
	int				h;
	int				w;
	float			*color;
	char			str[64];
	int				w2;
	vec4_t			color2;
	int				h2;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}
	if ( !cg_drawCrosshairNames.integer ) {
		return;
	}
	if ( cg.renderingThirdPerson ) {
		return;
	}
	if ( cg.mvActive > 1 ) {
		return;
	}
	if ( !cg.osp_unk_1e84c ) {
		CG_ScanForCrosshairEntity();
	}

	color = CG_FadeColor( cg.crosshairClientTime, 1000 );
	if ( !color ) {
		trap_R_SetColor( NULL );
		return;
	}

	ci = &cgs.clientinfo[cg.crosshairClientNum];
	CG_OSPGetClientFontSize( &cf_CrosshairNames, &w, &h );

	len = cg.time - cg.crosshairClientTime;
	if ( 1000.0f - len < 200.0f ) {
		w = ( 1000.0f - len ) * w / 200.0f;
		h = ( 1000.0f - len ) * h / 200.0f;
	}

	if ( !( ci->team == cg.snap->ps.persistant[PERS_TEAM] || cg_drawCrosshairNames.integer != 2 ||
		( cgs.gameTypeFreeze && ci->team == cgs.clientinfo[cg.snap->ps.clientNum].team ) ) ) {
		return;
	}

	if ( !ch_CrosshairNamesLeft.integer ) {
		len = CG_DrawStrlen( ci->name ) * w;
		CG_OSPDrawString( 320.0f - len / 2.0f, 170, ci->name, w, h, NULL, 0, 0 );
		if ( ci->team != TEAM_FREE && ci->team == cg.snap->ps.persistant[PERS_TEAM] &&
			ch_TeamCrosshairHealth.integer != 0 && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			CG_OSPGetClientFontSize( &cf_TeamCrosshairHealth, &w2, &h2 );
			CG_GetColorForHealth( ci->health, ci->armor, color2 );
			Com_sprintf( str, 64, "%i/%i", ci->health, ci->armor );
			len = CG_DrawStrlen( str ) * w2;
			color2[3] = color[3];
			CG_OSPDrawString( 320.0f - len / 2.0f, h + 170, str, w2, h2, color2, 0, 0 );
		}
	} else {
		int		y;
		int		wStatus;
		int		hTeam;
		int		hStatus;
		int		healthW;
		int		xName;
		int		xHealth;

		CG_OSPGetClientFontSize( &cf_Statusbar, &wStatus, &hStatus );
		CG_OSPGetClientFontSize( &cf_Teamchat, &wStatus, &hTeam );
		y = 480 - hStatus - cg_teamChatHeight.integer * hTeam - h;
		if ( ci->team != TEAM_FREE && ci->team == cg.snap->ps.persistant[PERS_TEAM] &&
			ch_TeamCrosshairHealth.integer != 0 && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			CG_OSPGetClientFontSize( &cf_TeamCrosshairHealth, &w2, &h2 );
			y -= h2;
			Com_sprintf( str, 64, "%i/%i", ci->health, ci->armor );
			len = CG_DrawStrlen( ci->name ) * w;
			healthW = CG_DrawStrlen( str ) * w2;
			if ( len > healthW ) {
				xName = 0;
				xHealth = len / 2.0f - healthW / 2;
			} else {
				xName = healthW / 2 - len / 2.0f;
				xHealth = 0;
			}
			CG_OSPDrawString( xName, y, ci->name, w, h, color, 0, 0 );
			CG_GetColorForHealth( ci->health, ci->armor, color2 );
			color2[3] = 0.5f * color[3];
			CG_OSPDrawString( xHealth, y + h, str, w2, h2, color2, 0, 0 );
		} else {
			CG_OSPDrawString( 0, y, ci->name, w, h, color, 0, 0 );
		}
	}
	trap_R_SetColor( NULL );
}

/*
CG_OSPDrawCenterString: id's centre print, cut into lines of at most 40 characters at a space,
drawn with a shadow, 1.5 times as high as wide
*/
void CG_OSPDrawCenterString( void ) {
	int		i;
	char	*start;
	int		space;
	char	*p;
	int		charWidth;
	char	linebuffer[1024];
	int		y;
	int		x;
	int		w;
	float	*color;

	if ( !cg.centerPrintTime ) {
		return;
	}
	color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}
	trap_R_SetColor( color );

	start = cg.centerPrint;
	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;
	charWidth = cg.centerPrintCharWidth;
	while ( *start ) {
		p = linebuffer;
		space = 0;
		for ( i = 0; i < 40; i++ ) {
			if ( start[i] == ' ' ) {
				space = i;
			}
			if ( !start[i] || start[i] == '\n' ) {
				space = i;
				break;
			}
			*p++ = start[i];
		}
		if ( space && space != i ) {
			linebuffer[space] = 0;
			start += space + 1;
		} else {
			start += i + 1;
			*p = 0;
		}
		w = charWidth * CG_DrawStrlen( linebuffer );
		x = ( SCREEN_WIDTH - w ) / 2;
		CG_OSPDrawStringWithShadow( x, y, linebuffer, charWidth, 1.5f * charWidth, color, 40 );
		y += 1.5f * charWidth;
	}
	trap_R_SetColor( NULL );
}

/*
CG_OSPDrawPowerups: id's powerup timers (sorted by the time left, blinking in their last 5 s,
the icon pulsing after a pickup), drawn upwards from y at x; returns the top
*/
float CG_OSPDrawPowerups( float x, float y ) {
	int				k;
	int				sortedTime[MAX_POWERUPS];
	int				sorted[MAX_POWERUPS];
	int				j;
	int				active;
	int				t;
	int				i;
	float			size;
	playerState_t	*ps;
	float			f;
	gitem_t			*item;
	int				color;
	static float	colors[2][4] = {
		{ 0.2f, 1.0f, 0.2f, 1.0f },
		{ 1.0f, 0.2f, 0.2f, 1.0f }
	};

	ps = &cg.snap->ps;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	// sort the list by time remaining
	active = 0;
	for ( i = 0; i < MAX_POWERUPS; i++ ) {
		if ( !ps->powerups[i] ) {
			continue;
		}
		t = ps->powerups[i] - cg.time;
		// ZOID--don't draw if the power up has unlimited time (999 seconds)
		// This is true of the CTF flags
		if ( t < 0 || t > 999000 ) {
			continue;
		}

		// insert into the list
		for ( j = 0; j < active; j++ ) {
			if ( sortedTime[j] >= t ) {
				for ( k = active - 1; k >= j; k-- ) {
					sorted[k + 1] = sorted[k];
					sortedTime[k + 1] = sortedTime[k];
				}
				break;
			}
		}
		sorted[j] = i;
		sortedTime[j] = t;
		active++;
	}

	// draw the icons and timers
	for ( i = 0; i < active; i++ ) {
		item = BG_FindItemForPowerup( sorted[i] );
		color = 1;

		y -= ICON_SIZE;

		trap_R_SetColor( colors[color] );
		CG_DrawField( x, y, 2, sortedTime[i] / 1000, 32, 48 );

		t = ps->powerups[sorted[i]];
		if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
			trap_R_SetColor( NULL );
		} else {
			vec4_t	modulate;

			f = (float)( t - cg.time ) / POWERUP_BLINK_TIME;
			f -= (int)f;
			modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
			trap_R_SetColor( modulate );
		}

		if ( cg.powerupActive == sorted[i] && cg.time - cg.powerupTime < PULSE_TIME ) {
			f = 1.0 - ( ( (float)cg.time - cg.powerupTime ) / PULSE_TIME );
			size = ICON_SIZE * ( 1.0 + ( PULSE_SCALE - 1.0 ) * f );
		} else {
			size = ICON_SIZE;
		}

		CG_DrawPic( x + 64, y + ICON_SIZE / 2 - size / 2, size, size, trap_R_RegisterShader( item->icon ) );
	}
	trap_R_SetColor( NULL );

	return y;
}

/*
CG_OSPDrawWeaponBar_2_3: ch_Weaponswitch 2 (fading after a weapon change) / 3: the weapon icons
at the right edge, centred between 64 and y, then the powerups below or beside them
*/
float CG_OSPDrawWeaponBar_2_3( float y ) {
	int		i;
	int		t;
	int		numPowerups;
	int		weapons;
	int		numWeapons;
	int		top;
	int		h;
	int		w;
	float	*color;

	CG_OSPGetClientFontSize( &cf_WeaponSwitch, &w, &h );

	numPowerups = 0;
	for ( i = 0; i < MAX_POWERUPS; i++ ) {
		if ( !cg.snap->ps.powerups[i] ) {
			continue;
		}
		t = cg.snap->ps.powerups[i] - cg.time;
		if ( t < 0 || t > 999000 ) {
			continue;
		}
		numPowerups++;
	}

	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	numWeapons = 0;
	for ( i = 1; i < MAX_WEAPONS; i++ ) {
		if ( weapons & ( 1 << i ) ) {
			numWeapons++;
		}
	}

	if ( ch_Weaponswitch.integer <= 1 || ch_Weaponswitch.integer >= 4 ) {
		return CG_OSPDrawPowerups( 528, y );
	}
	if ( ch_Weaponswitch.integer == 2 ) {
		color = CG_FadeColor( cg.weaponSelectTime, WEAPON_SELECT_TIME );
		if ( !color ) {
			return CG_OSPDrawPowerups( 528, y );
		}
	} else if ( ch_Weaponswitch.integer == 3 ) {
		color = colorWhite;
	} else {
		return y;
	}

	top = ( y - 64 ) / 2 + 64 - numWeapons * ( 5 * h / 8 );
	CG_OSPDrawWeaponSelect( 640 - 5 * w / 4, top, color );
	if ( top + numWeapons * ( 5 * h / 4 ) + 48 * numPowerups > y ) {
		CG_OSPDrawPowerups( 640 - 5 * w / 4 - 48 - 64, y );
	} else {
		CG_OSPDrawPowerups( 528, y );
	}
	return y;
}

/*
CG_OSPDrawWeaponBar_4_5: ch_Weaponswitch 4 (fading after a weapon change) / 5 (with the ammo):
the weapon icons at the left edge, centred between 64 and y
*/
void CG_OSPDrawWeaponBar_4_5( float y ) {
	int		weaponNum;
	int		h;
	float	top;
	float	x;
	int		w;
	int		weapons;
	int		cw;
	int		ch;
	int		numWeapons;
	float	*color;

	if ( ch_Weaponswitch.integer == 2 ) {
		cg.itemPickupTime = 0;
	}

	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	numWeapons = 0;
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weapons & ( 1 << weaponNum ) ) {
			numWeapons++;
		}
	}

	CG_OSPGetClientFontSize( &cf_WeaponSwitch, &w, &h );
	x = 6;
	top = ( y - 64 ) / 2 + 64 - numWeapons * ( h + 8 ) / 2;
	cw = 8;
	ch = 8;

	if ( ch_Weaponswitch.integer == 4 ) {
		color = CG_FadeColor( cg.weaponSelectTime, WEAPON_SELECT_TIME );
		if ( !color ) {
			return;
		}
	} else if ( ch_Weaponswitch.integer == 5 ) {
		color = colorWhite;
	} else {
		return;
	}

	trap_R_SetColor( color );
	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( !( weapons & ( 1 << weaponNum ) ) ) {
			continue;
		}
		CG_RegisterWeapon( weaponNum );
		CG_DrawPic( x, top, w, h, cg_weapons[weaponNum].weaponIcon );
		if ( weaponNum == cg.weaponSelect ) {
			CG_DrawPic( x - 4, top - 4, w + 8, h + 8, cgs.media.selectShader );
		}
		if ( !cg.snap->ps.ammo[weaponNum] ) {
			CG_DrawPic( x, top, w, h, cgs.media.noammoShader );
		}
		if ( ch_Weaponswitch.integer == 5 && cg.snap->ps.ammo[weaponNum] != -1 ) {
			CG_OSPDrawString( x + w + cw, top + ( ( h + 8 ) / 2 - ch ), va( "%3i", cg.snap->ps.ammo[weaponNum] ), cw, ch, NULL, 0, 0 );
		}
		top += h + 8;
	}
	trap_R_SetColor( NULL );
}

/*
CG_OSPDrawVote: id's vote line with the cf_Vote font size, centred at y 58
*/
void CG_OSPDrawVote( void ) {
	char	*s;
	int		w;
	int		sec;
	int		h;

	if ( !cgs.voteTime ) {
		return;
	}

	CG_OSPGetClientFontSize( &cf_Vote, &w, &h );

	// play a talk beep whenever it is modified
	if ( cgs.voteModified ) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
	}

	sec = ( VOTE_TIME - ( cg.time - cgs.voteTime ) ) / 1000;
	if ( sec < 0 ) {
		sec = 0;
	}
	s = va( "VOTE(%i):%s yes(F1):%i no(F2):%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo );
	CG_OSPDrawString( ( 0 < 320 - CG_OSPGetNormalizedStringSize( s ) * w / 2 ) ? 320 - CG_OSPGetNormalizedStringSize( s ) * w / 2 : 0,
		58, s, w, h, NULL, 0, 0 );
}

/*
CG_OSPDrawSpectatorInfo: "Following" and the followed player's name (cf_Following) at the top
while following (in demos: unless the player is a spectator); always returns qfalse
*/
qboolean CG_OSPDrawSpectatorInfo( void ) {
	const char	*str;
	int			w;
	int			h;

	if ( !cg.demoPlayback ) {
		if ( !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			return qfalse;
		}
		if ( cg.snap->ps.pm_type == PM_FREEZE || cg.intermissionStarted || cg.snap->ps.clientNum == cg.clientNum ) {
			return qfalse;
		}
	} else if ( cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_SPECTATOR ) {
		return qfalse;
	}

	CG_OSPGetClientFontSize( &cf_Following, &w, &h );
	str = "Following";
	CG_OSPDrawString( 320 - CG_OSPGetNormalizedStringSize( str ) * w / 2, 24, str, w, h, NULL, 0, 0 );
	str = cgs.clientinfo[cg.snap->ps.clientNum].name;
	CG_OSPDrawString( 320 - CG_OSPGetNormalizedStringSize( str ) * w / 2, h + 24, str, w, h, NULL, 0, 0 );
	return qfalse;
}

/*
CG_OSPHUDDrawScores: the OSP HUD's score boxes (cf_Scores size, cg_float), right-aligned above
y; with the Q3Comp status bars (ch_StatusbarFormat 9..12) they sit in the lower right corner, in
CTF with the flag status and, with ch_drawFlagNames, the carrier's name. Returns the top - 8.
*/
float CG_OSPHUDDrawScores( float y ) {
	static vec4_t	colors[3] = {
		{ 0.0f, 0.0f, 1.0f, 0.33f },
		{ 1.0f, 0.0f, 0.0f, 0.33f },
		{ 1.0f, 1.0f, 1.0f, 0.33f }
	};
	int			x;
	int			fontH;
	int			w;
	const char	*s;
	int			fontW;
	int			s1, s2;
	float		y1;
	vec4_t		color;
	int			score;
	int			floatX, floatY;
	gitem_t		*item;
	int			limit;
	int			i;
	clientInfo_t	*ci;
	int			tx, ty;
	qboolean	redTaken, blueTaken;
	int			wstr;

	CG_OSPGetClientFontSize( &cf_Scores, &fontW, &fontH );
	s1 = cgs.scores1;
	s2 = cgs.scores2;

	if ( cg_float.integer ) {
		floatY = 2 * sin( M_PI * 2 * ( ( cg.time / 6 ) % 360 ) / 360 );
		floatX = 2 * sin( ( M_PI * 2 * ( ( cg.time / 5 ) % 360 ) + 66 ) / 360 );
	} else {
		floatX = 0;
		floatY = 0;
	}

	if ( ch_StatusbarFormat.integer >= 9 && ch_StatusbarFormat.integer <= 12 ) {
		if ( cg_lagometer.integer != 1 && cg_drawTeamOverlay.integer != 2 ) {
			if ( cgs.gametype != GT_CTF || !ch_drawFlagNames.integer ) {
				x = 640;
				y = 480 - ( fontH + 8 );
			} else {
				int		sbH, sbW;

				CG_OSPGetClientFontSize( &cf_Statusbar, &sbW, &sbH );
				x = 640;
				y = 472 - sbH - fontH;
			}
		} else if ( cgs.gametype == GT_TEAM || cgs.gameTypeFreeze || CG_OSPIsGameTypeCA( cgs.gametype ) || ( cgs.gametype == GT_CTF && !ch_drawFlagNames.integer ) ) {
			x = 584;
			y = 480 - ( fontH + 8 );
		} else if ( cgs.gametype == GT_CTF ) {
			x = 640;
			y -= fontH + 8;
		} else {
			x = 640;
			y -= 8;
		}
	} else {
		x = 640;
		y -= fontH + 10;
	}

	y1 = y;

	if ( cgs.gametype >= GT_TEAM ) {
		s = !cgs.gameTypeFreeze ? va( "%2i", s2 ) : va( "(%i) %2i", cgs.osp_teamcount2, s2 );
		w = CG_DrawStrlen( s ) * fontW + 8;
		x += floatX - w - 2;
		y += floatY;
		if ( cgs.gameTypeFreeze && cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_BLUE ) {
			CG_OSPDrawPoly( x - 1, y - 5, w + 2, fontH + 8 + 2, colors[2] );
		}
		CG_OSPDrawPoly( x, y - 4, w, fontH + 8, colors[0] );
		if ( !cgs.gameTypeFreeze && cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
			CG_OSPDrawPic( x, y - 4, w, fontH + 8, cgs.media.selectShader );
		}
		CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );

		if ( cgs.gametype == GT_CTF ) {
			item = BG_FindItemForPowerup( PW_BLUEFLAG );
			if ( item ) {
				y1 = y - fontH - 8;
				if ( cgs.blueflag >= 0 && cgs.blueflag <= 2 ) {
					if ( !ch_drawFlagNames.integer ) {
						CG_DrawPic( x, y1 - 4, w, fontH + 8, cgs.media.blueFlagShader[cgs.blueflag] );
					} else {
						if ( ch_drawFlagNames.integer == 2 || ch_drawFlagNames.integer == 4 ) {
							x -= fontW * 2 + 8;
							CG_DrawPic( x, y - 4, w, fontH + 8, cgs.media.blueFlagShader[cgs.blueflag] );
						}
						switch ( cgs.blueflag ) {
						case 0:
							break;
						case 1:
							ci = NULL;
							blueTaken = qfalse;
							if ( cgs.ospFlagStatus ) {
								ci = &cgs.clientinfo[cgs.ospBlueflag];
								blueTaken = qtrue;
							} else {
								for ( i = 0; i < cgs.maxclients; i++ ) {
									ci = &cgs.clientinfo[i];
									if ( ci->powerups & ( 1 << PW_BLUEFLAG ) ) {
										blueTaken = qtrue;
										break;
									}
								}
							}
							if ( blueTaken && ci ) {
								wstr = CG_DrawStrlen( ci->name ) * fontW / 2 + 10;
								if ( wstr > 120 ) {
									wstr = 120;
								}
								tx = x - wstr;
								ty = y + 4;
								CG_OSPDrawStringWithShadow( tx, ty, ci->name, fontW / 2, 3 * fontH / 4, colorWhite, 15 );
							} else {
								tx = x - ( CG_DrawStrlen( "Taken" ) * fontW / 2 + 10 );
								ty = y + 4;
								CG_OSPDrawStringWithShadow( tx, ty, "Taken", fontW / 2, 3 * fontH / 4, colorWhite, 15 );
							}
							break;
						case 2:
							tx = x - ( 3 * ( CG_DrawStrlen( "dropped" ) * fontW ) / 4 + 10 );
							ty = y + 4;
							CG_OSPDrawStringWithShadow( tx, ty, "^xff8000^b^f^3DROPPED^F^7   FLAG", 3 * fontW / 4, 3 * fontH / 4, colorWhite, 15 );
							break;
						}
					}
				}
			}
		}

		s = !cgs.gameTypeFreeze ? va( "%2i", s1 ) : va( "(%i) %2i", cgs.osp_teamcount1, s1 );
		w = CG_DrawStrlen( s ) * fontW + 8;
		if ( cgs.gametype == GT_CTF && !ch_drawFlagNames.integer ) {
			x -= w;
		} else if ( cgs.gametype == GT_CTF && ( ch_drawFlagNames.integer == 2 || ch_drawFlagNames.integer == 4 ) ) {
			x += w;
			y -= fontH + 8;
		} else if ( ch_StatusbarFormat.integer > 9 || cgs.gametype == GT_CTF ) {
			y -= fontH + 8;
		} else {
			x -= w;
		}
		if ( cgs.gameTypeFreeze && cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_RED ) {
			CG_OSPDrawPoly( x - 1, y - 5, w + 2, fontH + 8 + 2, colors[2] );
		}
		CG_OSPDrawPoly( x, y - 4, w, fontH + 8, colors[1] );
		if ( !cgs.gameTypeFreeze && cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
			CG_OSPDrawPic( x, y - 4, w, fontH + 8, cgs.media.selectShader );
		}
		CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );

		if ( cgs.gametype == GT_CTF ) {
			item = BG_FindItemForPowerup( PW_REDFLAG );
			if ( item ) {
				y1 = y - fontH - 8;
				if ( cgs.redflag >= 0 && cgs.redflag <= 2 ) {
					if ( !ch_drawFlagNames.integer ) {
						CG_DrawPic( x, y1 - 4, w, fontH + 8, cgs.media.redFlagShader[cgs.redflag] );
					} else {
						if ( ch_drawFlagNames.integer == 2 || ch_drawFlagNames.integer == 4 ) {
							x -= fontW * 2 + 8;
							CG_DrawPic( x, y - 4, w, fontH + 8, cgs.media.redFlagShader[cgs.redflag] );
						}
						switch ( cgs.redflag ) {
						case 0:
							break;
						case 1:
							ci = NULL;
							redTaken = qfalse;
							if ( cgs.ospFlagStatus ) {
								ci = &cgs.clientinfo[cgs.ospRedflag];
								redTaken = qtrue;
							} else {
								for ( i = 0; i < cgs.maxclients; i++ ) {
									ci = &cgs.clientinfo[i];
									if ( ci->powerups & ( 1 << PW_REDFLAG ) ) {
										redTaken = qtrue;
										break;
									}
								}
							}
							if ( redTaken && ci ) {
								wstr = CG_DrawStrlen( ci->name ) * fontW / 2 + 10;
								if ( wstr > 120 ) {
									wstr = 120;
								}
								tx = x - wstr;
								ty = y + 4;
								CG_OSPDrawStringWithShadow( tx, ty, ci->name, fontW / 2, 3 * fontH / 4, colorWhite, 15 );
							} else {
								tx = x - ( CG_DrawStrlen( "Taken" ) * fontW / 2 + 10 );
								ty = y + 4;
								CG_OSPDrawStringWithShadow( tx, ty, "Taken", fontW / 2, 3 * fontH / 4, colorWhite, 15 );
							}
							break;
						case 2:
							tx = x - ( 3 * ( CG_DrawStrlen( "dropped" ) * fontW ) / 4 + 10 );
							ty = y + 4;
							CG_OSPDrawStringWithShadow( tx, ty, "^xff8000^b^f^3DROPPED^F^7   FLAG", 3 * fontW / 4, 3 * fontH / 4, colorWhite, 15 );
							break;
						}
					}
				}
			}
		}

		if ( ( cgs.gametype == GT_CTF && !ch_drawFlagNames.integer ) || ( cgs.gametype != GT_CTF && ch_StatusbarFormat.integer < 9 ) ) {
			if ( cgs.gametype == GT_CTF ) {
				limit = cgs.capturelimit;
			} else {
				limit = cgs.fraglimit;
			}
			if ( limit ) {
				s = va( "%2i", limit );
				w = CG_DrawStrlen( s ) * fontW + 8;
				x -= w;
				CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );
			}
		}
	} else {
		qboolean	spectator;

		x = 640;
		y -= fontH + 8;
		y1 = y;
		score = cg.snap->ps.persistant[PERS_SCORE];
		spectator = ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR );
		x += floatX - 2;
		y += floatY;

		// always show your score in the second box if not in first place
		if ( s1 != score && !spectator ) {
			s2 = score;
		}
		if ( s2 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s2 );
			w = CG_DrawStrlen( s ) * fontW + 8;
			x -= w;
			color[0] = 1.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = 0.33f;
			CG_OSPDrawPoly( x, y - 4, w, fontH + 8, color );
			if ( !spectator && score == s2 && score != s1 ) {
				CG_OSPDrawPic( x, y - 4, w, fontH + 8, cgs.media.selectShader );
			}
			CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );
		}

		// first place
		if ( s1 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s1 );
			w = CG_DrawStrlen( s ) * fontW + 8;
			x -= w;
			color[0] = 0.0f;
			color[1] = 0.0f;
			color[2] = 1.0f;
			color[3] = 0.33f;
			CG_OSPDrawPoly( x, y - 4, w, fontH + 8, color );
			if ( !spectator && score == s1 ) {
				CG_OSPDrawPic( x, y - 4, w, fontH + 8, cgs.media.selectShader );
			}
			CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );
		}

		if ( cgs.fraglimit ) {
			s = va( "%2i", cgs.fraglimit );
			w = CG_DrawStrlen( s ) * fontW + 8;
			x -= w;
			CG_OSPDrawStringWithShadow( x + 4, y, s, fontW, fontH, NULL, 0 );
		}
	}

	return y1 - 8;
}

// OSP: the ch_CrosshairColor names (unreferenced "white" twice: the
// first entry is the default); the colour pointers are q_math.c's
typedef struct {
	char	*name;
	float	*color;
} ospCrosshairColor_t;

ospCrosshairColor_t	ospCrosshairColors[] = {
	{ "white", colorWhite },
	{ "black", colorBlack },
	{ "red", colorRed },
	{ "green", colorGreen },
	{ "blue", colorBlue },
	{ "yellow", colorYellow },
	{ "magenta", colorMagenta },
	{ "cyan", colorCyan },
	{ "white", colorWhite },
	{ "grey", colorMdGrey },
	{ NULL, NULL }
};

/*
CG_OSPDrawCrosshair: id's CG_DrawCrosshair with ch_drawSpeed 2 (the speed above the crosshair),
cg_crosshairPulse, and ch_CrosshairColor: a bit mask (1 red, 2 green, 4 blue) or, through
CG_OSPCrosshairColor, a colour name / 0xRRGGBB; coloured crosshairs use the second set
(crosshair%c2), white ones the normal set
*/
void CG_OSPDrawCrosshair( void ) {
	float		w, h;
	float		f;
	float		x, y;
	int			ca;
	vec4_t		hcolor;
	qboolean	white;
	qhandle_t	hShader;

	ca = ch_CrosshairColor.integer;
	white = qfalse;

	if ( ch_drawSpeed.integer == 2 ) {
		char	*s;
		vec4_t	color = { 1.0f, 1.0f, 1.0f, 0.4f };

		s = va( "<%fups>", cg.xyspeed );
		h = 0.5f * cg_crosshairSize.value;
		CG_OSPDrawString( 320 - 5 * strlen( s ), 225 - h, s, 10, 10, color, 0, 0 );
	}

	if ( !cg_drawCrosshair.integer ) {
		return;
	}
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}
	if ( cg.renderingThirdPerson ) {
		return;
	}
	if ( cg.mvActive > 1 ) {
		return;
	}

	// set color based on health
	if ( cg_crosshairHealth.integer ) {
		CG_ColorForHealth( hcolor );
		trap_R_SetColor( hcolor );
	} else if ( ca > 0 ) {
		hcolor[0] = ( ca & 1 ) ? 1.0f : 0;
		hcolor[1] = ( ca & 2 ) ? 1.0f : 0;
		hcolor[2] = ( ca & 4 ) ? 1.0f : 0;
		hcolor[3] = 1.0f;
		trap_R_SetColor( hcolor );
	} else {
		white = CG_OSPCrosshairColor();
	}

	w = h = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( cg_crosshairPulse.integer && f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
		h *= ( 1 + f );
	}

	x = cg_crosshairX.integer;
	y = cg_crosshairY.integer;
	CG_AdjustFrom640( &x, &y, &w, &h );

	if ( white ) {
		hShader = cgs.media.crosshairShader[ cg_drawCrosshair.integer % NUM_CROSSHAIRS ];
	} else {
		hShader = cgs.media.crosshairShader45[ cg_drawCrosshair.integer % NUM_CROSSHAIRS ];
	}

	trap_R_DrawStretchPic( x + cg.refdef.x + 0.5 * ( cg.refdef.width - w ),
		y + cg.refdef.y + 0.5 * ( cg.refdef.height - h ),
		w, h, 0, 0, 1, 1, hShader );
	trap_R_SetColor( NULL );
}

#define OSP_IS_HEX( c )	( ( c ) && ( ( ( c ) >= '0' && ( c ) <= '9' ) || ( ( c ) >= 'A' && ( c ) <= 'F' ) || ( ( c ) >= 'a' && ( c ) <= 'f' ) ) )
#define OSP_HEX_VALUE( c )	( ( c ) > '9' ? ( ( c ) >= 'a' ? ( c ) - 'a' + 10 : ( c ) - '7' ) : ( c ) - '0' )

/*
CG_OSPCrosshairColor: sets the crosshair colour from ch_CrosshairColor (a name of
ospCrosshairColors or 0xRRGGBB; anything else resets the cvar to "White"), parsed once per
modificationCount. Returns qtrue for white (no colour set, or a colour of 0.999 or more in each
channel)
*/
int CG_OSPCrosshairColor( void ) {
	static qboolean	white = qtrue;
	static int		modificationCount = -1;
	static vec4_t	color = { 0, 0, 0, 0 };
	char	*s;

	if ( ch_CrosshairColor.modificationCount == modificationCount ) {
		trap_R_SetColor( color );
		return white;
	}
	s = ch_CrosshairColor.string;
	modificationCount = ch_CrosshairColor.modificationCount;
	white = qfalse;

	if ( s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) ) {
		s += 2;
		if ( !( OSP_IS_HEX( s[0] ) && OSP_IS_HEX( s[1] ) && OSP_IS_HEX( s[2] ) && OSP_IS_HEX( s[3] ) && OSP_IS_HEX( s[4] ) && OSP_IS_HEX( s[5] ) ) ) {
			trap_Cvar_Set( CG_OSPGetCvarName( &ch_CrosshairColor ), "White" );
			trap_R_SetColor( NULL );
			white = qtrue;
		} else {
			color[0] = ( OSP_HEX_VALUE( s[0] ) * 16 + OSP_HEX_VALUE( s[1] ) ) / 255.0f;
			color[1] = ( OSP_HEX_VALUE( s[2] ) * 16 + OSP_HEX_VALUE( s[3] ) ) / 255.0f;
			color[2] = ( OSP_HEX_VALUE( s[4] ) * 16 + OSP_HEX_VALUE( s[5] ) ) / 255.0f;
			color[3] = 1.0f;
			if ( color[0] > 0.999f && color[1] > 0.999f && color[2] > 0.999f ) {
				white = qtrue;
			}
			trap_R_SetColor( color );
		}
	} else {
		int		i;

		for ( i = 0; ospCrosshairColors[i].name; i++ ) {
			if ( !Q_stricmp( s, ospCrosshairColors[i].name ) ) {
				color[0] = ospCrosshairColors[i].color[0];
				color[1] = ospCrosshairColors[i].color[1];
				color[2] = ospCrosshairColors[i].color[2];
				color[3] = 1.0f;
				trap_R_SetColor( color );
				if ( !Q_stricmp( s, "White" ) ) {
					white = qtrue;
				}
				break;
			}
		}
		if ( !ospCrosshairColors[i].name ) {
			trap_R_SetColor( NULL );
			trap_Cvar_Set( CG_OSPGetCvarName( &ch_CrosshairColor ), "White" );
			color[0] = color[1] = color[2] = color[3] = 1.0f;
			white = qtrue;
		}
	}
	return white;
}

/*
CG_OSPHUDDrawFlag: the carried flag's model at x in the Q3Comp status bar (higher with
ch_drawHealthMeters)
*/
void CG_OSPHUDDrawFlag( float x, int team ) {
	if ( ch_drawHealthMeters.integer )
		CG_DrawFlagModel( x, 427, 48, 48, team, qfalse );
	else
		CG_DrawFlagModel( x, 432, 48, 48, team, qfalse );
}

/*
CG_OSPDrawQ3CompWeapon1: one weapon line of the Q3Comp weapon list (ch_StatusbarFormat 9 / 11):
an ammo bar in the weapon's colour (120 pixels for maxAmmo), the name and the ammo count; the
current weapon is framed
*/
void CG_OSPDrawQ3CompWeapon1( int x, int y, int weapon, int ammo, float *color, char *name, float maxAmmo ) {
	int			ty;
	vec4_t		c;
	centity_t	*cent;
	int			w;

	cent = &cg_entities[cg.snap->ps.clientNum];
	ty = y + 2;
	if ( cent->currentState.weapon == weapon ) {
		CG_DrawRect( x, y, 120, 12, 1, colorWhite );
		color[3] = 0.9f;
	} else {
		color[3] = 0.25f;
	}
	if ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) ) {
		if ( ammo ) {
			Vector4Copy( color, c );
			w = 120.0f / maxAmmo * ammo;
			if ( w > 120 ) {
				w = 120;
			}
		} else {
			Vector4Copy( colorBlack, c );
			w = 120;
		}
		CG_FillRect( x, y, w, 12, c );
	}
	CG_DrawStringExt( x + 1, ty, name, colorWhite, qfalse, qfalse, 8, 8, 18 );
	ty = y;
	CG_DrawStringExt( x + 120 - 12 * CG_DrawStrlen( va( "%d", ammo ) ), ty, va( "%d", ammo ), colorWhite, qfalse, qfalse, 12, 12, 18 );
}

/*
CG_OSPDrawQ3CompWeapon2: one weapon of the compact Q3Comp weapon list (ch_StatusbarFormat 12):
the icon and the ammo count in the weapon's colour; the current weapon is framed
*/
void CG_OSPDrawQ3CompWeapon2( int x, int y, int weapon, int ammo, float *color, char *name, float maxAmmo ) {
	centity_t	*cent;
	qhandle_t	icon;
	int			ty;

	cent = &cg_entities[cg.snap->ps.clientNum];
	ty = y + 2;
	if ( cent->currentState.weapon == weapon ) {
		CG_DrawRect( x - 1, y - 1, 54, 14, 1, colorWhite );
		color[3] = 1.0f;
	} else {
		color[3] = 0.7f;
	}
	if ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) ) {
		icon = cg_weapons[weapon].ammoIcon;
		if ( icon ) {
			CG_DrawPic( x, y, 12, 12, icon );
		}
	}
	CG_DrawStringExt( x + 16, y, va( "%d", ammo ), color, qfalse, qtrue, 12, 12, 18 );
}

// the Q3Comp status bar's lightning colour, also CG_OSPDrawDecalInfo's "OFF" lines
vec4_t	shaftColor = { 0.8f, 0.8f, 0.8f, 0.5f };

/*
CG_OSPDrawQ3CompStatusbar: the Q3Comp status bars (ch_StatusbarFormat 9..12): team background,
the 3D ammo model (10, 12), the carried flag, the ammo count, the weapon list (9, 11: bars, 12:
icons), health and armor as numbers or, with ch_drawHealthMeters, as bars
*/
void CG_OSPDrawQ3CompStatusbar( void ) {
	static vec4_t	colors[4] = {
		{ 1.0f, 0.69f, 0.0f, 1.0f },	// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing
		{ 1.0f, 1.0f, 1.0f, 1.0f }		// health over 100
	};
	int				posY;
	playerState_t	*ps;
	int				value;
	int				posX;
	vec4_t			color;
	vec4_t			railColor;
	vec3_t			angles;
	centity_t		*cent;
	int				wBar;
	int				w, h;
	int				wField;
	int				flagX;
	vec3_t			origin;
	int				colorIndex;

	if ( !cg_drawStatus.integer ) {
		return;
	}

	if ( ch_TeamBackground.integer ) {
		CG_DrawTeamBackground( 0, 420, 640, 60, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );
	}

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	VectorClear( angles );

	if ( ch_StatusbarFormat.integer == 10 || ch_StatusbarFormat.integer == 12 ) {
		if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
			origin[0] = 70;
			origin[1] = 0;
			origin[2] = 0;
			angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
			if ( ch_StatusbarFormat.integer == 10 ) {
				CG_Draw3DModel( 100, 432, 48, 48, cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
			} else {
				CG_Draw3DModel( 105, 432, 48, 48, cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
			}
		}
	}

	if ( ch_drawHealthMeters.integer ) {
		flagX = 415;
	} else {
		flagX = 350;
	}
	if ( cg.predictedPlayerState.powerups[PW_REDFLAG] ) {
		CG_OSPHUDDrawFlag( flagX, TEAM_RED );
	} else if ( cg.predictedPlayerState.powerups[PW_BLUEFLAG] ) {
		CG_OSPHUDDrawFlag( flagX, TEAM_BLUE );
	} else if ( cg.predictedPlayerState.powerups[PW_NEUTRALFLAG] ) {
		CG_OSPHUDDrawFlag( flagX, TEAM_FREE );
	}

	CG_OSPGetClientFontSize( &cf_Statusbar, &w, &h );
	wField = 3 * w;

	if ( ch_StatusbarFormat.integer != 9 ) {
		if ( cent->currentState.weapon ) {
			value = ps->ammo[cent->currentState.weapon];
			if ( value > -1 ) {
				if ( cg.predictedPlayerState.weaponstate == WEAPON_FIRING && cg.predictedPlayerState.weaponTime > 100 ) {
					colorIndex = 2;
				} else if ( value >= 0 ) {
					colorIndex = 0;
				} else {
					colorIndex = 1;
				}
				trap_R_SetColor( colors[colorIndex] );

				if ( ch_StatusbarFormat.integer == 10 ) {
					CG_DrawField( 96 - wField, 476 - h, 3, value, w, h );
					trap_R_SetColor( NULL );
					if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
						qhandle_t	icon;

						icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
						if ( icon ) {
							CG_DrawPic( wField + 4, 476 - h, h, w, icon );
						}
					}
				} else {
					if ( ch_StatusbarFormat.integer == 12 ) {
						if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
							qhandle_t	icon;

							icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
							if ( icon ) {
								CG_DrawPic( 111, 438, w, h, icon );
							}
						}
					}
					CG_DrawField( 236 - wField, 476 - h, 3, value, w, h );
					trap_R_SetColor( NULL );
				}
			}
		}
	}

	if ( ch_StatusbarFormat.integer == 9 ) {
		posX = 5;
		posY = 466;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_BFG, ps->ammo[WP_BFG], colorBlue, "BFG", 50 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_GRENADE_LAUNCHER, ps->ammo[WP_GRENADE_LAUNCHER], colorGreen, "Grenades", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_SHOTGUN, ps->ammo[WP_SHOTGUN], colorOrange, "Shotgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_MACHINEGUN, ps->ammo[WP_MACHINEGUN], colorYellow, "Machinegun", 100 );
		posX += 126;
		posY = 466;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_PLASMAGUN, ps->ammo[WP_PLASMAGUN], colorMagenta, "Plasma", 100 );
		posY -= 14;
		Vector4Copy( colorGreen, railColor );
		railColor[0] += 0.4f;
		railColor[3] = 0.5f;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_RAILGUN, ps->ammo[WP_RAILGUN], railColor, "Railgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_LIGHTNING, ps->ammo[WP_LIGHTNING], shaftColor, "Lightning", 200 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_ROCKET_LAUNCHER, ps->ammo[WP_ROCKET_LAUNCHER], colorRed, "Rockets", 20 );
	} else if ( ch_StatusbarFormat.integer == 11 ) {
		posX = 5;
		posY = 466;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_PLASMAGUN, ps->ammo[WP_PLASMAGUN], colorMagenta, "Plasma", 100 );
		posY -= 14;
		Vector4Copy( colorGreen, railColor );
		railColor[0] += 0.4f;
		railColor[3] = 0.5f;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_RAILGUN, ps->ammo[WP_RAILGUN], railColor, "Railgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_LIGHTNING, ps->ammo[WP_LIGHTNING], shaftColor, "Lightning", 200 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_ROCKET_LAUNCHER, ps->ammo[WP_ROCKET_LAUNCHER], colorRed, "Rockets", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_GRENADE_LAUNCHER, ps->ammo[WP_GRENADE_LAUNCHER], colorGreen, "Grenades", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_SHOTGUN, ps->ammo[WP_SHOTGUN], colorOrange, "Shotgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon1( posX, posY, WP_MACHINEGUN, ps->ammo[WP_MACHINEGUN], colorYellow, "Machinegun", 100 );
	} else if ( ch_StatusbarFormat.integer == 12 ) {
		posX = 5;
		posY = 466;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_BFG, ps->ammo[WP_BFG], colorBlue, "BFG", 50 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_GRENADE_LAUNCHER, ps->ammo[WP_GRENADE_LAUNCHER], colorGreen, "Grenades", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_SHOTGUN, ps->ammo[WP_SHOTGUN], colorOrange, "Shotgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_MACHINEGUN, ps->ammo[WP_MACHINEGUN], colorYellow, "Machinegun", 100 );
		posX += 54;
		posY = 466;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_PLASMAGUN, ps->ammo[WP_PLASMAGUN], colorMagenta, "Plasma", 100 );
		posY -= 14;
		Vector4Copy( colorGreen, railColor );
		railColor[0] += 0.4f;
		railColor[3] = 0.5f;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_RAILGUN, ps->ammo[WP_RAILGUN], railColor, "Railgun", 20 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_LIGHTNING, ps->ammo[WP_LIGHTNING], shaftColor, "Lightning", 200 );
		posY -= 14;
		CG_OSPDrawQ3CompWeapon2( posX, posY, WP_ROCKET_LAUNCHER, ps->ammo[WP_ROCKET_LAUNCHER], colorRed, "Rockets", 20 );
	}

	value = ps->stats[STAT_HEALTH];
	if ( ch_drawHealthMeters.integer ) {
		Vector4Copy( colorBlue, color );
		color[3] = 0.3f;
		if ( value > 100 ) {
			wBar = 150;
			CG_FillRect( 260, 424, wBar, 24, color );
			wBar = 1.5f * ( value - 100 );
			color[3] = 0.7f;
		} else if ( value < 0 ) {
			wBar = 0;
		} else {
			wBar = 1.5f * value;
		}
		CG_FillRect( 260, 424, wBar, 24, color );
		CG_DrawStringExt( 260, 430, "Health", colorWhite, qfalse, qfalse, 10, 12, 0 );
		CG_DrawStringExt( 410 - CG_DrawStrlen( va( "%d", value ) ) * 16, 426, va( "%d", value ), colorWhite, qfalse, qfalse, 16, 22, 0 );
	} else {
		if ( value > 100 ) {
			trap_R_SetColor( colors[3] );
		} else if ( value > 25 ) {
			trap_R_SetColor( colors[0] );
		} else if ( value > 0 ) {
			colorIndex = ( cg.time >> 8 ) & 1;
			trap_R_SetColor( colors[colorIndex] );
		} else {
			trap_R_SetColor( colors[1] );
		}
		CG_DrawField( 354 - wField, 476 - h, 3, value, w, h );
		CG_ColorForHealth( color );
		trap_R_SetColor( color );
	}

	value = ps->stats[STAT_ARMOR];
	if ( ch_drawHealthMeters.integer ) {
		Vector4Copy( colorGreen, color );
		color[3] = 0.3f;
		if ( value > 100 ) {
			wBar = 150;
			CG_FillRect( 260, 452, wBar, 24, color );
			wBar = 1.5f * ( value - 100 );
			color[3] = 0.7f;
		} else if ( value < 0 ) {
			wBar = 0;
		} else {
			wBar = 1.5f * value;
		}
		CG_FillRect( 260, 452, wBar, 24, color );
		CG_DrawStringExt( 260, 459, "Armor", colorWhite, qfalse, qfalse, 10, 12, 0 );
		CG_DrawStringExt( 410 - CG_DrawStrlen( va( "%d", value ) ) * 16, 454, va( "%d", value ), colorWhite, qfalse, qfalse, 16, 22, 0 );
	} else if ( value > 0 ) {
		trap_R_SetColor( colors[0] );
		CG_DrawField( 486 - wField, 476 - h, 3, value, w, h );
		if ( cgs.server_mode & 2 ) {
			qhandle_t	icon;

			icon = ( &cgs.media.armorIconGreen )[ ps->stats[STAT_ARMOR_TYPE] ];
			if ( icon ) {
				CG_DrawPic( 486 - wField + 3 * w + 3, 452, 24, 24, icon );
			}
		}
		trap_R_SetColor( NULL );
	}
}

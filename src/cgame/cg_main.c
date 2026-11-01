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
// cg_main.c -- initialization and primary entry point for cgame
#include "cg_local.h"

#ifdef MISSIONPACK
#include "../ui/ui_shared.h"
// display context for new ui stuff
displayContextDef_t cgDC;
#endif

int CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum );	// OSP returns 0
void CG_Shutdown( void );


/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {

	switch ( command ) {
	case CG_INIT:
		return CG_Init( arg0, arg1, arg2 );
	case CG_SHUTDOWN:
		CG_Shutdown();
		return 0;
	case CG_CONSOLE_COMMAND:
		return CG_ConsoleCommand();
	case CG_DRAW_ACTIVE_FRAME:
		CG_DrawActiveFrame( arg0, arg1, arg2 );
		return 0;
	case CG_CROSSHAIR_PLAYER:
		return CG_CrosshairPlayer();
	case CG_LAST_ATTACKER:
		return CG_LastAttacker();
	case CG_KEY_EVENT:
		CG_KeyEvent(arg0, arg1);
		return 0;
	case CG_MOUSE_EVENT:
#ifdef MISSIONPACK
		cgDC.cursorx = cgs.cursorX;
		cgDC.cursory = cgs.cursorY;
#endif
		CG_MouseEvent(arg0, arg1);
		return 0;
	case CG_EVENT_HANDLING:
		CG_EventHandling(arg0);
		return 0;
	default:
		CG_Error( "vmMain: unknown command %i", command );
		break;
	}
	return -1;
}


// OSP: defined after vmMain in OSP
int forceModelModificationCount = -1;

cg_t				cg;
cgs_t				cgs;
centity_t			cg_entities[MAX_GENTITIES];
weaponInfo_t		cg_weapons[MAX_WEAPONS];
itemInfo_t			cg_items[MAX_ITEMS];


// OSP's cvars; the order of the externs in cg_local.h sets their layout. The last five
// have no extern (they are the lowest cg_main.c cvars).
vmCvar_t	osp_client;
vmCvar_t	cg_centertime;
vmCvar_t	cg_runpitch;
vmCvar_t	cg_runroll;
vmCvar_t	cg_bobup;
vmCvar_t	cg_bobpitch;
vmCvar_t	cg_bobroll;
vmCvar_t	cg_swingSpeed;
vmCvar_t	cg_shadows;
vmCvar_t	cg_gibs;
vmCvar_t	cg_drawTimer;
vmCvar_t	cg_drawFPS;
vmCvar_t	cg_drawPing;
vmCvar_t	cg_drawSnapshot;
vmCvar_t	cg_draw3dIcons;
vmCvar_t	cg_drawIcons;
vmCvar_t	cg_drawAmmoWarning;
vmCvar_t	cg_drawCrosshair;
vmCvar_t	cg_drawCrosshairNames;
vmCvar_t	cg_drawRewards;
vmCvar_t	cg_drawTeamOverlay;
vmCvar_t	cg_teamOverlayUserinfo;
vmCvar_t	cg_crosshairX;
vmCvar_t	cg_crosshairY;
vmCvar_t	cg_crosshairSize;
vmCvar_t	cg_crosshairHealth;
vmCvar_t	cg_drawStatus;
vmCvar_t	cg_draw2D;
vmCvar_t	cg_animSpeed;
vmCvar_t	cg_debugAnim;
vmCvar_t	cg_debugPosition;
vmCvar_t	cg_debugEvents;
vmCvar_t	cg_railTrailTime;
vmCvar_t	cg_errorDecay;
vmCvar_t	cg_nopredict;
vmCvar_t	cg_noPlayerAnims;
vmCvar_t	cg_showmiss;
vmCvar_t	cg_footsteps;
vmCvar_t	cg_addMarks;
vmCvar_t	cg_brassTime;
vmCvar_t	cg_gun_frame;
vmCvar_t	cg_gun_x;
vmCvar_t	cg_gun_y;
vmCvar_t	cg_gun_z;
vmCvar_t	cg_drawGun;
vmCvar_t	cg_viewsize;
vmCvar_t	cg_tracerChance;
vmCvar_t	cg_tracerWidth;
vmCvar_t	cg_tracerLength;
vmCvar_t	cg_autoswitch;
vmCvar_t	cg_ignore;
vmCvar_t	cg_simpleItems;
vmCvar_t	cg_fov;
vmCvar_t	cg_zoomFov;
vmCvar_t	cg_thirdPersonRange;
vmCvar_t	cg_thirdPersonAngle;
vmCvar_t	cg_thirdPerson;
vmCvar_t	cg_stereoSeparation;
vmCvar_t	cg_lagometer;
vmCvar_t	cg_drawAttacker;
vmCvar_t	cg_synchronousClients;
vmCvar_t	cg_teamChatTime;
vmCvar_t	cg_teamChatHeight;
vmCvar_t	cg_stats;
vmCvar_t	cg_forceModel;
vmCvar_t	cg_buildScript;
vmCvar_t	cg_paused;
vmCvar_t	cg_blood;
vmCvar_t	cg_predictItems;
vmCvar_t	cg_deferPlayers;
vmCvar_t	cg_drawFriend;
vmCvar_t	cg_teamChatsOnly;
vmCvar_t	cg_noVoiceChats;
vmCvar_t	cg_noVoiceText;
vmCvar_t	cg_scorePlum;
vmCvar_t	cg_smoothClients;
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
vmCvar_t	cg_cameraOrbit;
vmCvar_t	cg_cameraOrbitDelay;
vmCvar_t	cg_timescaleFadeEnd;
vmCvar_t	cg_timescaleFadeSpeed;
vmCvar_t	cg_timescale;
vmCvar_t	cg_cameraMode;
vmCvar_t	cg_smallFont;
vmCvar_t	cg_bigFont;
vmCvar_t	cg_noTaunt;
vmCvar_t	cg_noProjectileTrail;
vmCvar_t	cg_oldRail;
vmCvar_t	cg_oldPlasma;
vmCvar_t	cg_oldRocket;
vmCvar_t	cg_enableBreath;
vmCvar_t	cg_altGrenades;
vmCvar_t	cg_altLightning;
vmCvar_t	cg_altPlasma;
vmCvar_t	cg_ammoCheck;
vmCvar_t	cg_autoAction;
vmCvar_t	cg_crosshairPulse;
vmCvar_t	cg_Customloc;
vmCvar_t	cg_damageDraw;
vmCvar_t	cg_damageKick;
vmCvar_t	cg_deadBodyFilter;
vmCvar_t	cg_enableOSPHUD;
vmCvar_t	cg_enemyColors;
vmCvar_t	cg_enemyModel;
vmCvar_t	cg_execVstr;
vmCvar_t	cg_fallKick;
vmCvar_t	cg_float;
vmCvar_t	cg_followkiller;
vmCvar_t	cg_followpowerup;
vmCvar_t	cg_followviewcam;
vmCvar_t	cg_forceColors;
vmCvar_t	cg_lightningImpact;
vmCvar_t	cg_MaxlocationWidth;
vmCvar_t	cg_muzzleFlash;
vmCvar_t	cg_nochatbeep;
vmCvar_t	cg_NoMOTDDisplayDuringDemo;
vmCvar_t	cg_nomip;
vmCvar_t	cg_oldCTFSounds;
vmCvar_t	cg_oversampleMouse;
vmCvar_t	cg_q3compScoreboard;
vmCvar_t	cg_scoreTransparency;
vmCvar_t	cg_showPlayerLean;
vmCvar_t	cg_smoke_sg;
vmCvar_t	cg_smokegrowth_gl;
vmCvar_t	cg_smokegrowth_rl;
vmCvar_t	cg_smokeradius_gl;
vmCvar_t	cg_smokeradius_rl;
vmCvar_t	cg_statScrollTime;
vmCvar_t	cg_swapSkins;
vmCvar_t	cg_trueLightning;
vmCvar_t	cg_teamRails;
vmCvar_t	cg_useScreenShotJPEG;
vmCvar_t	ch_3waveFont;
vmCvar_t	ch_ColorLocations;
vmCvar_t	ch_CrosshairColor;
vmCvar_t	cf_CrosshairNames;
vmCvar_t	ch_CrosshairNamesLeft;
vmCvar_t	ch_drawFlagNames;
vmCvar_t	ch_drawHealthMeters;
vmCvar_t	ch_drawSpeed;
vmCvar_t	ch_TeamBackground;
vmCvar_t	ch_FilterLocationsTeamchat;
vmCvar_t	cf_Following;
vmCvar_t	cf_Fragmsg;
vmCvar_t	ch_InverseTeamChat;
vmCvar_t	ch_Obituaries;
vmCvar_t	ch_Pickupitem;
vmCvar_t	cf_Pickupstring;
vmCvar_t	ch_recordMessage;
vmCvar_t	cf_Scores;
vmCvar_t	cf_Statusbar;
vmCvar_t	ch_StatusbarFormat;
vmCvar_t	ch_TeamchatOnly;
vmCvar_t	cf_Teamchat;
vmCvar_t	ch_TeamCrosshairHealth;
vmCvar_t	cf_TeamCrosshairHealth;
vmCvar_t	ch_Teamoverlay;
vmCvar_t	cf_Teamoverlay;
vmCvar_t	cf_Vote;
vmCvar_t	cf_WeaponSwitch;
vmCvar_t	cf_WeaponName;
vmCvar_t	ch_Weaponswitch;
vmCvar_t	cf_AmmoStatusbar;
vmCvar_t	s_ambient;
vmCvar_t	cl_maxpackets;
vmCvar_t	r_shownormals;
vmCvar_t	r_showtris;
vmCvar_t	snaps;
vmCvar_t	cl_timenudge;
vmCvar_t	r_znear;
vmCvar_t	com_maxfps;
vmCvar_t	m_pitch;
vmCvar_t	r_fullbright;
vmCvar_t	r_lodCurveError;
vmCvar_t	r_subdivisions;
vmCvar_t	cg_pmove_msec;
vmCvar_t	cg_clientLog;
vmCvar_t	cg_drawDecals;
vmCvar_t	osp_unk_fb0c;
vmCvar_t	handicap;

#ifdef MISSIONPACK
vmCvar_t 	cg_redTeamName;
vmCvar_t 	cg_blueTeamName;
vmCvar_t	cg_currentSelectedPlayer;
vmCvar_t	cg_currentSelectedPlayerName;
vmCvar_t	cg_singlePlayer;
vmCvar_t	cg_enableDust;

vmCvar_t	cg_singlePlayerActive;
vmCvar_t	cg_recordSPDemo;
vmCvar_t	cg_recordSPDemoName;
vmCvar_t	cg_obeliskRespawnDelay;
#endif

// OSP's entries have a fifth int (0 in every entry: the
// modificationCount CG_UpdateCvars compares); the table is not static: cg_osp_main.c walks it
// with id's 16-byte cvarTable_t (its own declaration).
typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
	int			modificationCount;	// OSP: last seen vmCvar->modificationCount (CG_UpdateCvars)
} cvarTable_t;

// OSP's table (189 entries of 20 bytes)
cvarTable_t cvarTable[] = {
	{ &osp_client, "osp_client", "1008", CVAR_USERINFO, 0 },
	{ &osp_unk_fb0c, "", "0", CVAR_CHEAT, 0 },
	{ &cg_ignore, "cg_ignore", "0", 0, 0 },
	{ &cg_autoswitch, "cg_autoswitch", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawGun, "cg_drawGun", "1", CVAR_ARCHIVE, 0 },
	{ &cg_zoomFov, "cg_zoomfov", "22.5", CVAR_ARCHIVE, 0 },
	{ &cg_fov, "cg_fov", "90", CVAR_ARCHIVE, 0 },
	{ &cg_viewsize, "cg_viewsize", "100", CVAR_ARCHIVE, 0 },
	{ &cg_stereoSeparation, "cg_stereoSeparation", "0.4", CVAR_ARCHIVE, 0 },
	{ &cg_shadows, "cg_shadows", "1", CVAR_ARCHIVE, 0 },
	{ &cg_gibs, "cg_gibs", "1", CVAR_ARCHIVE, 0 },
	{ &cg_draw2D, "cg_draw2D", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawStatus, "cg_drawStatus", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawTimer, "cg_drawTimer", "0", CVAR_ARCHIVE, 0 },
	{ &cg_drawFPS, "cg_drawFPS", "0", CVAR_ARCHIVE, 0 },
	{ &cg_drawSnapshot, "cg_drawSnapshot", "0", CVAR_ARCHIVE, 0 },
	{ &cg_draw3dIcons, "cg_draw3dIcons", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawIcons, "cg_drawIcons", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawAmmoWarning, "cg_drawAmmoWarning", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawAttacker, "cg_drawAttacker", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawCrosshair, "cg_drawCrosshair", "4", CVAR_ARCHIVE, 0 },
	{ &cg_drawCrosshairNames, "cg_drawCrosshairNames", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawRewards, "cg_drawRewards", "1", CVAR_ARCHIVE, 0 },
	{ &cg_crosshairSize, "cg_crosshairSize", "24", CVAR_ARCHIVE, 0 },
	{ &cg_crosshairHealth, "cg_crosshairHealth", "1", CVAR_ARCHIVE, 0 },
	{ &cg_crosshairX, "cg_crosshairX", "0", CVAR_ARCHIVE, 0 },
	{ &cg_crosshairY, "cg_crosshairY", "0", CVAR_ARCHIVE, 0 },
	{ &cg_brassTime, "cg_brassTime", "2500", CVAR_ARCHIVE, 0 },
	{ &cg_simpleItems, "cg_simpleItems", "0", CVAR_ARCHIVE, 0 },
	{ &cg_addMarks, "cg_marks", "1", CVAR_ARCHIVE, 0 },
	{ &cg_lagometer, "cg_lagometer", "1", CVAR_ARCHIVE, 0 },
	{ &cg_railTrailTime, "cg_railTrailTime", "400", CVAR_ARCHIVE, 0 },
	{ &cg_gun_x, "cg_gunX", "0", CVAR_CHEAT, 0 },
	{ &cg_gun_y, "cg_gunY", "0", CVAR_CHEAT, 0 },
	{ &cg_gun_z, "cg_gunZ", "0", CVAR_CHEAT, 0 },
	{ &cg_centertime, "cg_centertime", "3", CVAR_CHEAT, 0 },
	{ &cg_runpitch, "cg_runpitch", "0.002", CVAR_ARCHIVE, 0 },
	{ &cg_runroll, "cg_runroll", "0.005", CVAR_ARCHIVE, 0 },
	{ &cg_bobup, "cg_bobup", "0.005", CVAR_ARCHIVE, 0 },
	{ &cg_bobpitch, "cg_bobpitch", "0.002", CVAR_ARCHIVE, 0 },
	{ &cg_bobroll, "cg_bobroll", "0.002", CVAR_ARCHIVE, 0 },
	{ &cg_swingSpeed, "cg_swingSpeed", "0.3", CVAR_CHEAT, 0 },
	{ &cg_animSpeed, "cg_animspeed", "1", CVAR_CHEAT, 0 },
	{ &cg_debugAnim, "cg_debuganim", "0", CVAR_CHEAT, 0 },
	{ &cg_debugPosition, "cg_debugposition", "0", CVAR_CHEAT, 0 },
	{ &cg_debugEvents, "cg_debugevents", "0", CVAR_CHEAT, 0 },
	{ &cg_errorDecay, "cg_errordecay", "100", 0, 0 },
	{ &cg_nopredict, "cg_nopredict", "0", 0, 0 },
	{ &cg_noPlayerAnims, "cg_noplayeranims", "0", CVAR_CHEAT, 0 },
	{ &cg_showmiss, "cg_showmiss", "0", 0, 0 },
	{ &cg_footsteps, "cg_footsteps", "1", CVAR_CHEAT, 0 },
	{ &cg_tracerChance, "cg_tracerchance", "0.4", CVAR_CHEAT, 0 },
	{ &cg_tracerWidth, "cg_tracerwidth", "1", CVAR_CHEAT, 0 },
	{ &cg_tracerLength, "cg_tracerlength", "100", CVAR_CHEAT, 0 },
	{ &cg_thirdPersonRange, "cg_thirdPersonRange", "40", CVAR_CHEAT, 0 },
	{ &cg_thirdPersonAngle, "cg_thirdPersonAngle", "0", CVAR_CHEAT, 0 },
	{ &cg_thirdPerson, "cg_thirdPerson", "0", 0, 0 },
	{ &cg_teamChatTime, "cg_teamChatTime", "3000", CVAR_ARCHIVE, 0 },
	{ &cg_teamChatHeight, "cg_teamChatHeight", "0", CVAR_ARCHIVE, 0 },
	{ &cg_forceModel, "cg_forceModel", "0", CVAR_ARCHIVE, 0 },
	{ &cg_predictItems, "cg_predictItems", "1", CVAR_ARCHIVE, 0 },
	{ &cg_deferPlayers, "cg_deferPlayers", "1", CVAR_ARCHIVE, 0 },
	{ &cg_drawTeamOverlay, "cg_drawTeamOverlay", "0", CVAR_ARCHIVE, 0 },
	{ &cg_teamOverlayUserinfo, "teamoverlay", "0", CVAR_USERINFO | CVAR_ROM, 0 },
	{ &cg_stats, "cg_stats", "0", 0, 0 },
	{ &cg_drawFriend, "cg_drawFriend", "1", CVAR_ARCHIVE, 0 },
	{ &cg_teamChatsOnly, "cg_teamChatsOnly", "0", CVAR_ARCHIVE, 0 },
	{ &cg_noVoiceChats, "cg_noVoiceChats", "0", CVAR_ARCHIVE, 0 },
	{ &cg_noVoiceText, "cg_noVoiceText", "0", CVAR_ARCHIVE, 0 },
	{ &cg_buildScript, "com_buildScript", "0", 0, 0 },
	{ &cg_paused, "cl_paused", "0", CVAR_ROM, 0 },
	{ &cg_blood, "com_blood", "1", CVAR_ARCHIVE, 0 },
	{ &cg_synchronousClients, "g_synchronousClients", "0", 0, 0 },
	{ &cg_cameraOrbit, "cg_cameraOrbit", "0", CVAR_CHEAT, 0 },
	{ &cg_cameraOrbitDelay, "cg_cameraOrbitDelay", "50", CVAR_ARCHIVE, 0 },
	{ &cg_timescaleFadeEnd, "cg_timescaleFadeEnd", "1", 0, 0 },
	{ &cg_timescaleFadeSpeed, "cg_timescaleFadeSpeed", "0", 0, 0 },
	{ &cg_timescale, "timescale", "1", 0, 0 },
	{ &cg_scorePlum, "cg_scorePlums", "1", CVAR_ARCHIVE | CVAR_USERINFO, 0 },
	{ &cg_smoothClients, "cg_smoothClients", "0", CVAR_ARCHIVE | CVAR_USERINFO, 0 },
	{ &cg_cameraMode, "com_cameraMode", "0", CVAR_CHEAT, 0 },
	{ &pmove_fixed, "pmove_fixed", "0", 0, 0 },
	{ &pmove_msec, "pmove_msec", "8", 0, 0 },
	{ &cg_noTaunt, "cg_noTaunt", "0", CVAR_ARCHIVE, 0 },
	{ &cg_noProjectileTrail, "cg_noProjectileTrail", "0", CVAR_ARCHIVE, 0 },
	{ &cg_smallFont, "ui_smallFont", "0.25", CVAR_ARCHIVE, 0 },
	{ &cg_bigFont, "ui_bigFont", "0.4", CVAR_ARCHIVE, 0 },
	{ &cg_oldRail, "cg_oldRail", "1", CVAR_ARCHIVE, 0 },
	{ &cg_oldRocket, "cg_oldRocket", "1", CVAR_ARCHIVE, 0 },
	{ &cg_oldPlasma, "cg_oldPlasma", "1", CVAR_ARCHIVE, 0 },
	{ &cg_altGrenades, "cg_altGrenades", "0", CVAR_ARCHIVE, 0 },
	{ &cg_altLightning, "cg_altLightning", "0", CVAR_ARCHIVE, 0 },
	{ &cg_altPlasma, "cg_altPlasma", "0", CVAR_ARCHIVE, 0 },
	{ &cg_ammoCheck, "cg_ammoCheck", "0", CVAR_ARCHIVE, 0 },
	{ &cg_autoAction, "cg_autoAction", "0", CVAR_ARCHIVE, 0 },
	{ &cg_clientLog, "cg_clientLog", "0", CVAR_ARCHIVE, 0 },
	{ &cg_crosshairPulse, "cg_crosshairPulse", "1", CVAR_ARCHIVE, 0 },
	{ &cg_Customloc, "cg_Customloc", "0", CVAR_ARCHIVE | CVAR_LATCH, 0 },
	{ &cg_damageDraw, "cg_damageDraw", "1", CVAR_ARCHIVE, 0 },
	{ &cg_damageKick, "cg_damageKick", "1", CVAR_ARCHIVE, 0 },
	{ &cg_deadBodyFilter, "cg_deadBodyFilter", "0", CVAR_ARCHIVE, 0 },
	{ &cg_drawDecals, "cg_drawDecals", "1", CVAR_ARCHIVE | CVAR_LATCH, 0 },
	{ &cg_drawPing, "cg_drawPing", "0", CVAR_ARCHIVE, 0 },
	{ &cg_enableOSPHUD, "cg_enableOSPHUD", "1", CVAR_ARCHIVE, 0 },
	{ &cg_enableBreath, "cg_enableBreath", "1", 0, 0 },
	{ &cg_enemyColors, "cg_enemyColors", "0", CVAR_ARCHIVE, 0 },
	{ &cg_enemyModel, "cg_enemyModel", "", CVAR_ARCHIVE, 0 },
	{ &cg_execVstr, "cg_execVstr", "", CVAR_ARCHIVE, 0 },
	{ &cg_fallKick, "cg_fallKick", "1", CVAR_ARCHIVE, 0 },
	{ &cg_float, "cg_float", "0", CVAR_ARCHIVE, 0 },
	{ &cg_followkiller, "cg_followkiller", "0", CVAR_ARCHIVE, 0 },
	{ &cg_followpowerup, "cg_followpowerup", "0", CVAR_ARCHIVE, 0 },
	{ &cg_followviewcam, "cg_followviewcam", "1", CVAR_ARCHIVE, 0 },
	{ &cg_forceColors, "cg_forceColors", "0", CVAR_ARCHIVE, 0 },
	{ &cg_lightningImpact, "cg_lightningImpact", "1", CVAR_ARCHIVE, 0 },
	{ &cg_MaxlocationWidth, "cg_MaxlocationWidth", "16", CVAR_ARCHIVE, 0 },
	{ &cg_muzzleFlash, "cg_muzzleFlash", "1", CVAR_ARCHIVE, 0 },
	{ &cg_nochatbeep, "cg_nochatbeep", "0", CVAR_ARCHIVE, 0 },
	{ &cg_NoMOTDDisplayDuringDemo, "cg_NoMOTDDisplayDuringDemo", "0", CVAR_ARCHIVE, 0 },
	{ &cg_nomip, "cg_nomip", "0", CVAR_ARCHIVE, 0 },
	{ &cg_oldCTFSounds, "cg_oldCTFSounds", "0", CVAR_ARCHIVE, 0 },
	{ &cg_oversampleMouse, "cg_oversampleMouse", "0", CVAR_ARCHIVE, 0 },
	{ &cg_statScrollTime, "cg_statScrollTime", "0.15", CVAR_ARCHIVE, 0 },
	{ &cg_q3compScoreboard, "cg_q3compScoreboard", "1", CVAR_ARCHIVE, 0 },
	{ &cg_scoreTransparency, "cg_scoreTransparency", "1.0", CVAR_ARCHIVE, 0 },
	{ &cg_showPlayerLean, "cg_showPlayerLean", "1", CVAR_ARCHIVE, 0 },
	{ &cg_smoke_sg, "cg_smoke_sg", "1", CVAR_ARCHIVE, 0 },
	{ &cg_smokegrowth_gl, "cg_smokegrowth_gl", "1", CVAR_ARCHIVE, 0 },
	{ &cg_smokegrowth_rl, "cg_smokegrowth_rl", "1", CVAR_ARCHIVE, 0 },
	{ &cg_smokeradius_gl, "cg_smokeradius_gl", "32", CVAR_ARCHIVE, 0 },
	{ &cg_smokeradius_rl, "cg_smokeradius_rl", "64", CVAR_ARCHIVE, 0 },
	{ &cg_swapSkins, "cg_swapSkins", "0", 0, 0 },
	{ &cg_teamRails, "cg_teamRails", "0", CVAR_ARCHIVE, 0 },
	{ &cg_trueLightning, "cg_trueLightning", "0.0", CVAR_ARCHIVE, 0 },
	{ &cg_useScreenShotJPEG, "cg_useScreenShotJPEG", "0", CVAR_ARCHIVE, 0 },
	{ &ch_3waveFont, "ch_3waveFont", "0", CVAR_ARCHIVE, 0 },
	{ &ch_ColorLocations, "ch_ColorLocations", "1", CVAR_ARCHIVE, 0 },
	{ &ch_CrosshairColor, "ch_CrosshairColor", "White", CVAR_ARCHIVE, 0 },
	{ &cf_CrosshairNames, "cf_CrosshairNames", "12x12", CVAR_ARCHIVE, 0 },
	{ &ch_CrosshairNamesLeft, "ch_CrosshairNamesLeft", "0", CVAR_ARCHIVE, 0 },
	{ &ch_crosshairTeamInfo, "ch_crosshairTeamInfo", "1", CVAR_ARCHIVE, 0 },
	{ &ch_drawHealthMeters, "ch_drawHealthMeters", "0", CVAR_ARCHIVE, 0 },
	{ &ch_drawFlagNames, "ch_drawFlagNames", "2", CVAR_ARCHIVE, 0 },
	{ &ch_drawPickup, "ch_drawPickup", "1", CVAR_ARCHIVE, 0 },
	{ &ch_drawSpeed, "ch_drawSpeed", "0", CVAR_ARCHIVE, 0 },
	{ &ch_TeamBackground, "ch_TeamBackground", "0", CVAR_ARCHIVE, 0 },
	{ &cf_Following, "cf_Following", "24x24", CVAR_ARCHIVE, 0 },
	{ &ch_FilterLocationsTeamchat, "ch_FilterLocationsTeamchat", "0", CVAR_ARCHIVE, 0 },
	{ &ch_fragMessage, "ch_fragMessage", "1", CVAR_ARCHIVE, 0 },
	{ &cf_Fragmsg, "cf_Fragmsg", "16x16", CVAR_ARCHIVE, 0 },
	{ &ch_graphs, "ch_graphs", "1", CVAR_ARCHIVE, 0 },
	{ &ch_InverseTeamChat, "ch_InverseTeamChat", "1", CVAR_ARCHIVE, 0 },
	{ &ch_Obituaries, "ch_Obituaries", "1", CVAR_ARCHIVE, 0 },
	{ &ch_Pickupitem, "ch_Pickupitem", "1", CVAR_ARCHIVE, 0 },
	{ &cf_Pickupstring, "cf_Pickupstring", "10x14", CVAR_ARCHIVE, 0 },
	{ &ch_recordMessage, "ch_recordMessage", "1", CVAR_ARCHIVE, 0 },
	{ &cf_Scores, "cf_Scores", "16x16", CVAR_ARCHIVE, 0 },
	{ &cf_Statusbar, "cf_Statusbar", "32x32", CVAR_ARCHIVE, 0 },
	{ &ch_StatusbarFormat, "ch_StatusbarFormat", "10", CVAR_ARCHIVE, 0 },
	{ &ch_statusbarStyle, "ch_statusbarStyle", "1", CVAR_ARCHIVE, 0 },
	{ &ch_teamBackground, "ch_teamBackground", "1", CVAR_ARCHIVE, 0 },
	{ &cf_Teamchat, "cf_Teamchat", "8x12", CVAR_ARCHIVE, 0 },
	{ &ch_TeamchatOnly, "ch_TeamchatOnly", "0", CVAR_ARCHIVE, 0 },
	{ &ch_TeamCrosshairHealth, "ch_TeamCrosshairHealth", "1", CVAR_ARCHIVE, 0 },
	{ &cf_TeamCrosshairHealth, "cf_TeamCrosshairHealth", "12x12", CVAR_ARCHIVE, 0 },
	{ &ch_Teamoverlay, "ch_Teamoverlay", "p12n h/aw l", CVAR_ARCHIVE, 0 },
	{ &cf_Teamoverlay, "cf_Teamoverlay", "6x11", CVAR_ARCHIVE, 0 },
	{ &cf_Vote, "cf_Vote", "10x14", CVAR_ARCHIVE, 0 },
	{ &ch_weaponList, "ch_weaponList", "1", CVAR_ARCHIVE, 0 },
	{ &ch_weaponListBackground, "ch_weaponListBackground", "1", CVAR_ARCHIVE, 0 },
	{ &ch_weaponListDrawAll, "ch_weaponListDrawAll", "0", CVAR_ARCHIVE, 0 },
	{ &ch_weaponListFlash, "ch_weaponListFlash", "1", CVAR_ARCHIVE, 0 },
	{ &ch_Weaponswitch, "ch_Weaponswitch", "4", CVAR_ARCHIVE, 0 },
	{ &cf_WeaponSwitch, "cf_WeaponSwitch", "32x32", CVAR_ARCHIVE, 0 },
	{ &cf_WeaponName, "cf_WeaponName", "12x12", CVAR_ARCHIVE, 0 },
	{ &cf_AmmoStatusbar, "cf_AmmoStatusbar", "8x8", CVAR_ARCHIVE, 0 },
	{ &s_ambient, "s_ambient", "1", CVAR_ARCHIVE, 0 },
	{ &cl_maxpackets, "cl_maxpackets", "30", CVAR_ARCHIVE, 0 },
	{ &cl_timenudge, "cl_timenudge", "0", CVAR_ARCHIVE, 0 },
	{ &snaps, "snaps", "20", CVAR_ARCHIVE, 0 },
	{ &r_shownormals, "r_shownormals", "0", CVAR_LATCH | CVAR_CHEAT, 0 },
	{ &r_fullbright, "r_fullbright", "0", CVAR_LATCH, 0 },
	{ &r_lodCurveError, "r_lodCurveError", "250", CVAR_LATCH, 0 },
	{ &r_showtris, "r_showtris", "0", 0, 0 },
	{ &r_subdivisions, "r_subdivisions", "4", CVAR_LATCH, 0 },
	{ &r_znear, "r_znear", "4", 0, 0 },
	{ &handicap, "handicap", "100", 0, 0 },
	{ &m_pitch, "m_pitch", "0.022", CVAR_ARCHIVE, 0 },
	{ &com_maxfps, "com_maxfps", "85", CVAR_ARCHIVE, 0 },
};

int		cvarTableSize = sizeof( cvarTable ) / sizeof( cvarTable[0] );

// OSP: an int initialised to 1; set by
// CG_UpdateCvars when a model / colour cvar or the followed client changes, then
// CG_NewClientInfo( cg.clientNum ) runs once a snapshot is there
int		osp_clientInfoChanged = 1;

/*
=================
CG_RegisterCvars
=================
*/
void CG_RegisterCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	char		var[MAX_TOKEN_CHARS];

	for ( i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );
		// OSP keeps each cvar's modificationCount in the table;
		// -2 makes CG_UpdateCvars send the first "teamoverlay"
		cv->modificationCount = cv->vmCvar->modificationCount;
		if ( cv->vmCvar == &cg_drawTeamOverlay ) {
			cv->modificationCount = -2;
		}
	}

	// see if we are also running the server on this machine
	trap_Cvar_VariableStringBuffer( "sv_running", var, sizeof( var ) );
	cgs.localServer = atoi( var ) ? qtrue : qfalse;

	// OSP
	trap_Cvar_Set( "ui_recordSPDemo", ch_recordMessage.integer > 0 ? "0" : "1" );

	trap_Cvar_Register(NULL, "model", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE );
	trap_Cvar_Register(NULL, "headmodel", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE );
	trap_Cvar_Register(NULL, "team_model", DEFAULT_TEAM_MODEL, CVAR_USERINFO | CVAR_ARCHIVE );
	trap_Cvar_Register(NULL, "team_headmodel", DEFAULT_TEAM_HEAD, CVAR_USERINFO | CVAR_ARCHIVE );

	CG_OSPCvarsRestrictValues();	// OSP
}

/*
=================
CG_UpdateCvars

OSP handles every changed cvar here (id: only the team overlay and
cg_forceModel after the loop): server-imposed limits, client info updates, restrictions
=================
*/
void CG_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean	restrictValues;

	restrictValues = qfalse;
	for ( i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++ ) {
		if ( !cv->vmCvar ) {
			continue;
		}
		trap_Cvar_Update( cv->vmCvar );
		if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
			cv->modificationCount = cv->vmCvar->modificationCount;

			// If team overlay is on, ask for updates from the server.  If its off,
			// let the server know so we don't receive it
			if ( cv->vmCvar == &cg_drawTeamOverlay ) {
				if ( cg_drawTeamOverlay.integer > 0 || ( !( cgs.server_mode & 1 ) && ch_TeamCrosshairHealth.integer > 0 ) ||
					( ( cgs.server_mode & 1 ) && ch_crosshairTeamInfo.integer > 0 ) ) {
					trap_Cvar_Set( "teamoverlay", "1" );
				} else {
					trap_Cvar_Set( "teamoverlay", "0" );
					CHS_Free( 6 );
				}
			}

			if ( cv->vmCvar == &cl_maxpackets ) {
				if ( cgs.serverConfigMinimumMaxpackets && cgs.serverConfigMinimumMaxpackets > cl_maxpackets.integer ) {
					CG_Printf( "^3Server-imposed minimum maxpackets set to %d.  Adjusting...\n", cgs.serverConfigMinimumMaxpackets );
					trap_Cvar_Set( "cl_maxpackets", va( "%d", cgs.serverConfigMinimumMaxpackets ) );
				} else if ( cgs.serverConfigMaximumMaxpackets && cgs.serverConfigMaximumMaxpackets < cl_maxpackets.integer ) {
					CG_Printf( "^3Server-imposed maximum maxpackets set to %d.  Adjusting...\n", cgs.serverConfigMaximumMaxpackets );
					trap_Cvar_Set( "cl_maxpackets", va( "%d", cgs.serverConfigMaximumMaxpackets ) );
				}
				if ( cgs.serverConfigMinimumMaxpackets > 0 && com_maxfps.integer > 0 && cgs.serverConfigMinimumMaxpackets > com_maxfps.integer ) {
					CG_Printf( "^3Server-imposed minimum maxfps set to %d.  Adjusting...\n", cgs.serverConfigMinimumMaxpackets );
					trap_Cvar_Set( "com_maxfps", va( "%d", cgs.serverConfigMinimumMaxpackets ) );
				}
				if ( !cgs.nextCvarsUpdateTime ) {
					cgs.nextCvarsUpdateTime = cg.time + 1200;
				}
			}

			if ( cv->vmCvar == &cl_timenudge ) {
				if ( cgs.serverConfigMinimumTimenudge && cgs.serverConfigMinimumTimenudge > cl_timenudge.integer ) {
					CG_Printf( "^3Server-imposed minimum timenudge set to %d.  Adjusting...\n", cgs.serverConfigMinimumTimenudge );
					trap_Cvar_Set( "cl_timenudge", va( "%d", cgs.serverConfigMinimumTimenudge ) );
				} else if ( cgs.serverConfigMaximumTimenudge && cgs.serverConfigMaximumTimenudge < cl_timenudge.integer ) {
					CG_Printf( "^3Server-imposed maximum timenudge set to %d.  Adjusting...\n", cgs.serverConfigMaximumTimenudge );
					trap_Cvar_Set( "cl_timenudge", va( "%d", cgs.serverConfigMaximumTimenudge ) );
				}
				if ( !cgs.nextCvarsUpdateTime ) {
					cgs.nextCvarsUpdateTime = cg.time + 1200;
				}
			}

			if ( cv->vmCvar == &com_maxfps ) {
				if ( cgs.serverConfigMinimumMaxpackets > 0 && com_maxfps.integer > 0 && cgs.serverConfigMinimumMaxpackets > com_maxfps.integer ) {
					CG_Printf( "^3Server-imposed minimum maxfps set to %d.  Adjusting...\n", cgs.serverConfigMinimumMaxpackets );
					trap_Cvar_Set( "com_maxfps", va( "%d", cgs.serverConfigMinimumMaxpackets ) );
				}
			}

			if ( cv->vmCvar == &ch_recordMessage && cg.snap ) {
				trap_Cvar_Set( "ui_recordSPDemo", ch_recordMessage.integer > 0 ? "0" : "1" );
			}

			if ( cg.snap && ( cv->vmCvar == &cg_forceModel || cv->vmCvar == &cg_forceColors ||
				cv->vmCvar == &cg_enemyModel || cv->vmCvar == &cg_swapSkins ) ) {
				osp_clientInfoChanged = qtrue;
			}

			if ( ( cv->vmCvar == &cg_followpowerup || cv->vmCvar == &cg_followkiller || cv->vmCvar == &cg_followviewcam ||
				cv->vmCvar == &cg_autoAction || cv->vmCvar == &snaps || cv->vmCvar == &cg_useScreenShotJPEG ) &&
				!cgs.nextCvarsUpdateTime ) {
				cgs.nextCvarsUpdateTime = cg.time + 1200;
			}

			if ( cv->vmCvar == &r_lodCurveError || cv->vmCvar == &r_subdivisions || cv->vmCvar == &r_znear ||
				cv->vmCvar == &cg_trueLightning || cv->vmCvar == &com_maxfps || cv->vmCvar == &r_shownormals ||
				cv->vmCvar == &r_showtris || cv->vmCvar == &cg_shadows || cv->vmCvar == &r_fullbright ) {
				restrictValues = qtrue;
			}

			if ( cv->vmCvar == &cg_enemyColors ) {
				if ( cg_enemyColors.string[0] ) {
					vec3_t	dummy;

					CG_OSPColorsFromString( cg_enemyColors.string, cgs.enemyRailColor, dummy, dummy, dummy );
				} else {
					VectorClear( cgs.enemyRailColor );
				}
				osp_clientInfoChanged = qtrue;
			}

			if ( cv->vmCvar == &handicap ) {
				if ( !cg.warmup && cgs.gametype != GT_FFA && cgs.gametype != GT_SINGLE_PLAYER ) {
					if ( forceModelModificationCount < 5 ) {
						forceModelModificationCount = 100;
					}
					trap_Cvar_Set( "handicap", va( "%d", forceModelModificationCount ) );
				} else {
					forceModelModificationCount = handicap.integer;
				}
			}

			if ( cv->vmCvar == &s_ambient && s_ambient.modificationCount > -1 ) {
				trap_SendConsoleCommand( "vid_restart;\n" );
			}

			if ( cv->vmCvar == &pmove_fixed ) {
				if ( !cgs.allow_pmove && pmove_fixed.integer ) {
					CG_Printf( "^3Enhanced client sampling has been disabled on this server.\n" );
					trap_Cvar_Set( "pmove_fixed", "0" );
				}
			}

			if ( cv->vmCvar == &cg_altPlasma || cv->vmCvar == &cg_altGrenades ) {
				if ( !( cgs.custom_client_2 & 4 ) && ( cg_altPlasma.integer || cg_altGrenades.integer ) ) {
					CG_Printf( "^3Alternate weapon graphics have been disabled on this server.\n" );
					trap_Cvar_Set( "cg_altPlasma", "0" );
					trap_Cvar_Set( "cg_altGrenades", "0" );
				}
			}

			if ( cv->vmCvar == &cg_enableOSPHUD ) {
				if ( !( cgs.custom_client_2 & 8 ) && cg_enableOSPHUD.integer ) {
					CG_Printf( "^3The OSP-based HUD has been disabled on this server.\n" );
					trap_Cvar_Set( "cg_enableOSPHUD", "0" );
				}
			}
		}
	}

	// check for modications here
	if ( cg.demoPlayback && cg.snap && cg.clientNum != cg.snap->ps.clientNum ) {
		cg.clientNum = cg.snap->ps.clientNum;
		osp_clientInfoChanged = qtrue;
	}

	if ( osp_clientInfoChanged && cg.snap ) {
		CG_NewClientInfo( cg.clientNum );
		CG_LoadDeferredPlayers();
		osp_clientInfoChanged = qfalse;
	}

	if ( restrictValues ) {
		CG_OSPCvarsRestrictValues();
	}
}

int CG_CrosshairPlayer( void ) {
	if ( cg.time > ( cg.crosshairClientTime + 1000 ) ) {
		return -1;
	}
	return cg.crosshairClientNum;
}

int CG_LastAttacker( void ) {
	if ( !cg.attackerTime ) {
		return -1;
	}
	return cg.snap->ps.persistant[PERS_ATTACKER];
}

// OSP's client log (cg_clientLog): every printed line with a time stamp
void CG_PrintLog( char *msg ) {
	if ( cgs.logFileHandle ) {
		char	*tmp;
		char	buffer[MAX_STRING_CHARS];

		CG_OSPNormalizeText( msg, strlen( msg ), buffer );
		tmp = va( "%.1f %s\n", (float)cg.time / 1000.0, buffer );
		trap_FS_Write( tmp, strlen( tmp ), cgs.logFileHandle );
	}
}


void QDECL CG_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof( text ), msg, argptr);
	va_end (argptr);

	CG_PrintLog( text );	// OSP
	trap_Print( text );
}

void QDECL CG_Error( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof( text ), msg, argptr);
	va_end (argptr);

	CG_PrintLog( text );	// OSP
	trap_Error( text );
}

#ifndef CGAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link (FIXME)

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	Q_vsnprintf (text, sizeof( text ), error, argptr);
	va_end (argptr);

	CG_Error( "%s", text);
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof( text ), msg, argptr);
	va_end (argptr);

	CG_Printf ("%s", text);
}

#endif

/*
================
CG_Argv
================
*/
const char *CG_Argv( int arg ) {
	static char	buffer[MAX_STRING_CHARS];

	trap_Argv( arg, buffer, sizeof( buffer ) );

	return buffer;
}


//========================================================================

/*
=================
CG_RegisterItemSounds

The server says this item is used on this level
=================
*/
static void CG_RegisterItemSounds( int itemNum ) {
	gitem_t			*item;
	char			data[MAX_QPATH];
	char			*s, *start;
	int				len;

	item = &bg_itemlist[ itemNum ];

	if( item->pickup_sound ) {
		trap_S_RegisterSound( item->pickup_sound, qfalse );
	}

	// parse the space seperated precache string for other media
	s = item->sounds;
	if (!s || !s[0])
		return;

	while (*s) {
		start = s;
		while (*s && *s != ' ') {
			s++;
		}

		len = s-start;
		if (len >= MAX_QPATH || len < 5) {
			CG_Error( "PrecacheItem: %s has bad precache string", 
				item->classname);
			return;
		}
		memcpy (data, start, len);
		data[len] = 0;
		if ( *s ) {
			s++;
		}

		if ( !strcmp(data+len-3, "wav" )) {
			trap_S_RegisterSound( data, qfalse );
		}
	}
}


/*
=================
CG_RegisterSounds

called during a precache command
=================
*/
static void CG_RegisterSounds( void ) {
	int		i;
	char	items[MAX_ITEMS+1];
	char	name[MAX_QPATH];
	const char	*soundName;

	// voice commands
#ifdef MISSIONPACK
	CG_LoadVoiceChats();
#endif

	cgs.media.oneMinuteSound = trap_S_RegisterSound( "sound/feedback/1_minute.wav", qtrue );
	cgs.media.fiveMinuteSound = trap_S_RegisterSound( "sound/feedback/5_minute.wav", qtrue );
	cgs.media.suddenDeathSound = trap_S_RegisterSound( "sound/feedback/sudden_death.wav", qtrue );
	cgs.media.oneFragSound = trap_S_RegisterSound( "sound/feedback/1_frag.wav", qtrue );
	cgs.media.twoFragSound = trap_S_RegisterSound( "sound/feedback/2_frags.wav", qtrue );
	cgs.media.threeFragSound = trap_S_RegisterSound( "sound/feedback/3_frags.wav", qtrue );
	cgs.media.count3Sound = trap_S_RegisterSound( "sound/feedback/three.wav", qtrue );
	cgs.media.count2Sound = trap_S_RegisterSound( "sound/feedback/two.wav", qtrue );
	cgs.media.count1Sound = trap_S_RegisterSound( "sound/feedback/one.wav", qtrue );
	cgs.media.countFightSound = trap_S_RegisterSound( "sound/feedback/fight.wav", qtrue );
	cgs.media.countPrepareSound = trap_S_RegisterSound( "sound/feedback/prepare.wav", qtrue );
#ifdef MISSIONPACK
	cgs.media.countPrepareTeamSound = trap_S_RegisterSound( "sound/feedback/prepare_team.wav", qtrue );
#endif

	if ( cgs.gametype >= GT_TEAM || cg_buildScript.integer ) {

		cgs.media.captureAwardSound = trap_S_RegisterSound( "sound/teamplay/flagcapture_yourteam.wav", qtrue );
		cgs.media.redLeadsSound = trap_S_RegisterSound( "sound/feedback/redleads.wav", qtrue );
		cgs.media.blueLeadsSound = trap_S_RegisterSound( "sound/feedback/blueleads.wav", qtrue );
		cgs.media.teamsTiedSound = trap_S_RegisterSound( "sound/feedback/teamstied.wav", qtrue );
		cgs.media.hitTeamSound = trap_S_RegisterSound( "sound/feedback/hit_teammate.wav", qtrue );

		cgs.media.redScoredSound = trap_S_RegisterSound( "sound/teamplay/voc_red_scores.wav", qtrue );
		cgs.media.blueScoredSound = trap_S_RegisterSound( "sound/teamplay/voc_blue_scores.wav", qtrue );

		cgs.media.captureYourTeamSound = trap_S_RegisterSound( "sound/teamplay/flagcapture_yourteam.wav", qtrue );
		cgs.media.captureOpponentSound = trap_S_RegisterSound( "sound/teamplay/flagcapture_opponent.wav", qtrue );

		cgs.media.returnYourTeamSound = trap_S_RegisterSound( "sound/teamplay/flagreturn_yourteam.wav", qtrue );
		cgs.media.returnOpponentSound = trap_S_RegisterSound( "sound/teamplay/flagreturn_opponent.wav", qtrue );

		cgs.media.takenYourTeamSound = trap_S_RegisterSound( "sound/teamplay/flagtaken_yourteam.wav", qtrue );
		cgs.media.takenOpponentSound = trap_S_RegisterSound( "sound/teamplay/flagtaken_opponent.wav", qtrue );

		if ( cgs.gametype == GT_CTF || cg_buildScript.integer ) {
			cgs.media.redFlagReturnedSound = trap_S_RegisterSound( "sound/teamplay/voc_red_returned.wav", qtrue );
			cgs.media.blueFlagReturnedSound = trap_S_RegisterSound( "sound/teamplay/voc_blue_returned.wav", qtrue );
			cgs.media.enemyTookYourFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_enemy_flag.wav", qtrue );
			cgs.media.yourTeamTookEnemyFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_team_flag.wav", qtrue );
		}

#ifdef MISSIONPACK
		if ( cgs.gametype == GT_1FCTF || cg_buildScript.integer ) {
			// FIXME: get a replacement for this sound ?
			cgs.media.neutralFlagReturnedSound = trap_S_RegisterSound( "sound/teamplay/flagreturn_opponent.wav", qtrue );
			cgs.media.yourTeamTookTheFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_team_1flag.wav", qtrue );
			cgs.media.enemyTookTheFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_enemy_1flag.wav", qtrue );
		}

		if ( cgs.gametype == GT_1FCTF || cgs.gametype == GT_CTF || cg_buildScript.integer ) {
			cgs.media.youHaveFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_you_flag.wav", qtrue );
			cgs.media.holyShitSound = trap_S_RegisterSound("sound/feedback/voc_holyshit.wav", qtrue);
		}

		if ( cgs.gametype == GT_OBELISK || cg_buildScript.integer ) {
			cgs.media.yourBaseIsUnderAttackSound = trap_S_RegisterSound( "sound/teamplay/voc_base_attack.wav", qtrue );
		}
#else
		cgs.media.youHaveFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_you_flag.wav", qtrue );
		cgs.media.holyShitSound = trap_S_RegisterSound("sound/feedback/voc_holyshit.wav", qtrue);
		cgs.media.neutralFlagReturnedSound = trap_S_RegisterSound( "sound/teamplay/flagreturn_opponent.wav", qtrue );
		cgs.media.yourTeamTookTheFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_team_1flag.wav", qtrue );
		cgs.media.enemyTookTheFlagSound = trap_S_RegisterSound( "sound/teamplay/voc_enemy_1flag.wav", qtrue );
#endif
	}

	cgs.media.tracerSound = trap_S_RegisterSound( "sound/weapons/machinegun/buletby1.wav", qfalse );
	cgs.media.selectSound = trap_S_RegisterSound( "sound/weapons/change.wav", qfalse );
	cgs.media.wearOffSound = trap_S_RegisterSound( "sound/items/wearoff.wav", qfalse );
	cgs.media.useNothingSound = trap_S_RegisterSound( "sound/items/use_nothing.wav", qfalse );
	cgs.media.gibSound = trap_S_RegisterSound( "sound/player/gibsplt1.wav", qfalse );
	cgs.media.gibBounce1Sound = trap_S_RegisterSound( "sound/player/gibimp1.wav", qfalse );
	cgs.media.gibBounce2Sound = trap_S_RegisterSound( "sound/player/gibimp2.wav", qfalse );
	cgs.media.gibBounce3Sound = trap_S_RegisterSound( "sound/player/gibimp3.wav", qfalse );

#ifdef MISSIONPACK
	cgs.media.useInvulnerabilitySound = trap_S_RegisterSound( "sound/items/invul_activate.wav", qfalse );
	cgs.media.invulnerabilityImpactSound1 = trap_S_RegisterSound( "sound/items/invul_impact_01.wav", qfalse );
	cgs.media.invulnerabilityImpactSound2 = trap_S_RegisterSound( "sound/items/invul_impact_02.wav", qfalse );
	cgs.media.invulnerabilityImpactSound3 = trap_S_RegisterSound( "sound/items/invul_impact_03.wav", qfalse );
	cgs.media.invulnerabilityJuicedSound = trap_S_RegisterSound( "sound/items/invul_juiced.wav", qfalse );
	cgs.media.obeliskHitSound1 = trap_S_RegisterSound( "sound/items/obelisk_hit_01.wav", qfalse );
	cgs.media.obeliskHitSound2 = trap_S_RegisterSound( "sound/items/obelisk_hit_02.wav", qfalse );
	cgs.media.obeliskHitSound3 = trap_S_RegisterSound( "sound/items/obelisk_hit_03.wav", qfalse );
	cgs.media.obeliskRespawnSound = trap_S_RegisterSound( "sound/items/obelisk_respawn.wav", qfalse );

	cgs.media.ammoregenSound = trap_S_RegisterSound("sound/items/cl_ammoregen.wav", qfalse);
	cgs.media.doublerSound = trap_S_RegisterSound("sound/items/cl_doubler.wav", qfalse);
	cgs.media.guardSound = trap_S_RegisterSound("sound/items/cl_guard.wav", qfalse);
	cgs.media.scoutSound = trap_S_RegisterSound("sound/items/cl_scout.wav", qfalse);
#endif

	cgs.media.teleInSound = trap_S_RegisterSound( "sound/world/telein.wav", qfalse );
	cgs.media.teleOutSound = trap_S_RegisterSound( "sound/world/teleout.wav", qfalse );
	cgs.media.respawnSound = trap_S_RegisterSound( "sound/items/respawn1.wav", qfalse );

	cgs.media.noAmmoSound = trap_S_RegisterSound( "sound/weapons/noammo.wav", qfalse );

	cgs.media.talkSound = trap_S_RegisterSound( "sound/player/talk.wav", qfalse );
	cgs.media.landSound = trap_S_RegisterSound( "sound/player/land1.wav", qfalse);

	// OSP's hit level sounds (CG_CheckLocalSounds)
	cgs.media.hitLowestSound = trap_S_RegisterSound( "sound/feedback/hitlowest.wav", qfalse );
	cgs.media.hitLowSound = trap_S_RegisterSound( "sound/feedback/hitlow.wav", qfalse );
	cgs.media.hitSound = trap_S_RegisterSound( "sound/feedback/hit.wav", qfalse );
	cgs.media.hitHighSound = trap_S_RegisterSound( "sound/feedback/hithigh.wav", qfalse );
#ifdef MISSIONPACK
	cgs.media.hitSoundHighArmor = trap_S_RegisterSound( "sound/feedback/hithi.wav", qfalse );
	cgs.media.hitSoundLowArmor = trap_S_RegisterSound( "sound/feedback/hitlo.wav", qfalse );
#endif

	cgs.media.impressiveSound = trap_S_RegisterSound( "sound/feedback/impressive.wav", qtrue );
	cgs.media.excellentSound = trap_S_RegisterSound( "sound/feedback/excellent.wav", qtrue );
	cgs.media.deniedSound = trap_S_RegisterSound( "sound/feedback/denied.wav", qtrue );
	cgs.media.humiliationSound = trap_S_RegisterSound( "sound/feedback/humiliation.wav", qtrue );
	cgs.media.assistSound = trap_S_RegisterSound( "sound/feedback/assist.wav", qtrue );
	cgs.media.defendSound = trap_S_RegisterSound( "sound/feedback/defense.wav", qtrue );
#ifdef MISSIONPACK
	cgs.media.firstImpressiveSound = trap_S_RegisterSound( "sound/feedback/first_impressive.wav", qtrue );
	cgs.media.firstExcellentSound = trap_S_RegisterSound( "sound/feedback/first_excellent.wav", qtrue );
	cgs.media.firstHumiliationSound = trap_S_RegisterSound( "sound/feedback/first_gauntlet.wav", qtrue );
#endif

	cgs.media.takenLeadSound = trap_S_RegisterSound( "sound/feedback/takenlead.wav", qtrue);
	cgs.media.tiedLeadSound = trap_S_RegisterSound( "sound/feedback/tiedlead.wav", qtrue);
	cgs.media.lostLeadSound = trap_S_RegisterSound( "sound/feedback/lostlead.wav", qtrue);

#ifdef MISSIONPACK
	cgs.media.voteNow = trap_S_RegisterSound( "sound/feedback/vote_now.wav", qtrue);
	cgs.media.votePassed = trap_S_RegisterSound( "sound/feedback/vote_passed.wav", qtrue);
	cgs.media.voteFailed = trap_S_RegisterSound( "sound/feedback/vote_failed.wav", qtrue);
#endif

	cgs.media.watrInSound = trap_S_RegisterSound( "sound/player/watr_in.wav", qfalse);
	cgs.media.watrOutSound = trap_S_RegisterSound( "sound/player/watr_out.wav", qfalse);
	cgs.media.watrUnSound = trap_S_RegisterSound( "sound/player/watr_un.wav", qfalse);

	cgs.media.jumpPadSound = trap_S_RegisterSound ("sound/world/jumppad.wav", qfalse );

	for (i=0 ; i<4 ; i++) {
		Com_sprintf (name, sizeof(name), "sound/player/footsteps/step%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_NORMAL][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/boot%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_BOOT][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/flesh%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_FLESH][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/mech%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_MECH][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/energy%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_ENERGY][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/splash%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_SPLASH][i] = trap_S_RegisterSound (name, qfalse);

		Com_sprintf (name, sizeof(name), "sound/player/footsteps/clank%i.wav", i+1);
		cgs.media.footsteps[FOOTSTEP_METAL][i] = trap_S_RegisterSound (name, qfalse);
	}

	// only register the items that the server says we need
	Q_strncpyz( items, CG_ConfigString( CS_ITEMS ), sizeof( items ) );

	for ( i = 1 ; i < bg_numItems ; i++ ) {
//		if ( items[ i ] == '1' || cg_buildScript.integer ) {
			CG_RegisterItemSounds( i );
//		}
	}

	for ( i = 1 ; i < MAX_SOUNDS ; i++ ) {
		soundName = CG_ConfigString( CS_SOUNDS+i );
		if ( !soundName[0] ) {
			break;
		}
		if ( soundName[0] == '*' ) {
			continue;	// custom sound
		}
		cgs.gameSounds[i] = trap_S_RegisterSound( soundName, qfalse );
	}

	// FIXME: only needed with item
	cgs.media.flightSound = trap_S_RegisterSound( "sound/items/flight.wav", qfalse );
	cgs.media.medkitSound = trap_S_RegisterSound ("sound/items/use_medkit.wav", qfalse);
	cgs.media.quadSound = trap_S_RegisterSound("sound/items/damage3.wav", qfalse);
	cgs.media.sfx_ric1 = trap_S_RegisterSound ("sound/weapons/machinegun/ric1.wav", qfalse);
	cgs.media.sfx_ric2 = trap_S_RegisterSound ("sound/weapons/machinegun/ric2.wav", qfalse);
	cgs.media.sfx_ric3 = trap_S_RegisterSound ("sound/weapons/machinegun/ric3.wav", qfalse);
	cgs.media.sfx_railg = trap_S_RegisterSound ("sound/weapons/railgun/railgf1a.wav", qfalse);
	cgs.media.sfx_rockexp = trap_S_RegisterSound ("sound/weapons/rocket/rocklx1a.wav", qfalse);
	cgs.media.sfx_plasmaexp = trap_S_RegisterSound ("sound/weapons/plasma/plasmx1a.wav", qfalse);
#ifdef MISSIONPACK
	cgs.media.sfx_proxexp = trap_S_RegisterSound( "sound/weapons/proxmine/wstbexpl.wav" , qfalse);
	cgs.media.sfx_nghit = trap_S_RegisterSound( "sound/weapons/nailgun/wnalimpd.wav" , qfalse);
	cgs.media.sfx_nghitflesh = trap_S_RegisterSound( "sound/weapons/nailgun/wnalimpl.wav" , qfalse);
	cgs.media.sfx_nghitmetal = trap_S_RegisterSound( "sound/weapons/nailgun/wnalimpm.wav", qfalse );
	cgs.media.sfx_chghit = trap_S_RegisterSound( "sound/weapons/vulcan/wvulimpd.wav", qfalse );
	cgs.media.sfx_chghitflesh = trap_S_RegisterSound( "sound/weapons/vulcan/wvulimpl.wav", qfalse );
	cgs.media.sfx_chghitmetal = trap_S_RegisterSound( "sound/weapons/vulcan/wvulimpm.wav", qfalse );
	cgs.media.weaponHoverSound = trap_S_RegisterSound( "sound/weapons/weapon_hover.wav", qfalse );
	cgs.media.kamikazeExplodeSound = trap_S_RegisterSound( "sound/items/kam_explode.wav", qfalse );
	cgs.media.kamikazeImplodeSound = trap_S_RegisterSound( "sound/items/kam_implode.wav", qfalse );
	cgs.media.kamikazeFarSound = trap_S_RegisterSound( "sound/items/kam_explode_far.wav", qfalse );
	cgs.media.winnerSound = trap_S_RegisterSound( "sound/feedback/voc_youwin.wav", qfalse );
	cgs.media.loserSound = trap_S_RegisterSound( "sound/feedback/voc_youlose.wav", qfalse );
	cgs.media.youSuckSound = trap_S_RegisterSound( "sound/misc/yousuck.wav", qfalse );

	cgs.media.wstbimplSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpl.wav", qfalse);
	cgs.media.wstbimpmSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpm.wav", qfalse);
	cgs.media.wstbimpdSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpd.wav", qfalse);
	cgs.media.wstbactvSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbactv.wav", qfalse);
#endif

	cgs.media.regenSound = trap_S_RegisterSound("sound/items/regen.wav", qfalse);
	cgs.media.protectSound = trap_S_RegisterSound("sound/items/protect3.wav", qfalse);
	cgs.media.n_healthSound = trap_S_RegisterSound("sound/items/n_health.wav", qfalse );
	cgs.media.hgrenb1aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb1a.wav", qfalse);
	cgs.media.hgrenb2aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb2a.wav", qfalse);

#ifdef MISSIONPACK
	trap_S_RegisterSound("sound/player/james/death1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/death2.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/death3.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/jump1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/pain25_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/pain75_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/pain100_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/falling1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/gasp.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/drown.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/fall1.wav", qfalse );
	trap_S_RegisterSound("sound/player/james/taunt.wav", qfalse );

	trap_S_RegisterSound("sound/player/janet/death1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/death2.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/death3.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/jump1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/pain25_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/pain75_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/pain100_1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/falling1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/gasp.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/drown.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/fall1.wav", qfalse );
	trap_S_RegisterSound("sound/player/janet/taunt.wav", qfalse );
#endif

}


//===================================================================================


/*
=================
CG_RegisterGraphics

This function may execute for a couple of minutes with a slow disk.
=================
*/
static void CG_RegisterGraphics( void ) {
	int			i;
	char		items[MAX_ITEMS+1];
	static char		*sb_nums[11] = {
		"gfx/2d/numbers/zero_32b",
		"gfx/2d/numbers/one_32b",
		"gfx/2d/numbers/two_32b",
		"gfx/2d/numbers/three_32b",
		"gfx/2d/numbers/four_32b",
		"gfx/2d/numbers/five_32b",
		"gfx/2d/numbers/six_32b",
		"gfx/2d/numbers/seven_32b",
		"gfx/2d/numbers/eight_32b",
		"gfx/2d/numbers/nine_32b",
		"gfx/2d/numbers/minus_32b",
	};

	// clear any references to old media
	memset( &cg.refdef, 0, sizeof( cg.refdef ) );
	trap_R_ClearScene();

	CG_LoadingString( cgs.mapname );

	trap_R_LoadWorldMap( cgs.mapname );

	// precache status bar pics
	CG_LoadingString( "game media" );

	for ( i=0 ; i<11 ; i++) {
		cgs.media.numberShaders[i] = trap_R_RegisterShader( sb_nums[i] );
	}

	// OSP registers the 2D pictures without mip maps
	cgs.media.botSkillShaders[0] = trap_R_RegisterShaderNoMip( "menu/art/skill1.tga" );
	cgs.media.botSkillShaders[1] = trap_R_RegisterShaderNoMip( "menu/art/skill2.tga" );
	cgs.media.botSkillShaders[2] = trap_R_RegisterShaderNoMip( "menu/art/skill3.tga" );
	cgs.media.botSkillShaders[3] = trap_R_RegisterShaderNoMip( "menu/art/skill4.tga" );
	cgs.media.botSkillShaders[4] = trap_R_RegisterShaderNoMip( "menu/art/skill5.tga" );

	cgs.media.viewBloodShader = trap_R_RegisterShader( "viewBloodBlend" );

	cgs.media.deferShader = trap_R_RegisterShaderNoMip( "gfx/2d/defer.tga" );

	cgs.media.scoreboardName = trap_R_RegisterShaderNoMip( "menu/tab/name.tga" );
	cgs.media.scoreboardPing = trap_R_RegisterShaderNoMip( "menu/tab/ping.tga" );
	cgs.media.scoreboardScore = trap_R_RegisterShaderNoMip( "menu/tab/score.tga" );
	cgs.media.scoreboardTime = trap_R_RegisterShaderNoMip( "menu/tab/time.tga" );

	// OSP's cg_nomip variants fall back to the normal shaders
	cgs.media.smokePuffShader = trap_R_RegisterShader( "smokePuff" );
	cgs.media.smokePuffNoPicMipShader = trap_R_RegisterShader( "smokePuffNoPicMip" );
	if ( !cgs.media.smokePuffNoPicMipShader ) {
		cgs.media.smokePuffNoPicMipShader = cgs.media.smokePuffShader;
	}
	cgs.media.smokePuffRageProShader = trap_R_RegisterShader( "smokePuffRagePro" );
	cgs.media.shotgunSmokePuffShader = trap_R_RegisterShader( "shotgunSmokePuff" );
	cgs.media.shotgunSmokePuffNoPicMipShader = trap_R_RegisterShader( "shotgunSmokePuffNoPicMip" );
	if ( !cgs.media.shotgunSmokePuffNoPicMipShader ) {
		cgs.media.shotgunSmokePuffNoPicMipShader = cgs.media.shotgunSmokePuffShader;
	}
#ifdef MISSIONPACK
	cgs.media.nailPuffShader = trap_R_RegisterShader( "nailtrail" );
	cgs.media.blueProxMine = trap_R_RegisterModel( "models/weaphits/proxmineb.md3" );
#endif
	cgs.media.plasmaBallShader = trap_R_RegisterShader( "sprites/plasma1" );
	cgs.media.plasmaBallNoPicMipShader = trap_R_RegisterShaderNoMip( "sprites/plasma1NoPicMip" );
	if ( !cgs.media.plasmaBallNoPicMipShader ) {
		cgs.media.plasmaBallNoPicMipShader = cgs.media.plasmaBallShader;
	}
	cgs.media.plasmaNewBallShader = trap_R_RegisterShader( "sprites/plasma1new" );
	if ( !cgs.media.plasmaNewBallShader ) {
		cgs.media.plasmaNewBallShader = cgs.media.plasmaBallShader;
	}
	cgs.media.plasmaNewBallNoPicMipShader = trap_R_RegisterShaderNoMip( "sprites/plasma1newNoPicMip" );
	if ( !cgs.media.plasmaNewBallNoPicMipShader ) {
		cgs.media.plasmaNewBallNoPicMipShader = cgs.media.plasmaNewBallShader;
	}
	// OSP's CPMA grenade (PROMODE, cg_altGrenades)
	cgs.media.grenadeCPMANoPicMipShader = trap_R_RegisterShaderNoMip( "grenadeCPMA_NPM" );
	cgs.media.grenadeCPMAModel = trap_R_RegisterModel( "models/ammo/grenadeCPMA.md3" );
	if ( !cgs.media.grenadeCPMAModel ) {
		cgs.media.grenadeCPMAModel = trap_R_RegisterModel( "models/ammo/grenade1.md3" );
		cgs.media.grenadeCPMANoPicMipShader = 0;
	}
	cgs.media.bloodTrailShader = trap_R_RegisterShader( "bloodTrail" );
	cgs.media.lagometerShader = trap_R_RegisterShader("lagometer" );
	cgs.media.connectionShader = trap_R_RegisterShader( "disconnected" );

	cgs.media.waterBubbleShader = trap_R_RegisterShader( "waterBubble" );

	cgs.media.tracerShader = trap_R_RegisterShader( "gfx/misc/tracer" );
	cgs.media.selectShader = trap_R_RegisterShader( "gfx/2d/select" );

	for ( i = 0 ; i < NUM_CROSSHAIRS ; i++ ) {
		cgs.media.crosshairShader[i] = trap_R_RegisterShader( va("gfx/2d/crosshair%c", 'a'+i) );
		// OSP's second crosshair set
		cgs.media.crosshairShader45[i] = trap_R_RegisterShader( va( "gfx/2d/crosshair%c2", 'a' + i ) );
		if ( !cgs.media.crosshairShader45[i] ) {
			cgs.media.crosshairShader45[i] = cgs.media.crosshairShader[i];
		}
	}

	// OSP's decal pictures: "decalOSP" over blender_180 and "decal%d"
	// over each custom picture, for the decal configstrings (CG_Init)
	cgs.media.blender180Shader = trap_R_RegisterShader( "gfx/2d/blender_180.tga" );
	cgs.decalOSPShader[0] = trap_R_RegisterShader( "decalOSP" );
	if ( cgs.decalOSPShader[0] ) {
		cgs.blender180ShaderCopy[0] = cgs.media.blender180Shader;
		cgs.decalOSPShaderCopyCount++;
	}
	for ( i = 0; i < MAX_OSP_CUSTOM_GFX; i++ ) {
		cgs.media.customShader[i] = trap_R_RegisterShaderNoMip( va( "gfx/2d/custom%d", i + 1 ) );
		if ( !cgs.media.customShader[i] ) {
			break;
		}
		cgs.decalOSPShader[cgs.decalOSPShaderCopyCount] = trap_R_RegisterShaderNoMip( va( "decal%d", i + 1 ) );
		if ( cgs.decalOSPShader[cgs.decalOSPShaderCopyCount] ) {
			cgs.blender180ShaderCopy[cgs.decalOSPShaderCopyCount] = cgs.media.customShader[i];
			cgs.decalOSPShaderCopyCount++;
		}
	}

	cgs.media.backTileShader = trap_R_RegisterShader( "gfx/2d/backtile" );
	cgs.media.noammoShader = trap_R_RegisterShaderNoMip( "icons/noammo" );

	// powerup shaders
	cgs.media.quadShader = trap_R_RegisterShader("powerups/quad" );
	cgs.media.quadWeaponShader = trap_R_RegisterShader("powerups/quadWeapon" );
	cgs.media.battleSuitShader = trap_R_RegisterShader("powerups/battleSuit" );
	cgs.media.battleWeaponShader = trap_R_RegisterShader("powerups/battleWeapon" );
	cgs.media.invisShader = trap_R_RegisterShader("powerups/invisibility" );
	cgs.media.regenShader = trap_R_RegisterShader("powerups/regen" );
	cgs.media.hastePuffShader = trap_R_RegisterShader("hasteSmokePuff" );
	// OSP's freeze tag
	cgs.media.freezeShader = trap_R_RegisterShader( "freezeShader" );
	cgs.media.freezeMarkShader = trap_R_RegisterShader( "freezeMarkShader" );

#ifdef MISSIONPACK
	if ( cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER || cg_buildScript.integer ) {
#else
	if ( cgs.gametype == GT_CTF || cg_buildScript.integer ) {
#endif
		cgs.media.redCubeModel = trap_R_RegisterModel( "models/powerups/orb/r_orb.md3" );
		cgs.media.blueCubeModel = trap_R_RegisterModel( "models/powerups/orb/b_orb.md3" );
		// OSP registers no skull icons
	}

#ifdef MISSIONPACK
	if ( cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER || cg_buildScript.integer ) {
#else
	if ( cgs.gametype == GT_CTF || cg_buildScript.integer ) {
#endif
		cgs.media.redFlagModel = trap_R_RegisterModel( "models/flags/r_flag.md3" );
		cgs.media.blueFlagModel = trap_R_RegisterModel( "models/flags/b_flag.md3" );
		cgs.media.redFlagShader[0] = trap_R_RegisterShaderNoMip( "icons/iconf_red1" );
		cgs.media.redFlagShader[1] = trap_R_RegisterShaderNoMip( "icons/iconf_red2" );
		cgs.media.redFlagShader[2] = trap_R_RegisterShaderNoMip( "icons/iconf_red3" );
		cgs.media.blueFlagShader[0] = trap_R_RegisterShaderNoMip( "icons/iconf_blu1" );
		cgs.media.blueFlagShader[1] = trap_R_RegisterShaderNoMip( "icons/iconf_blu2" );
		cgs.media.blueFlagShader[2] = trap_R_RegisterShaderNoMip( "icons/iconf_blu3" );
#ifdef MISSIONPACK
		cgs.media.flagPoleModel = trap_R_RegisterModel( "models/flag2/flagpole.md3" );
		cgs.media.flagFlapModel = trap_R_RegisterModel( "models/flag2/flagflap3.md3" );

		cgs.media.redFlagFlapSkin = trap_R_RegisterSkin( "models/flag2/red.skin" );
		cgs.media.blueFlagFlapSkin = trap_R_RegisterSkin( "models/flag2/blue.skin" );
		cgs.media.neutralFlagFlapSkin = trap_R_RegisterSkin( "models/flag2/white.skin" );

		cgs.media.redFlagBaseModel = trap_R_RegisterModel( "models/mapobjects/flagbase/red_base.md3" );
		cgs.media.blueFlagBaseModel = trap_R_RegisterModel( "models/mapobjects/flagbase/blue_base.md3" );
		cgs.media.neutralFlagBaseModel = trap_R_RegisterModel( "models/mapobjects/flagbase/ntrl_base.md3" );
#endif
	}

#ifdef MISSIONPACK
	if ( cgs.gametype == GT_1FCTF || cg_buildScript.integer ) {
		cgs.media.neutralFlagModel = trap_R_RegisterModel( "models/flags/n_flag.md3" );
		cgs.media.flagShader[0] = trap_R_RegisterShaderNoMip( "icons/iconf_neutral1" );
		cgs.media.flagShader[1] = trap_R_RegisterShaderNoMip( "icons/iconf_red2" );
		cgs.media.flagShader[2] = trap_R_RegisterShaderNoMip( "icons/iconf_blu2" );
		cgs.media.flagShader[3] = trap_R_RegisterShaderNoMip( "icons/iconf_neutral3" );
	}

	if ( cgs.gametype == GT_OBELISK || cg_buildScript.integer ) {
		cgs.media.overloadBaseModel = trap_R_RegisterModel( "models/powerups/overload_base.md3" );
		cgs.media.overloadTargetModel = trap_R_RegisterModel( "models/powerups/overload_target.md3" );
		cgs.media.overloadLightsModel = trap_R_RegisterModel( "models/powerups/overload_lights.md3" );
		cgs.media.overloadEnergyModel = trap_R_RegisterModel( "models/powerups/overload_energy.md3" );
	}

	if ( cgs.gametype == GT_HARVESTER || cg_buildScript.integer ) {
		cgs.media.harvesterModel = trap_R_RegisterModel( "models/powerups/harvester/harvester.md3" );
		cgs.media.harvesterRedSkin = trap_R_RegisterSkin( "models/powerups/harvester/red.skin" );
		cgs.media.harvesterBlueSkin = trap_R_RegisterSkin( "models/powerups/harvester/blue.skin" );
		cgs.media.harvesterNeutralModel = trap_R_RegisterModel( "models/powerups/obelisk/obelisk.md3" );
	}

	cgs.media.redKamikazeShader = trap_R_RegisterShader( "models/weaphits/kamikred" );
	cgs.media.dustPuffShader = trap_R_RegisterShader("hasteSmokePuff" );
#endif

	if ( cgs.gametype >= GT_TEAM || cg_buildScript.integer ) {
		cgs.media.friendShader = trap_R_RegisterShader( "sprites/foe" );
		cgs.media.redQuadShader = trap_R_RegisterShader("powerups/blueflag" );
		cgs.media.teamStatusBar = trap_R_RegisterShaderNoMip( "gfx/2d/colorbar.tga" );	// OSP: NoMip
#ifdef MISSIONPACK
		cgs.media.blueKamikazeShader = trap_R_RegisterShader( "models/weaphits/kamikblu" );
#endif
	}
	// OSP registers the colour bar again for every game type
	cgs.media.teamStatusBar = trap_R_RegisterShaderNoMip( "gfx/2d/colorbar.tga" );

	// OSP's green and red armor
	cgs.media.armorModelGreen = trap_R_RegisterModel( "models/powerups/armor/armor_gre.md3" );
	cgs.media.armorIconGreen = trap_R_RegisterShaderNoMip( "icons/iconr_green" );
	cgs.media.armorModel = trap_R_RegisterModel( "models/powerups/armor/armor_yel.md3" );
	cgs.media.armorIcon  = trap_R_RegisterShaderNoMip( "icons/iconr_yellow" );
	cgs.media.armorModelRed = trap_R_RegisterModel( "models/powerups/armor/armor_red.md3" );
	cgs.media.armorIconRed = trap_R_RegisterShaderNoMip( "icons/iconr_red" );

	cgs.media.machinegunBrassModel = trap_R_RegisterModel( "models/weapons2/shells/m_shell.md3" );
	cgs.media.shotgunBrassModel = trap_R_RegisterModel( "models/weapons2/shells/s_shell.md3" );

	cgs.media.gibAbdomen = trap_R_RegisterModel( "models/gibs/abdomen.md3" );
	cgs.media.gibArm = trap_R_RegisterModel( "models/gibs/arm.md3" );
	cgs.media.gibChest = trap_R_RegisterModel( "models/gibs/chest.md3" );
	cgs.media.gibFist = trap_R_RegisterModel( "models/gibs/fist.md3" );
	cgs.media.gibFoot = trap_R_RegisterModel( "models/gibs/foot.md3" );
	cgs.media.gibForearm = trap_R_RegisterModel( "models/gibs/forearm.md3" );
	cgs.media.gibIntestine = trap_R_RegisterModel( "models/gibs/intestine.md3" );
	cgs.media.gibLeg = trap_R_RegisterModel( "models/gibs/leg.md3" );
	cgs.media.gibSkull = trap_R_RegisterModel( "models/gibs/skull.md3" );
	cgs.media.gibBrain = trap_R_RegisterModel( "models/gibs/brain.md3" );

	cgs.media.smoke2 = trap_R_RegisterModel( "models/weapons2/shells/s_shell.md3" );

	// OSP prefers a no-picmip chat balloon, blood explosion
	if ( !( cgs.media.balloonShader = trap_R_RegisterShader( "sprites/balloon3NoPicMip" ) ) ) {
		cgs.media.balloonShader = trap_R_RegisterShader( "sprites/balloon3" );
	}

	cgs.media.bloodExplosionShader = trap_R_RegisterShader( "bloodExplosion" );
	cgs.media.bloodExplosionNoPicMipShader = trap_R_RegisterShader( "bloodExplosionNoPicMip" );
	if ( !cgs.media.bloodExplosionNoPicMipShader ) {
		cgs.media.bloodExplosionNoPicMipShader = cgs.media.bloodExplosionShader;
	}

	cgs.media.bulletFlashModel = trap_R_RegisterModel("models/weaphits/bullet.md3");
	cgs.media.ringFlashModel = trap_R_RegisterModel("models/weaphits/ring02.md3");
	cgs.media.dishFlashModel = trap_R_RegisterModel("models/weaphits/boom01.md3");
#ifdef MISSIONPACK
	cgs.media.teleportEffectModel = trap_R_RegisterModel( "models/powerups/pop.md3" );
#else
	cgs.media.teleportEffectModel = trap_R_RegisterModel( "models/misc/telep.md3" );
	cgs.media.teleportEffectShader = trap_R_RegisterShader( "teleportEffect" );
	cgs.media.cursor = trap_R_RegisterShaderNoMip( "menu/art/3_cursor2" );	// OSP (HMI menus)
#endif
#ifdef MISSIONPACK
	cgs.media.kamikazeEffectModel = trap_R_RegisterModel( "models/weaphits/kamboom2.md3" );
	cgs.media.kamikazeShockWave = trap_R_RegisterModel( "models/weaphits/kamwave.md3" );
	cgs.media.kamikazeHeadModel = trap_R_RegisterModel( "models/powerups/kamikazi.md3" );
	cgs.media.kamikazeHeadTrail = trap_R_RegisterModel( "models/powerups/trailtest.md3" );
	cgs.media.guardPowerupModel = trap_R_RegisterModel( "models/powerups/guard_player.md3" );
	cgs.media.scoutPowerupModel = trap_R_RegisterModel( "models/powerups/scout_player.md3" );
	cgs.media.doublerPowerupModel = trap_R_RegisterModel( "models/powerups/doubler_player.md3" );
	cgs.media.ammoRegenPowerupModel = trap_R_RegisterModel( "models/powerups/ammo_player.md3" );
	cgs.media.invulnerabilityImpactModel = trap_R_RegisterModel( "models/powerups/shield/impact.md3" );
	cgs.media.invulnerabilityJuicedModel = trap_R_RegisterModel( "models/powerups/shield/juicer.md3" );
	cgs.media.medkitUsageModel = trap_R_RegisterModel( "models/powerups/regen.md3" );
	cgs.media.heartShader = trap_R_RegisterShaderNoMip( "ui/assets/statusbar/selectedhealth.tga" );

#endif

	cgs.media.invulnerabilityPowerupModel = trap_R_RegisterModel( "models/powerups/shield/shield.md3" );
	cgs.media.medalImpressive = trap_R_RegisterShaderNoMip( "medal_impressive" );
	cgs.media.medalExcellent = trap_R_RegisterShaderNoMip( "medal_excellent" );
	cgs.media.medalGauntlet = trap_R_RegisterShaderNoMip( "medal_gauntlet" );
	cgs.media.medalDefend = trap_R_RegisterShaderNoMip( "medal_defend" );
	cgs.media.medalAssist = trap_R_RegisterShaderNoMip( "medal_assist" );
	cgs.media.medalCapture = trap_R_RegisterShaderNoMip( "medal_capture" );


	memset( cg_items, 0, sizeof( cg_items ) );
	memset( cg_weapons, 0, sizeof( cg_weapons ) );

	// only register the items that the server says we need
	Q_strncpyz( items, CG_ConfigString( CS_ITEMS ), sizeof( items ) );

	for ( i = 1 ; i < bg_numItems ; i++ ) {
		if ( items[ i ] == '1' || cg_buildScript.integer ) {
			CG_LoadingItem( i );
			CG_RegisterItemVisuals( i );
		}
	}

	// wall marks
	// (OSP: with OSP's cg_nomip variants)
	cgs.media.bulletMarkShader = trap_R_RegisterShader( "gfx/damage/bullet_mrk" );
	cgs.media.bulletMarkNoPicMipShader = trap_R_RegisterShader( "bullet_mrkNoPicMip" );
	if ( !cgs.media.bulletMarkNoPicMipShader ) {
		cgs.media.bulletMarkNoPicMipShader = cgs.media.bulletMarkShader;
	}
	cgs.media.burnMarkShader = trap_R_RegisterShader( "gfx/damage/burn_med_mrk" );
	cgs.media.burnMarkNoPicMipShader = trap_R_RegisterShader( "burn_med_mrkNoPicMip" );
	if ( !cgs.media.burnMarkNoPicMipShader ) {
		cgs.media.burnMarkNoPicMipShader = cgs.media.burnMarkShader;
	}
	cgs.media.holeMarkShader = trap_R_RegisterShader( "gfx/damage/hole_lg_mrk" );
	cgs.media.holeMarkNoPicMipShader = trap_R_RegisterShader( "hole_lg_mrkNoPicMip" );
	if ( !cgs.media.holeMarkNoPicMipShader ) {
		cgs.media.holeMarkNoPicMipShader = cgs.media.holeMarkShader;
	}
	cgs.media.energyMarkShader = trap_R_RegisterShader( "gfx/damage/plasma_mrk" );
	cgs.media.energyMarkNoPicMipShader = trap_R_RegisterShader( "plasma_mrkNoPicMip" );
	if ( !cgs.media.energyMarkNoPicMipShader ) {
		cgs.media.energyMarkNoPicMipShader = cgs.media.energyMarkShader;
	}
	cgs.media.shadowMarkShader = trap_R_RegisterShader( "markShadow" );
	cgs.media.wakeMarkShader = trap_R_RegisterShader( "wake" );
	cgs.media.bloodMarkShader = trap_R_RegisterShader( "bloodMark" );

	// register the inline models
	cgs.numInlineModels = trap_CM_NumInlineModels();
	for ( i = 1 ; i < cgs.numInlineModels ; i++ ) {
		char	name[10];
		vec3_t			mins, maxs;
		int				j;

		Com_sprintf( name, sizeof(name), "*%i", i );
		cgs.inlineDrawModel[i] = trap_R_RegisterModel( name );
		trap_R_ModelBounds( cgs.inlineDrawModel[i], mins, maxs );
		for ( j = 0 ; j < 3 ; j++ ) {
			cgs.inlineModelMidpoints[i][j] = mins[j] + 0.5 * ( maxs[j] - mins[j] );
		}
	}

	// register all the server specified models
	for (i=1 ; i<MAX_MODELS ; i++) {
		const char		*modelName;

		modelName = CG_ConfigString( CS_MODELS+i );
		if ( !modelName[0] ) {
			break;
		}
		cgs.gameModels[i] = trap_R_RegisterModel( modelName );
	}

#ifdef MISSIONPACK
	// new stuff
	cgs.media.patrolShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/patrol.tga");
	cgs.media.assaultShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/assault.tga");
	cgs.media.campShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/camp.tga");
	cgs.media.followShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/follow.tga");
	cgs.media.defendShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/defend.tga");
	cgs.media.teamLeaderShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/team_leader.tga");
	cgs.media.retrieveShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/retrieve.tga");
	cgs.media.escortShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/escort.tga");
	cgs.media.cursor = trap_R_RegisterShaderNoMip( "menu/art/3_cursor2" );
	cgs.media.sizeCursor = trap_R_RegisterShaderNoMip( "ui/assets/sizecursor.tga" );
	cgs.media.selectCursor = trap_R_RegisterShaderNoMip( "ui/assets/selectcursor.tga" );
	cgs.media.flagShaders[0] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_in_base.tga");
	cgs.media.flagShaders[1] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_capture.tga");
	cgs.media.flagShaders[2] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_missing.tga");

	trap_R_RegisterModel( "models/players/james/lower.md3" );
	trap_R_RegisterModel( "models/players/james/upper.md3" );
	trap_R_RegisterModel( "models/players/heads/james/james.md3" );

	trap_R_RegisterModel( "models/players/janet/lower.md3" );
	trap_R_RegisterModel( "models/players/janet/upper.md3" );
	trap_R_RegisterModel( "models/players/heads/janet/janet.md3" );

#endif
	CG_ClearParticles ();
/*
	for (i=1; i<MAX_PARTICLES_AREAS; i++)
	{
		{
			int rval;

			rval = CG_NewParticleArea ( CS_PARTICLES + i);
			if (!rval)
				break;
		}
	}
*/
}



/*																																			
=======================
CG_BuildSpectatorString

=======================
*/
void CG_BuildSpectatorString() {
	int i;
	cg.spectatorList[0] = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_SPECTATOR ) {
			Q_strcat(cg.spectatorList, sizeof(cg.spectatorList), va("%s     ", cgs.clientinfo[i].name));
		}
	}
	i = strlen(cg.spectatorList);
	if (i != cg.spectatorLen) {
		cg.spectatorLen = i;
		cg.spectatorWidth = -1;
	}
}


/*																																			
===================
CG_RegisterClients
===================
*/
static void CG_RegisterClients( void ) {
	int		i;

	CG_LoadingClient(cg.clientNum);
	CG_NewClientInfo(cg.clientNum);

	for (i=0 ; i<MAX_CLIENTS ; i++) {
		const char		*clientInfo;

		if (cg.clientNum == i) {
			continue;
		}

		clientInfo = CG_ConfigString( CS_PLAYERS+i );
		if ( !clientInfo[0]) {
			continue;
		}
		CG_LoadingClient( i );
		CG_NewClientInfo( i );
	}
	CG_BuildSpectatorString();
}

//===========================================================================

/*
=================
CG_ConfigString
=================
*/
const char *CG_ConfigString( int index ) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		CG_Error( "CG_ConfigString: bad index: %i", index );
	}
	return cgs.gameState.stringData + cgs.gameState.stringOffsets[ index ];
}

//==================================================================

/*
======================
CG_StartMusic

======================
*/
void CG_StartMusic( void ) {
	char	*s;
	char	parm1[MAX_QPATH], parm2[MAX_QPATH];

	// start the background music
	s = (char *)CG_ConfigString( CS_MUSIC );
	Q_strncpyz( parm1, COM_Parse( &s ), sizeof( parm1 ) );
	Q_strncpyz( parm2, COM_Parse( &s ), sizeof( parm2 ) );

	trap_S_StartBackgroundTrack( parm1, parm2 );
}
#ifdef MISSIONPACK
char *CG_GetMenuBuffer(const char *filename) {
	int	len;
	fileHandle_t	f;
	static char buf[MAX_MENUFILE];

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		trap_Print( va( S_COLOR_RED "menu file not found: %s, using default\n", filename ) );
		return NULL;
	}
	if ( len >= MAX_MENUFILE ) {
		trap_Print( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", filename, len, MAX_MENUFILE ) );
		trap_FS_FCloseFile( f );
		return NULL;
	}

	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );

	return buf;
}

//
// ==============================
// new hud stuff ( mission pack )
// ==============================
//
qboolean CG_Asset_Parse(int handle) {
	pc_token_t token;
	const char *tempStr;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (Q_stricmp(token.string, "{") != 0) {
		return qfalse;
	}
    
	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;

		if (Q_stricmp(token.string, "}") == 0) {
			return qtrue;
		}

		// font
		if (Q_stricmp(token.string, "font") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
				return qfalse;
			}
			cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.textFont);
			continue;
		}

		// smallFont
		if (Q_stricmp(token.string, "smallFont") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
				return qfalse;
			}
			cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.smallFont);
			continue;
		}

		// font
		if (Q_stricmp(token.string, "bigfont") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
				return qfalse;
			}
			cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.bigFont);
			continue;
		}

		// gradientbar
		if (Q_stricmp(token.string, "gradientbar") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token.string, "menuEnterSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			cgDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token.string, "menuExitSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			cgDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token.string, "itemFocusSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			cgDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			cgDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr, qfalse );
			continue;
		}

		if (Q_stricmp(token.string, "cursor") == 0) {
			if (!PC_String_Parse(handle, &cgDC.Assets.cursorStr)) {
				return qfalse;
			}
			cgDC.Assets.cursor = trap_R_RegisterShaderNoMip( cgDC.Assets.cursorStr);
			continue;
		}

		if (Q_stricmp(token.string, "fadeClamp") == 0) {
			if (!PC_Float_Parse(handle, &cgDC.Assets.fadeClamp)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeCycle") == 0) {
			if (!PC_Int_Parse(handle, &cgDC.Assets.fadeCycle)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeAmount") == 0) {
			if (!PC_Float_Parse(handle, &cgDC.Assets.fadeAmount)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowX") == 0) {
			if (!PC_Float_Parse(handle, &cgDC.Assets.shadowX)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowY") == 0) {
			if (!PC_Float_Parse(handle, &cgDC.Assets.shadowY)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowColor") == 0) {
			if (!PC_Color_Parse(handle, &cgDC.Assets.shadowColor)) {
				return qfalse;
			}
			cgDC.Assets.shadowFadeClamp = cgDC.Assets.shadowColor[3];
			continue;
		}
	}
	return qfalse; // bk001204 - why not?
}

void CG_ParseMenu(const char *menuFile) {
	pc_token_t token;
	int handle;

	handle = trap_PC_LoadSource(menuFile);
	if (!handle)
		handle = trap_PC_LoadSource("ui/testhud.menu");
	if (!handle)
		return;

	while ( 1 ) {
		if (!trap_PC_ReadToken( handle, &token )) {
			break;
		}

		//if ( Q_stricmp( token, "{" ) ) {
		//	Com_Printf( "Missing { in menu file\n" );
		//	break;
		//}

		//if ( menuCount == MAX_MENUS ) {
		//	Com_Printf( "Too many menus!\n" );
		//	break;
		//}

		if ( token.string[0] == '}' ) {
			break;
		}

		if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
			if (CG_Asset_Parse(handle)) {
				continue;
			} else {
				break;
			}
		}


		if (Q_stricmp(token.string, "menudef") == 0) {
			// start a new menu
			Menu_New(handle);
		}
	}
	trap_PC_FreeSource(handle);
}

qboolean CG_Load_Menu(char **p) {
	char *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{') {
		return qfalse;
	}

	while ( 1 ) {

		token = COM_ParseExt(p, qtrue);
    
		if (Q_stricmp(token, "}") == 0) {
			return qtrue;
		}

		if ( !token || token[0] == 0 ) {
			return qfalse;
		}

		CG_ParseMenu(token); 
	}
	return qfalse;
}



void CG_LoadMenus(const char *menuFile) {
	char	*token;
	char *p;
	int	len, start;
	fileHandle_t	f;
	static char buf[MAX_MENUDEFFILE];

	start = trap_Milliseconds();

	len = trap_FS_FOpenFile( menuFile, &f, FS_READ );
	if ( !f ) {
		trap_Error( va( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile ) );
		len = trap_FS_FOpenFile( "ui/hud.txt", &f, FS_READ );
		if (!f) {
			trap_Error( va( S_COLOR_RED "default menu file not found: ui/hud.txt, unable to continue!\n", menuFile ) );
		}
	}

	if ( len >= MAX_MENUDEFFILE ) {
		trap_Error( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", menuFile, len, MAX_MENUDEFFILE ) );
		trap_FS_FCloseFile( f );
		return;
	}

	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	
	COM_Compress(buf);

	Menu_Reset();

	p = buf;

	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if( !token || token[0] == 0 || token[0] == '}') {
			break;
		}

		//if ( Q_stricmp( token, "{" ) ) {
		//	Com_Printf( "Missing { in menu file\n" );
		//	break;
		//}

		//if ( menuCount == MAX_MENUS ) {
		//	Com_Printf( "Too many menus!\n" );
		//	break;
		//}

		if ( Q_stricmp( token, "}" ) == 0 ) {
			break;
		}

		if (Q_stricmp(token, "loadmenu") == 0) {
			if (CG_Load_Menu(&p)) {
				continue;
			} else {
				break;
			}
		}
	}

	Com_Printf("UI menu load time = %d milli seconds\n", trap_Milliseconds() - start);

}



static qboolean CG_OwnerDrawHandleKey(int ownerDraw, int flags, float *special, int key) {
	return qfalse;
}


static int CG_FeederCount(float feederID) {
	int i, count;
	count = 0;
	if (feederID == FEEDER_REDTEAM_LIST) {
		for (i = 0; i < cg.numScores; i++) {
			if (cg.scores[i].team == TEAM_RED) {
				count++;
			}
		}
	} else if (feederID == FEEDER_BLUETEAM_LIST) {
		for (i = 0; i < cg.numScores; i++) {
			if (cg.scores[i].team == TEAM_BLUE) {
				count++;
			}
		}
	} else if (feederID == FEEDER_SCOREBOARD) {
		return cg.numScores;
	}
	return count;
}


void CG_SetScoreSelection(void *p) {
	menuDef_t *menu = (menuDef_t*)p;
	playerState_t *ps = &cg.snap->ps;
	int i, red, blue;
	red = blue = 0;
	for (i = 0; i < cg.numScores; i++) {
		if (cg.scores[i].team == TEAM_RED) {
			red++;
		} else if (cg.scores[i].team == TEAM_BLUE) {
			blue++;
		}
		if (ps->clientNum == cg.scores[i].client) {
			cg.selectedScore = i;
		}
	}

	if (menu == NULL) {
		// just interested in setting the selected score
		return;
	}

	if ( cgs.gametype >= GT_TEAM ) {
		int feeder = FEEDER_REDTEAM_LIST;
		i = red;
		if (cg.scores[cg.selectedScore].team == TEAM_BLUE) {
			feeder = FEEDER_BLUETEAM_LIST;
			i = blue;
		}
		Menu_SetFeederSelection(menu, feeder, i, NULL);
	} else {
		Menu_SetFeederSelection(menu, FEEDER_SCOREBOARD, cg.selectedScore, NULL);
	}
}

// FIXME: might need to cache this info
static clientInfo_t * CG_InfoFromScoreIndex(int index, int team, int *scoreIndex) {
	int i, count;
	if ( cgs.gametype >= GT_TEAM ) {
		count = 0;
		for (i = 0; i < cg.numScores; i++) {
			if (cg.scores[i].team == team) {
				if (count == index) {
					*scoreIndex = i;
					return &cgs.clientinfo[cg.scores[i].client];
				}
				count++;
			}
		}
	}
	*scoreIndex = index;
	return &cgs.clientinfo[ cg.scores[index].client ];
}

static const char *CG_FeederItemText(float feederID, int index, int column, qhandle_t *handle) {
	gitem_t *item;
	int scoreIndex = 0;
	clientInfo_t *info = NULL;
	int team = -1;
	score_t *sp = NULL;

	*handle = -1;

	if (feederID == FEEDER_REDTEAM_LIST) {
		team = TEAM_RED;
	} else if (feederID == FEEDER_BLUETEAM_LIST) {
		team = TEAM_BLUE;
	}

	info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
	sp = &cg.scores[scoreIndex];

	if (info && info->infoValid) {
		switch (column) {
			case 0:
				if ( info->powerups & ( 1 << PW_NEUTRALFLAG ) ) {
					item = BG_FindItemForPowerup( PW_NEUTRALFLAG );
					*handle = cg_items[ ITEM_INDEX(item) ].icon;
				} else if ( info->powerups & ( 1 << PW_REDFLAG ) ) {
					item = BG_FindItemForPowerup( PW_REDFLAG );
					*handle = cg_items[ ITEM_INDEX(item) ].icon;
				} else if ( info->powerups & ( 1 << PW_BLUEFLAG ) ) {
					item = BG_FindItemForPowerup( PW_BLUEFLAG );
					*handle = cg_items[ ITEM_INDEX(item) ].icon;
				} else {
					if ( info->botSkill > 0 && info->botSkill <= 5 ) {
						*handle = cgs.media.botSkillShaders[ info->botSkill - 1 ];
					} else if ( info->handicap < 100 ) {
					return va("%i", info->handicap );
					}
				}
			break;
			case 1:
				if (team == -1) {
					return "";
				} else {
					*handle = CG_StatusHandle(info->teamTask);
				}
		  break;
			case 2:
				if ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << sp->client ) ) {
					return "Ready";
				}
				if (team == -1) {
					if (cgs.gametype == GT_TOURNAMENT) {
						return va("%i/%i", info->wins, info->losses);
					} else if (info->infoValid && info->team == TEAM_SPECTATOR ) {
						return "Spectator";
					} else {
						return "";
					}
				} else {
					if (info->teamLeader) {
						return "Leader";
					}
				}
			break;
			case 3:
				return info->name;
			break;
			case 4:
				return va("%i", info->score);
			break;
			case 5:
				return va("%4i", sp->time);
			break;
			case 6:
				if ( sp->ping == -1 ) {
					return "connecting";
				} 
				return va("%4i", sp->ping);
			break;
		}
	}

	return "";
}

static qhandle_t CG_FeederItemImage(float feederID, int index) {
	return 0;
}

static void CG_FeederSelection(float feederID, int index) {
	if ( cgs.gametype >= GT_TEAM ) {
		int i, count;
		int team = (feederID == FEEDER_REDTEAM_LIST) ? TEAM_RED : TEAM_BLUE;
		count = 0;
		for (i = 0; i < cg.numScores; i++) {
			if (cg.scores[i].team == team) {
				if (index == count) {
					cg.selectedScore = i;
				}
				count++;
			}
		}
	} else {
		cg.selectedScore = index;
	}
}
#endif

#ifdef MISSIONPACK // bk001204 - only needed there
static float CG_Cvar_Get(const char *cvar) {
	char buff[128];
	memset(buff, 0, sizeof(buff));
	trap_Cvar_VariableStringBuffer(cvar, buff, sizeof(buff));
	return atof(buff);
}
#endif

#ifdef MISSIONPACK
void CG_Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char *text, int cursorPos, char cursor, int limit, int style) {
	CG_Text_Paint(x, y, scale, color, text, 0, limit, style);
}

static int CG_OwnerDrawWidth(int ownerDraw, float scale) {
	switch (ownerDraw) {
	  case CG_GAME_TYPE:
			return CG_Text_Width(CG_GameTypeString(), scale, 0);
	  case CG_GAME_STATUS:
			return CG_Text_Width(CG_GetGameStatusText(), scale, 0);
			break;
	  case CG_KILLER:
			return CG_Text_Width(CG_GetKillerText(), scale, 0);
			break;
	  case CG_RED_NAME:
			return CG_Text_Width(cg_redTeamName.string, scale, 0);
			break;
	  case CG_BLUE_NAME:
			return CG_Text_Width(cg_blueTeamName.string, scale, 0);
			break;


	}
	return 0;
}

static int CG_PlayCinematic(const char *name, float x, float y, float w, float h) {
  return trap_CIN_PlayCinematic(name, x, y, w, h, CIN_loop);
}

static void CG_StopCinematic(int handle) {
  trap_CIN_StopCinematic(handle);
}

static void CG_DrawCinematic(int handle, float x, float y, float w, float h) {
  trap_CIN_SetExtents(handle, x, y, w, h);
  trap_CIN_DrawCinematic(handle);
}

static void CG_RunCinematicFrame(int handle) {
  trap_CIN_RunCinematic(handle);
}

/*
=================
CG_LoadHudMenu();

=================
*/
void CG_LoadHudMenu() {
	char buff[1024];
	const char *hudSet;

	cgDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
	cgDC.setColor = &trap_R_SetColor;
	cgDC.drawHandlePic = &CG_DrawPic;
	cgDC.drawStretchPic = &trap_R_DrawStretchPic;
	cgDC.drawText = &CG_Text_Paint;
	cgDC.textWidth = &CG_Text_Width;
	cgDC.textHeight = &CG_Text_Height;
	cgDC.registerModel = &trap_R_RegisterModel;
	cgDC.modelBounds = &trap_R_ModelBounds;
	cgDC.fillRect = &CG_FillRect;
	cgDC.drawRect = &CG_DrawRect;   
	cgDC.drawSides = &CG_DrawSides;
	cgDC.drawTopBottom = &CG_DrawTopBottom;
	cgDC.clearScene = &trap_R_ClearScene;
	cgDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
	cgDC.renderScene = &trap_R_RenderScene;
	cgDC.registerFont = &trap_R_RegisterFont;
	cgDC.ownerDrawItem = &CG_OwnerDraw;
	cgDC.getValue = &CG_GetValue;
	cgDC.ownerDrawVisible = &CG_OwnerDrawVisible;
	cgDC.runScript = &CG_RunMenuScript;
	cgDC.getTeamColor = &CG_GetTeamColor;
	cgDC.setCVar = trap_Cvar_Set;
	cgDC.getCVarString = trap_Cvar_VariableStringBuffer;
	cgDC.getCVarValue = CG_Cvar_Get;
	cgDC.drawTextWithCursor = &CG_Text_PaintWithCursor;
	//cgDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
	//cgDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
	cgDC.startLocalSound = &trap_S_StartLocalSound;
	cgDC.ownerDrawHandleKey = &CG_OwnerDrawHandleKey;
	cgDC.feederCount = &CG_FeederCount;
	cgDC.feederItemImage = &CG_FeederItemImage;
	cgDC.feederItemText = &CG_FeederItemText;
	cgDC.feederSelection = &CG_FeederSelection;
	//cgDC.setBinding = &trap_Key_SetBinding;
	//cgDC.getBindingBuf = &trap_Key_GetBindingBuf;
	//cgDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
	//cgDC.executeText = &trap_Cmd_ExecuteText;
	cgDC.Error = &Com_Error; 
	cgDC.Print = &Com_Printf; 
	cgDC.ownerDrawWidth = &CG_OwnerDrawWidth;
	//cgDC.Pause = &CG_Pause;
	cgDC.registerSound = &trap_S_RegisterSound;
	cgDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
	cgDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
	cgDC.playCinematic = &CG_PlayCinematic;
	cgDC.stopCinematic = &CG_StopCinematic;
	cgDC.drawCinematic = &CG_DrawCinematic;
	cgDC.runCinematicFrame = &CG_RunCinematicFrame;
	
	Init_Display(&cgDC);

	Menu_Reset();
	
	trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
	hudSet = buff;
	if (hudSet[0] == '\0') {
		hudSet = "ui/hud.txt";
	}

	CG_LoadMenus(hudSet);
}

void CG_AssetCache() {
	//if (Assets.textFont == NULL) {
	//  trap_R_RegisterFont("fonts/arial.ttf", 72, &Assets.textFont);
	//}
	//Assets.background = trap_R_RegisterShaderNoMip( ASSET_BACKGROUND );
	//Com_Printf("Menu Size: %i bytes\n", sizeof(Menus));
	cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip( ASSET_GRADIENTBAR );
	cgDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip( ART_FX_BASE );
	cgDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip( ART_FX_RED );
	cgDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip( ART_FX_YELLOW );
	cgDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip( ART_FX_GREEN );
	cgDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip( ART_FX_TEAL );
	cgDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip( ART_FX_BLUE );
	cgDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip( ART_FX_CYAN );
	cgDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip( ART_FX_WHITE );
	cgDC.Assets.scrollBar = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR );
	cgDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
	cgDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
	cgDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
	cgDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
	cgDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );
	cgDC.Assets.sliderBar = trap_R_RegisterShaderNoMip( ASSET_SLIDER_BAR );
	cgDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip( ASSET_SLIDER_THUMB );
}
#endif
/*
=================
CG_Init

Called after every level change or subsystem restart
Will perform callbacks to make the loading info screen update.
=================
*/
int CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum ) {
	int			i;
	const char	*s;

	// clear everything
	memset( &cgs, 0, sizeof( cgs ) );
	memset( &cg, 0, sizeof( cg ) );
	memset( cg_entities, 0, sizeof(cg_entities) );
	memset( cg_weapons, 0, sizeof(cg_weapons) );
	memset( cg_items, 0, sizeof(cg_items) );

	cg.clientNum = clientNum;

	cgs.processedSnapshotNum = serverMessageNum;
	cgs.serverCommandSequence = serverCommandSequence;

	// load a few needed things before we do any screen updates
	cgs.media.charsetShader1	= trap_R_RegisterShader( "gfx/2d/bigchars1" );	// OSP (ch_3waveFont)
	cgs.media.charsetShader		= trap_R_RegisterShader( "gfx/2d/bigchars" );
	cgs.media.whiteShader		= trap_R_RegisterShader( "white" );
	cgs.media.charsetProp		= trap_R_RegisterShaderNoMip( "menu/art/font1_prop.tga" );
	cgs.media.charsetPropGlow	= trap_R_RegisterShaderNoMip( "menu/art/font1_prop_glo.tga" );
	cgs.media.charsetPropB		= trap_R_RegisterShaderNoMip( "menu/art/font2_prop.tga" );

	CG_RegisterCvars();

	CG_InitConsoleCommands();

	// OSP's client log (CG_PrintLog)
	if ( cg_clientLog.integer ) {
		trap_FS_FOpenFile( va( "client_logs/client%d.txt", cg_clientLog.integer ), &cgs.logFileHandle, FS_APPEND );
	}

	cg.weaponSelect = WP_MACHINEGUN;

	cgs.redflag = cgs.blueflag = -1; // For compatibily, default to unset for
	cgs.flagStatus = -1;
	// old servers

	// get the rendering configuration from the client system
	trap_GetGlconfig( &cgs.glconfig );
	cgs.screenXScale = cgs.glconfig.vidWidth / 640.0;
	cgs.screenYScale = cgs.glconfig.vidHeight / 480.0;

	// get the gamestate from the client system
	trap_GetGameState( &cgs.gameState );

	// check version
	s = CG_ConfigString( CS_GAME_VERSION );
	if ( strcmp( s, GAME_VERSION ) ) {
		CG_Error( "Client/Server game mismatch: %s/%s", GAME_VERSION, s );
	}

	s = CG_ConfigString( CS_LEVEL_START_TIME );
	cgs.levelStartTime = atoi( s );

	CG_ParseServerinfo();

	// OSP's server settings from its configstrings (as in
	// CG_ConfigStringModified), MOTD, custom graphics and decals
	CG_Printf( "\n>>> ^3Loaded OSP Client Version: ^5%s\n\n", "1.03" );
	CG_OSPPrintTime_f();
	CG_Printf( "------------------------------------------------\n" );

	s = CG_ConfigString( 806 );
	cgs.server_mode = ( atoi( s ) & 1 ) ? 1 : ( ( atoi( s ) & 2 ) ? 2 : 0 );
	CPM_UpdateSettings( atoi( s ) );

	forceModelModificationCount = handicap.integer;
	cg.osp_unk_1e8c4 = 0;
	cgs.pmSkinColors = 0;

	cgs.custom_client_2 = atoi( CG_ConfigString( 807 ) );
	cgs.configString_0x368 = atoi( CG_ConfigString( 872 ) ) & 1;
	cgs.allow_pmove = atoi( CG_ConfigString( 809 ) );
	cgs.serverConfigMinimumMaxpackets = atoi( CG_ConfigString( 810 ) );
	cgs.serverConfigMaximumMaxpackets = atoi( CG_ConfigString( 811 ) );
	cgs.serverConfigMinimumTimenudge = atoi( CG_ConfigString( 812 ) );
	cgs.serverConfigMaximumTimenudge = atoi( CG_ConfigString( 813 ) );
	cgs.clanBaseTeamDM = atoi( CG_ConfigString( 815 ) ) ? qtrue : qfalse;
	cgs.gameTypeFreeze = ( atoi( CG_ConfigString( 873 ) ) == 0 ) ? 0 : ( ( atoi( CG_ConfigString( 873 ) ) == 2 ) ? 2 : 1 );

	CG_OSPCvarsRestrictValues();

	cgs.numberOfStringsMotd = 0;
	for ( i = 0; i < 8; i++ ) {
		s = CG_ConfigString( 816 + i );
		if ( !s || !*s ) {
			break;
		}
		Q_strncpyz( cgs.motd[cgs.numberOfStringsMotd], s, 64 );
		cgs.numberOfStringsMotd++;
	}

	cgs.custom_gfx_number = 0;
	for ( i = 0; i < MAX_OSP_CUSTOM_GFX; i++ ) {
		char	*ptr;
		char	buf[MAX_STRING_CHARS];

		s = CG_ConfigString( 824 + i );
		if ( !s || !*s ) {
			break;
		}
		Q_strncpyz( buf, s, MAX_STRING_CHARS );
		ptr = strchr( buf, ':' );
		if ( !ptr ) {
			CG_Printf( "Malformed custom gfx entry: %s\n", buf );
			break;
		}
		*ptr = 0;
		ptr++;
		sscanf( buf, "%i %i %i %i",
			&cgs.custom_gfx[cgs.custom_gfx_number][0], &cgs.custom_gfx[cgs.custom_gfx_number][1],
			&cgs.custom_gfx[cgs.custom_gfx_number][2], &cgs.custom_gfx[cgs.custom_gfx_number][3] );
		sscanf( ptr, "%i %i %i %i",
			&cgs.custom_gfx[cgs.custom_gfx_number][4], &cgs.custom_gfx[cgs.custom_gfx_number][5],
			&cgs.custom_gfx[cgs.custom_gfx_number][6], &cgs.custom_gfx[cgs.custom_gfx_number][7] );
		if ( cgs.custom_gfx[cgs.custom_gfx_number][0] == -1 && cgs.custom_gfx[cgs.custom_gfx_number][1] == -1 ) {
			break;
		}
		cgs.custom_gfx_number++;
	}
	CG_Printf( "\n>>> %d custom graphics specified\n\n", cgs.custom_gfx_number );

	// load the new map
	CG_LoadingString( "collision map" );

	trap_CM_LoadMap( cgs.mapname );

	cg.loading = qtrue;		// force players to load instead of defer

	CG_LoadingString( "sounds" );

	CG_RegisterSounds();

	CG_LoadingString( "graphics" );

	CG_RegisterGraphics();

	CG_LoadingString( "clients" );

	CG_RegisterClients();		// if low on memory, some clients will be deferred

	cg.loading = qfalse;	// future players will be deferred

	CG_InitLocalEntities();

	CG_InitMarkPolys();

	CG_InitCTFLocations();	// OSP

	// remove the last loading update
	cg.infoScreenText[0] = 0;

	// Make sure we have update values (scores)
	CG_SetConfigValues();

	CG_StartMusic();

	CG_LoadingString( "" );

	CG_ShaderStateChanged();

	trap_S_ClearLoopingSounds( qtrue );

	// OSP
	VectorClear( cgs.enemyRailColor );
	CHS_Init();
	CG_CustomLocationsLoad();

	cgs.numDecals = 0;
	if ( cg_drawDecals.integer ) {
		for ( i = 0; i < MAX_OSP_DECALS; i++ ) {
			cgs.decals[i].shader = 0;
			cgs.decals[i].enabled = 0;
			cgs.decals[i].rotation = 0;
			cgs.decals[i].angle = 0;
			cgs.decals[i].size = 30;
			cgs.decals[i].origin[0] = cgs.decals[i].origin[1] = cgs.decals[i].origin[2] = 0;
			cgs.decals[i].dir[0] = cgs.decals[i].dir[1] = cgs.decals[i].dir[2] = 0;

			s = CG_ConfigString( 840 + i );
			if ( !s || !*s ) {
				continue;
			}
			sscanf( s, "%i %i %f %f %f %f %f %f %f",
				&cgs.decals[cgs.numDecals].shader, &cgs.decals[cgs.numDecals].size,
				&cgs.decals[cgs.numDecals].angle, &cgs.decals[cgs.numDecals].origin[0],
				&cgs.decals[cgs.numDecals].origin[1], &cgs.decals[cgs.numDecals].origin[2],
				&cgs.decals[cgs.numDecals].dir[0], &cgs.decals[cgs.numDecals].dir[1],
				&cgs.decals[cgs.numDecals].dir[2] );
			if ( !i && cgs.decals[0].shader == -1 ) {
				CG_Printf( "*** Decals disabled\n" );
				continue;
			}
			if ( !cgs.decals[i].size ) {
				continue;
			}
			if ( cgs.decals[cgs.numDecals].shader < cgs.decalOSPShaderCopyCount ) {
				int		tmp;

				tmp = cgs.decals[cgs.numDecals].size;
				cgs.decals[cgs.numDecals].enabled = 1;
				tmp = ( tmp < 800 ) ? tmp : 800;
				tmp = ( tmp > 10 ) ? tmp : 10;
				cgs.decals[cgs.numDecals].size = tmp;
				cgs.numDecals++;
			} else {
				CG_Printf( "^3** Illegal decal graphic %d referenced ... skipping\n", cgs.decals[cgs.numDecals].shader );
			}
		}
	}

	if ( cg_execVstr.string && cg_execVstr.string[0] ) {
		trap_SendConsoleCommand( va( "vstr %s;", cg_execVstr.string ) );
	}

	return 0;
}

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
void CG_Shutdown( void ) {
	// some mods may need to do cleanup work here,
	// like closing files or archiving session data
}


/*
==================
CG_EventHandling
==================
 type 0 - no event handling
      1 - team menu
      2 - hud editor

*/
#ifndef MISSIONPACK
void CG_EventHandling(int type) {
}



void CG_KeyEvent(int key, qboolean down) {
}

void CG_MouseEvent(int x, int y) {
}
#endif


/*
 * cg_osp_cpmhud.c: the CPM-style HUD (ch_statusbarStyle 0-5, ch_weaponList, hudstyle;
 * Docs/CPM-info.txt) with its own copies of cg_draw.c functions.
 */
#include "cg_local.h"

// the flash phase of
// a newly picked up weapon in the weapon lists
float	wepSwitchFlash;

extern int	osp_frameTime;	// cg_draw.c (local extern: keeps cg_draw.c's bss order)

// OSP: the CPM HUD cvars (cvarTable entries); their externs in cg_local.h
// set their bss order
vmCvar_t	ch_crosshairTeamInfo;
vmCvar_t	ch_drawPickup;
vmCvar_t	ch_fragMessage;
vmCvar_t	ch_graphs;
vmCvar_t	ch_statusbarStyle;
vmCvar_t	ch_teamBackground;
vmCvar_t	ch_weaponList;
vmCvar_t	ch_weaponListBackground;
vmCvar_t	ch_weaponListDrawAll;
vmCvar_t	ch_weaponListFlash;

// OSP: this file's data: the two
// file-scope ints below, then the statics, tables and jump tables of the functions in order.
int		cpmDrawPowerups = 1;		// CG_OSPDrawCPMHUD stores CG_OSPDrawCPMWeaponList's result; gates CG_OSPDrawCPMHoldableItem and the powerups in CG_OSPDrawCPMLowerRight
int		cpmDrawWeaponSelect = 1;	// id's CG_DrawWeaponSelect is drawn while set; the CPM weapon bars clear it, weapon lists 6 / 7 set it (also CG_OSPHUDRoutine)

// CG_Draw3DModel with a colour for the model's shaderRGBA (NULL: none)
void CG_OSPDraw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles, const float *color ) {
	refdef_t		refdef;
	refEntity_t		ent;

	if ( !cg_draw3dIcons.integer || !cg_drawIcons.integer ) {
		return;
	}

	CG_AdjustFrom640( &x, &y, &w, &h );

	memset( &refdef, 0, sizeof( refdef ) );

	memset( &ent, 0, sizeof( ent ) );
	AnglesToAxis( angles, ent.axis );
	VectorCopy( origin, ent.origin );
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;		// no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	if ( color ) {
		ent.shaderRGBA[0] = 255 * color[0];
		ent.shaderRGBA[1] = 255 * color[1];
		ent.shaderRGBA[2] = 255 * color[2];
		ent.shaderRGBA[3] = 255;
	}

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene( &ent );
	trap_R_RenderScene( &refdef );
}

// CG_DrawHead with the colour passed on to CG_OSPDraw3DModel
void CG_OSPDrawPlayerIcon( float x, float y, float w, float h, int clientNum, vec3_t angles, float *color ) {
	clipHandle_t	cm;
	clientInfo_t	*ci;
	float			len;
	vec3_t			origin;
	vec3_t			mins, maxs;

	ci = &cgs.clientinfo[ clientNum ];

	if ( cg_draw3dIcons.integer ) {
		cm = ci->headModel;
		if ( !cm ) {
			return;
		}

		// offset the origin y and z to center the head
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the head nearly fills the box
		// assume heads are taller than wide
		len = 0.7 * ( maxs[2] - mins[2] );
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		// allow per-model tweaking
		VectorAdd( origin, ci->headOffset, origin );

		CG_OSPDraw3DModel( x, y, w, h, ci->headModel, ci->headSkin, origin, angles, color );
	} else if ( cg_drawIcons.integer ) {
		CG_DrawPic( x, y, w, h, ci->modelIcon );
	}

	// if they are deferred, draw a cross out
	if ( ci->deferred ) {
		CG_DrawPic( x, y, w, h, cgs.media.deferShader );
	}
}

// CG_DrawFlagModel for the red (team 1) or blue flag, with OSP's slower spin
void CG_OSPDrawFlagModel( float x, float y, float w, float h, int team ) {
	qhandle_t		cm;
	float			len;
	vec3_t			origin, angles;
	vec3_t			mins, maxs;

	if ( cg_draw3dIcons.integer ) {

		VectorClear( angles );

		cm = cgs.media.redFlagModel;

		// offset the origin y and z to center the flag
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len = 0.5 * ( maxs[2] - mins[2] );
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		angles[YAW] = 60 * sin( cg.time / 2000.0 );

		CG_OSPDraw3DModel( x, y, w, h, ( team == TEAM_RED ) ? cgs.media.redFlagModel : cgs.media.blueFlagModel, 0, origin, angles, NULL );
	} else if ( cg_drawIcons.integer ) {
		gitem_t *item;

		item = BG_FindItemForPowerup( ( team == TEAM_RED ) ? PW_REDFLAG : PW_BLUEFLAG );
		CG_DrawPic( x, y, w, h, cg_items[ ITEM_INDEX(item) ].icon );
	}
}

// CG_DrawStatusBarHead: the own head, kicked by damage, through CG_OSPDrawPlayerIcon in the
// own colour1
void CG_OSPDrawCPMStatusbar0Attacker( float x ) {
	vec3_t		angles;
	float		size, stretch;
	float		frac;

	VectorClear( angles );

	if ( cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME ) {
		frac = (float)(cg.time - cg.damageTime ) / DAMAGE_TIME;
		size = ICON_SIZE * 1.25 * ( 1.5 - frac * 0.5 );

		stretch = size - ICON_SIZE * 1.25;
		// kick in the direction of damage
		x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

		cg.headStartYaw = 180 + cg.damageX * 45;

		cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
		cg.headEndPitch = 5 * cos( crandom()*M_PI );

		cg.headStartTime = cg.time;
		cg.headEndTime = cg.time + 100 + random() * 2000;
	} else {
		if ( cg.time >= cg.headEndTime ) {
			// select a new head angle
			cg.headStartYaw = cg.headEndYaw;
			cg.headStartPitch = cg.headEndPitch;
			cg.headStartTime = cg.headEndTime;
			cg.headEndTime = cg.time + 100 + random() * 2000;

			cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
			cg.headEndPitch = 5 * cos( crandom()*M_PI );
		}

		size = ICON_SIZE * 1.25;
	}

	// if the server was frozen for a while we may have a bad head start time
	if ( cg.headStartTime > cg.time ) {
		cg.headStartTime = cg.time;
	}

	frac = ( cg.time - cg.headStartTime ) / (float)( cg.headEndTime - cg.headStartTime );
	frac = frac * frac * ( 3 - 2 * frac );
	angles[YAW] = cg.headStartYaw + ( cg.headEndYaw - cg.headStartYaw ) * frac;
	angles[PITCH] = cg.headStartPitch + ( cg.headEndPitch - cg.headStartPitch ) * frac;

	CG_OSPDrawPlayerIcon( x, 480 - size, size, size,
				cg.snap->ps.clientNum, angles, cgs.clientinfo[cg.snap->ps.clientNum].color1 );
}

// CG_DrawStatusBarFlag
void CG_OSPDrawStatusBarFlag( float x, int team ) {
	CG_DrawFlagModel( x, 480 - ICON_SIZE, ICON_SIZE, ICON_SIZE, team, qfalse );
}

// CG_DrawTeamBackground
void CG_OSPDrawTeamBackground( int x, int y, int w, int h, float alpha, int team ) {
	vec4_t		hcolor;

	hcolor[3] = alpha;
	if ( team == TEAM_RED ) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if ( team == TEAM_BLUE ) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	} else {
		return;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );
}

// one horizontal CPM bar (health or armour) for status bar style 1: the number at 265 (right
// aligned bar growing left) or 323 (growing right), a 200 pixel scale line with ch_graphs, the
// bar (two rows above 100) and the icon at its end
void CG_OSPDrawCPMStatusbarsBar( int value, qhandle_t icon, int y, int direction, float *color ) {
	int		tmpX;
	vec4_t	white;
	int		x;
	float	w;
	int		fieldY;

	white[0] = white[1] = white[2] = white[3] = 1.0f;
	fieldY = y;
	trap_R_SetColor( color );
	if ( direction ) {
		CG_DrawField( 265, fieldY + 6, 3, value, 16, 24 );
		x = 267;
	} else {
		CG_DrawField( 323, fieldY + 6, 3, value, 16, 24 );
		x = 373;
	}
	trap_R_SetColor( NULL );

	tmpX = x;
	if ( ch_graphs.integer ) {
		if ( direction ) {
			tmpX -= 202;
		} else {
			tmpX += 2;
		}
		CG_FillRect( tmpX, y + 16 + 2, 200, 1, white );
	}

	color[3] = 0.5f;
	if ( value && ch_graphs.integer ) {
		w = ( value > 100 ? 100 : value ) << 1;
		tmpX = x;
		if ( direction ) {
			tmpX -= w + 2;
		} else {
			tmpX += 2;
		}
		CG_FillRect( tmpX, y, w, 16, color );

		if ( value > 100 ) {
			w = ( value << 1 ) - 200;
			tmpX = x;
			if ( direction ) {
				tmpX -= w + 2;
			} else {
				tmpX += 2;
			}
			y += 21;
			CG_FillRect( tmpX, y, w, 16, color );
		}
		if ( direction ) {
			tmpX -= 20;
		} else {
			tmpX += w + 4;
		}
		if ( icon > 0 && cg_drawIcons.integer ) {
			CG_DrawPic( tmpX, y, 16, 16, icon );
		}
	} else if ( icon > 0 && cg_drawIcons.integer ) {
		tmpX = x;
		if ( direction ) {
			tmpX -= 22;
		} else {
			tmpX += 6;
		}
		CG_DrawPic( tmpX, y, 16, 16, icon );
	}
	color[3] = 1.0f;
}

// one bar of status bar style 5: the number left of x, a 96 pixel scale line with ch_graphs
// (without ch_graphs only the icon), the bar growing left with a second row above 100
// (flag: clamped to 200) and the icon at its end
void CG_OSPDrawCPMStatusbar5Bars( int value, qhandle_t icon, int y, int x, float *color, qboolean flag ) {
	int		tmpX;
	vec4_t	white;
	int		offsetX;
	float	w;

	white[0] = white[1] = white[2] = white[3] = 1.0f;
	tmpX = x - 48;
	if ( value > -1 ) {
		trap_R_SetColor( color );
		CG_DrawField( tmpX, y + 6, 3, value, 16, 24 );
		trap_R_SetColor( NULL );
	}

	offsetX = tmpX - 3;

	if ( ch_graphs.integer && value > -1 ) {
		tmpX = offsetX - 98;
		CG_FillRect( tmpX, y + 16 + 2, 96, 1, white );
	} else {
		if ( icon > 0 && cg_drawIcons.integer ) {
			tmpX = offsetX - 16;
			CG_DrawPic( tmpX, y + 8 + 3, 16, 16, icon );
		}
		return;
	}

	color[3] = 0.5f;
	if ( flag && value > 200 ) {
		value = 200;
	}

	if ( value > 100 ) {
		w = value - 100;
		value = 100;
		tmpX = offsetX - w;
		CG_FillRect( tmpX, y + 16 + 4 + 1, w, 16, color );
		if ( icon > 0 && cg_drawIcons.integer ) {
			tmpX -= 20;
			if ( tmpX < offsetX - 100 ) {
				tmpX = offsetX - 100;
			}
			CG_DrawPic( tmpX, y + 16 + 4 + 1, 16, 16, icon );
			icon = 0;
		}
	}

	if ( value ) {
		tmpX = offsetX - value;
		CG_FillRect( tmpX, y, value, 16, color );
		if ( icon > 0 && cg_drawIcons.integer ) {
			tmpX -= 20;
			if ( tmpX < offsetX - 100 ) {
				tmpX = offsetX - 100;
			}
			CG_DrawPic( tmpX, y, 16, 16, icon );
		}
	} else if ( icon > 0 && cg_drawIcons.integer ) {
		tmpX = offsetX - 20;
		CG_DrawPic( tmpX, y, 16, 16, icon );
	}

	color[3] = 1.0f;
}

// health and armour bars of status bar style 3: a scale line, each bar growing left from x with
// its icon, and the numbers (10x15) at x
void CG_OSPDrawCPMStatusbar3Bars( int health, int armor, qhandle_t iconHealth, qhandle_t iconArmor, int x, int y, float *colorHealth, float *colorArmor ) {
	int		tmpX;
	vec4_t	color;

	color[0] = color[1] = color[2] = 1.0f;
	color[3] = 0.7f;
	if ( ch_graphs.integer ) {
		CG_FillRect( x - 102, y - 2, 1, 40, color );
	}

	tmpX = x;
	colorHealth[3] = 0.5f;
	if ( health && ch_graphs.integer ) {
		tmpX -= health + 2;
		CG_FillRect( tmpX, y, health, 16, colorHealth );
		tmpX -= 20;
		if ( iconHealth > 0 && cg_drawIcons.integer ) {
			CG_DrawPic( tmpX, y, 16, 16, iconHealth );
		}
	} else {
		if ( iconHealth > 0 && cg_drawIcons.integer ) {
			tmpX -= 11;
			CG_DrawPic( tmpX, y, 16, 16, iconHealth );
		}
	}
	colorHealth[3] = 1.0f;

	trap_R_SetColor( colorHealth );
	CG_DrawField( x, y, 3, health, 10, 15 );
	trap_R_SetColor( NULL );

	tmpX = x;
	colorArmor[3] = 0.5f;
	y += 21;

	if ( armor && ch_graphs.integer ) {
		tmpX -= armor + 2;
		CG_FillRect( tmpX, y, armor, 16, colorArmor );
		tmpX -= 20;
		// OSP tests the health icon here
		if ( iconHealth > 0 && cg_drawIcons.integer ) {
			CG_DrawPic( tmpX, y, 16, 16, iconArmor );
		}
	} else {
		if ( iconArmor > 0 && cg_drawIcons.integer ) {
			tmpX -= 11;
			CG_DrawPic( tmpX, y, 16, 16, iconArmor );
		}
	}

	colorArmor[3] = 1.0f;
	trap_R_SetColor( colorArmor );
	CG_DrawField( x, y, 3, armor, 10, 15 );
	trap_R_SetColor( NULL );
}

// one column of status bar style 4: the 32x32 icon, the number above it and the value as up to
// four vertical bars of 50 (2 pixels per point)
void CG_OSPDrawCPMStatusbar4Bars( int x, int y, int value, qhandle_t icon, float *color ) {
	int		reminder;

	if ( cg_drawIcons.integer ) {
		y -= 32;
		if ( icon > 0 ) {
			CG_DrawPic( x, y, 32, 32, icon );
		}
		y -= 5;
	}
	if ( value < 0 ) {
		return;
	}

	y -= 16;
	trap_R_SetColor( color );
	CG_DrawField( x + 2, y + 1, 3, value, 10, 15 );
	trap_R_SetColor( NULL );
	color[3] = 0.5f;

	if ( value > 150 ) {
		reminder = value - 150;
		if ( ch_graphs.integer ) {
			CG_FillRect( x - 2, y - ( reminder << 1 ), 8, reminder << 1, color );
		}
		value = 150;
	}
	if ( value > 100 ) {
		reminder = value - 100;
		if ( ch_graphs.integer ) {
			CG_FillRect( x + 7, y - ( reminder << 1 ), 8, reminder << 1, color );
		}
		value = 100;
	}
	if ( value > 50 ) {
		reminder = value - 50;
		if ( ch_graphs.integer ) {
			CG_FillRect( x + 17, y - ( reminder << 1 ), 8, reminder << 1, color );
		}
		value = 50;
	}
	if ( value && ch_graphs.integer ) {
		CG_FillRect( x + 26, y - ( value << 1 ), 8, value << 1, color );
	}
	color[3] = 1.0f;
}

// status bar style 0 (CPM's id-like bar): ammo, head, flag, health and armour numbers with the
// rotating 3D ammo / armour models (the armour model and icon follow the CPM armour class)
void CG_OSPDrawCPMStatusbar0( void ) {
	int			color;
	centity_t	*cent;
	playerState_t	*ps;
	int			value;
	vec4_t		hcolor;
	vec3_t		angles;
	vec3_t		origin;
	qhandle_t	icon;
	static float colors[4][4] = {
		{ 1.0f, 0.7f, 0.0f, 1.0f },		// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	VectorClear( angles );

	// draw any 3D icons first, so the changes back to 2D are minimized
	if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
		origin[0] = 70;
		origin[1] = 0;
		origin[2] = 0;
		angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
		CG_OSPDraw3DModel( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
					   cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles, NULL );
	}

	CG_OSPDrawCPMStatusbar0Attacker( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE );

	if( cg.predictedPlayerState.powerups[PW_REDFLAG] ) {
		CG_OSPDrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_RED );
	} else if( cg.predictedPlayerState.powerups[PW_BLUEFLAG] ) {
		CG_OSPDrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_BLUE );
	}

	if ( ps->stats[ STAT_ARMOR ] ) {
		qhandle_t	model;

		model = ( &cgs.media.armorModelGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
		origin[0] = 90;
		origin[1] = 0;
		origin[2] = -10;
		angles[YAW] = ( cg.time & 2047 ) * 360 / 2048.0;
		CG_OSPDraw3DModel( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
					   model, 0, origin, angles, NULL );
	}
	//
	// ammo
	//
	if ( cent->currentState.weapon ) {
		value = ps->ammo[cent->currentState.weapon];
		if ( value > -1 ) {
			if ( value >= 0 ) {
				color = 0;	// green
			} else {
				color = 1;	// red
			}
			trap_R_SetColor( colors[color] );

			CG_DrawField (0, 432, 3, value, 32, 48);
			trap_R_SetColor( NULL );

			// if we didn't draw a 3D icon, draw a 2D icon for ammo
			if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
				qhandle_t	icon;

				icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
				if ( icon ) {
					CG_DrawPic( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, icon );
				}
			}
		}
	}

	//
	// health
	//
	value = ps->stats[STAT_HEALTH];
	if ( value > 100 ) {
		trap_R_SetColor( colors[3] );		// white
	} else if (value > 30) {
		trap_R_SetColor( colors[0] );	// green
	} else if (value > 0) {
		color = (cg.time >> 8) & 1;	// flash
		trap_R_SetColor( colors[color] );
	} else {
		trap_R_SetColor( colors[1] );	// red
	}

	// stretch the health up when taking damage
	CG_DrawField ( 185, 432, 3, value, 32, 48);
	CG_ColorForHealth( hcolor );
	trap_R_SetColor( hcolor );

	//
	// armor
	//
	value = ps->stats[STAT_ARMOR];
	if (value > 0 ) {
		trap_R_SetColor( colors[0] );
		CG_DrawField (370, 432, 3, value, 32, 48);
		trap_R_SetColor( NULL );
		// if we didn't draw a 3D icon, draw a 2D icon for armor
		if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
			icon = ( &cgs.media.armorIconGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
			CG_DrawPic( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, icon );
		}
	}
}

// status bar style 1: horizontal health (left, with the model icon blinking at 30 and below)
// and armour (right) bars from the centre of the screen
void CG_OSPDrawCPMStatusbarBars( void ) {
	int			value;
	int			color;
	playerState_t	*ps;
	qhandle_t	icon;
	static float colors[4][4] = {
		{ 1.0f, 0.7f, 0.0f, 1.0f },		// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing (unused)
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	ps = &cg.snap->ps;
	value = ps->stats[STAT_HEALTH];
	if ( value > 30 || cg.time % 1000 < 500 ) {
		icon = cgs.clientinfo[cg.snap->ps.clientNum].modelIcon;
	} else {
		icon = -1;
	}
	if ( value > 100 ) {
		color = 3;
	} else if ( value > 30 ) {
		color = 0;
	} else {
		color = 1;
	}
	CG_OSPDrawCPMStatusbarsBar( value, icon, 432, qtrue, colors[color] );

	value = ps->stats[STAT_ARMOR];
	if ( value > 100 ) {
		color = 3;
	} else {
		color = 0;
	}
	icon = ( &cgs.media.armorIconGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
	CG_OSPDrawCPMStatusbarsBar( value, icon, 432, qfalse, colors[color] );
	CG_FillRect( 320, 432, 1, 32, colors[3] );
}

// status bar style 2: health and armour bars (CG_OSPDrawCPMStatusbar3Bars) and the ammo number
// with the rotating 3D ammo model (none with ch_weaponList 3)
void CG_OSPDrawCPMStatusbar3( void ) {
	int			value;
	playerState_t	*ps;
	vec3_t		angles;
	centity_t	*cent;
	int			armor;
	int			color;
	int			armorColor;
	qhandle_t	icon;
	qhandle_t	armorIcon;
	vec3_t		origin;
	static float colors[4][4] = {
		{ 1.0f, 0.7f, 0.0f, 1.0f },		// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing (unused)
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	VectorClear( angles );

	value = ps->stats[STAT_HEALTH];
	if ( value > 30 || cg.time % 1000 < 500 ) {
		icon = cgs.clientinfo[cg.snap->ps.clientNum].modelIcon;
	} else {
		icon = -1;
	}
	if ( value > 100 ) {
		color = 3;
	} else if ( value > 30 ) {
		color = 0;
	} else {
		color = 1;
	}
	armor = ps->stats[STAT_ARMOR];
	if ( armor > 100 ) {
		armorColor = 3;
	} else {
		armorColor = 0;
	}
	armorIcon = ( &cgs.media.armorIconGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
	CG_OSPDrawCPMStatusbar3Bars( value, armor, icon, armorIcon, 225, 432, colors[color], colors[armorColor] );

	if ( ch_weaponList.integer == 3 ) {
		return;
	}

	if ( cent->currentState.weapon ) {
		if ( cg_weapons[ cent->currentState.weapon ].ammoModel ) {
			origin[0] = 70;
			origin[1] = 0;
			origin[2] = 0;
			angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
			CG_OSPDraw3DModel( 380, 427, ICON_SIZE, ICON_SIZE,
						   cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles, NULL );
		}

		value = ps->ammo[cent->currentState.weapon];
		if ( value > -1 ) {
			if ( value >= 0 ) {
				color = 0;
			} else {
				color = 1;
			}
			trap_R_SetColor( colors[color] );
			CG_DrawField( 280, 427, 3, value, 32, 48 );
			trap_R_SetColor( NULL );

			if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
				qhandle_t	ammoIcon;

				ammoIcon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
				if ( ammoIcon ) {
					CG_DrawPic( 380, 427, ICON_SIZE, ICON_SIZE, ammoIcon );
				}
			}
		}
	}
}

// status bar style 3: health, armour and (unless ch_weaponList 1 / 3) ammo columns
// (CG_OSPDrawCPMStatusbar4Bars)
void CG_OSPDrawCPMStatusbar4( void ) {
	int			value;
	qhandle_t	icon;
	int			color;
	playerState_t	*ps;
	centity_t	*cent;
	static float colors[4][4] = {
		{ 1.0f, 0.7f, 0.0f, 1.0f },		// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing (unused)
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	value = ps->stats[STAT_HEALTH];
	if ( value > 30 || cg.time % 1000 < 500 ) {
		icon = cgs.clientinfo[cg.snap->ps.clientNum].modelIcon;
	} else {
		icon = -1;
	}
	if ( value > 100 ) {
		color = 3;
	} else if ( value > 30 ) {
		color = 0;
	} else {
		color = 1;
	}
	CG_OSPDrawCPMStatusbar4Bars( 5, 470, value, icon, colors[color] );

	value = ps->stats[STAT_ARMOR];
	if ( value > 100 ) {
		color = 3;
	} else {
		color = 0;
	}
	icon = ( &cgs.media.armorIconGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
	CG_OSPDrawCPMStatusbar4Bars( 62, 470, value, icon, colors[color] );

	if ( ch_weaponList.integer == 3 || ch_weaponList.integer == 1 ) {
		return;
	}

	value = ps->ammo[cent->currentState.weapon];
	if ( value > -1 ) {
		if ( value >= 0 ) {
			color = 0;
		} else {
			color = 1;
		}
	}
	icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
	CG_OSPDrawCPMStatusbar4Bars( 119, 470, value, icon, colors[color] );
}

// status bar style 5: health, armour and ammo bars side by side (CG_OSPDrawCPMStatusbar5Bars;
// narrower without ch_graphs) with separator lines
void CG_OSPDrawCPMStatusbar5( void ) {
	int			value;
	qhandle_t	icon;
	int			color;
	playerState_t	*ps;
	centity_t	*cent;
	static float colors[4][4] = {
		{ 1.0f, 0.7f, 0.0f, 1.0f },		// normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },		// low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },		// weapon firing (unused)
		{ 1.0f, 1.0f, 1.0f, 1.0f } };	// health > 100

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	value = ps->stats[STAT_HEALTH];
	if ( value > 30 || cg.time % 1000 < 500 ) {
		icon = cgs.clientinfo[cg.snap->ps.clientNum].modelIcon;
	} else {
		icon = -1;
	}
	if ( value > 100 ) {
		color = 3;
	} else if ( value > 30 ) {
		color = 0;
	} else {
		color = 1;
	}
	CG_OSPDrawCPMStatusbar5Bars( value, icon, 432, !ch_graphs.integer ? 275 : 220, colors[color], qfalse );

	value = ps->stats[STAT_ARMOR];
	if ( value > 100 ) {
		color = 3;
	} else {
		color = 0;
	}
	icon = ( &cgs.media.armorIconGreen )[ ps->stats[ STAT_ARMOR_TYPE ] ];
	CG_OSPDrawCPMStatusbar5Bars( value, icon, 432, !ch_graphs.integer ? 350 : 390, colors[color], qfalse );

	value = ps->ammo[cent->currentState.weapon];
	if ( value > -1 ) {
		if ( value >= 0 ) {
			color = 0;
		} else {
			color = 1;
		}
	}
	icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
	CG_OSPDrawCPMStatusbar5Bars( value, icon, 432, !ch_graphs.integer ? 425 : 560, colors[color], qtrue );

	if ( ch_graphs.integer ) {
		CG_FillRect( 230, 432, 1, 35, colors[3] );
		CG_FillRect( 400, 432, 1, 35, colors[3] );
	} else {
		CG_FillRect( 280, 432, 1, 35, colors[3] );
		CG_FillRect( 355, 432, 1, 35, colors[3] );
	}
}

// the CPM status bar (cg_drawStatus): the team background (ch_teamBackground) and the bar of
// ch_statusbarStyle 0..5 (style 4: the style 0 bar, not drawn with a weapon list)
void CG_OSPDrawCPMStatusbar( void ) {
	if ( !cg_drawStatus.integer ) {
		return;
	}
	if ( ch_statusbarStyle.integer == 4 && ch_weaponList.integer ) {
		return;
	}
	if ( ch_teamBackground.integer ) {
		CG_DrawTeamBackground( 0, 420, 640, 60, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );
	}
	if ( ch_statusbarStyle.integer == 0 || ch_statusbarStyle.integer == 4 ) {
		CG_OSPDrawCPMStatusbar0();
	} else if ( ch_statusbarStyle.integer == 1 ) {
		CG_OSPDrawCPMStatusbarBars();
	} else if ( ch_statusbarStyle.integer == 2 ) {
		CG_OSPDrawCPMStatusbar3();
	} else if ( ch_statusbarStyle.integer == 3 ) {
		CG_OSPDrawCPMStatusbar4();
	} else if ( ch_statusbarStyle.integer == 5 ) {
		CG_OSPDrawCPMStatusbar5();
	}
}

// the weapon list / weapon bar tables and the colours of the
// 6 / 7 weapon lists. CG_OSPDrawCPMWeaponList's jump table
// and CG_OSPSetHUDStyle's style table follow with their functions.
vec4_t	wepSwitchColor1 = { 0.1f, 0.1f, 0.5f, 0.5f };	// background of the selected weapon (ch_weaponListBackground)
vec4_t	wepSwitchColor2 = { 1.0f, 0.75f, 1.0f, 1.0f };	// flash of a new weapon (ch_weaponListFlash); [2] / [3] fade with the time left
int		wepSwitch67[MAX_WEAPONS] = { 0 };						// per weapon: 1 = seen, 900 .. 2 = flash time left (ms), 0 = not held
vec4_t	powerupColors67[2] = {							// the powerup time colours of CG_OSPDrawPowerup67 (only [1] is used)
	{ 0.2f, 1.0f, 0.2f, 1.0f },
	{ 1.0f, 0.2f, 0.2f, 1.0f }
};
vec4_t	healthColors67[3] = {							// CG_OSPDrawHealthArmor67: normal, low (<= 30), over 100
	{ 1.0f, 0.7f, 0.0f, 1.0f },
	{ 1.0f, 0.2f, 0.2f, 1.0f },
	{ 1.0f, 1.0f, 1.0f, 1.0f }
};

/*
CG_OSPDrawWeaponSwitchAttacker: the attacker's head and name in the upper right corner (id's
CG_DrawAttacker with CG_OSPDrawPlayerIcon), below the weapon list 7
*/
float CG_OSPDrawWeaponSwitchAttacker( float y ) {
	int			t;
	float		size;
	vec3_t		angles;
	const char	*info;
	const char	*name;
	int			clientNum;

	if ( cg.predictedPlayerState.stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	if ( !cg.attackerTime ) {
		return y;
	}

	clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum ) {
		return y;
	}

	t = cg.time - cg.attackerTime;
	if ( t > ATTACKER_HEAD_TIME ) {
		cg.attackerTime = 0;
		return y;
	}

	size = 60;

	angles[PITCH] = 0;
	angles[YAW] = 180;
	angles[ROLL] = 0;
	CG_OSPDrawPlayerIcon( 640 - size, y, size, size, clientNum, angles, cgs.clientinfo[clientNum].color1 );

	info = CG_ConfigString( CS_PLAYERS + clientNum );
	name = Info_ValueForKey( info, "n" );
	y += size;
	CG_DrawBigString( 640 - ( CG_DrawStrlen( name ) * BIGCHAR_WIDTH ), y, name, 0.5 );

	return y + BIGCHAR_HEIGHT + 2;
}

/*
CG_OSPDrawCPMUpperRight: the CPM HUD's upper right corner (team overlay 1, snapshot, FPS, timer,
speed, lagometer 2 or ping, attacker); returns the y below it
*/
float CG_OSPDrawCPMUpperRight( void ) {
	float	y;

	y = 0;
	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1 ) {
		y = CG_DrawTeamOverlay( y, qtrue, qtrue );
	}
	if ( cg_drawSnapshot.integer ) {
		y = CG_DrawSnapshot( y );
	}
	if ( cg_drawFPS.integer ) {
		y = CG_DrawFPS( y );
	}
	if ( cg_drawTimer.integer ) {
		y = CG_DrawTimer( y );
	}
	if ( ch_drawSpeed.integer == 1 ) {
		y = CG_OSPDrawSpeed( y );
	}
	if ( cg_lagometer.integer > 1 ) {
		CG_DrawLagometer( y );
	} else if ( cg_lagometer.integer <= 0 && cg_drawPing.integer != 0 ) {
		y = CG_OSPDrawPing( y );
	}
	if ( cg_drawAttacker.integer && ch_weaponList.integer != 2 ) {
		y = CG_OSPDrawWeaponSwitchAttacker( y );
	}
	return y;
}

/*
CG_OSPDrawCPMLowerRight: the CPM HUD's lower right corner (team overlay 2, scores, powerups
unless the weapon list 6 / 7 shows them)
*/
void CG_OSPDrawCPMLowerRight( void ) {
	float	y;

	if ( cg_lagometer.integer == 0 && ( ch_statusbarStyle.integer == 4 || ch_statusbarStyle.integer == 3 || cg_drawStatus.integer == 0 ) ) {
		y = 475;
	} else
		y = 432 - ( cg_lagometer.integer == 1 && cg_drawPing.integer > 0 ? 14 : 0 );

	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 2 ) {
		y = CG_DrawTeamOverlay( y, qtrue, qfalse );
	}
	y = CG_DrawScores( y );
	if ( cpmDrawPowerups ) {
		y = CG_DrawPowerups( y );
	}
}

/*
CG_OSPDrawCPMPickupItem: the picked up item's icon and name above y (ch_drawPickup); returns y
*/
int CG_OSPDrawCPMPickupItem( int y ) {
	int		value;
	int		size;
	int		x;
	float	*fadeColor;

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 || ch_drawPickup.integer == 0 ) {
		return y;
	}

	y -= ICON_SIZE;

	if ( ch_statusbarStyle.integer == 3 ) {
		x = 150;
		size = 32;
	} else {
		x = 0;
		size = ICON_SIZE;
	}

	value = cg.itemPickup;
	if ( value ) {
		fadeColor = CG_FadeColor( cg.itemPickupTime, 3000 );
		if ( fadeColor ) {
			CG_RegisterItemVisuals( value );
			trap_R_SetColor( fadeColor );
			CG_DrawPic( x + 8, y, size, size, cg_items[ value ].icon );
			CG_DrawBigString( x + ICON_SIZE + 16, y + ( ICON_SIZE / 2 - BIGCHAR_HEIGHT / 2 ), bg_itemlist[ value ].pickup_name, fadeColor[0] );
			trap_R_SetColor( NULL );
		}
	}

	return y;
}

/*
CG_OSPDrawCPMLowerLeft: the CPM HUD's lower left corner (team overlay 3, picked up item)
*/
void CG_OSPDrawCPMLowerLeft( void ) {
	float	y;

	if ( ch_statusbarStyle.integer == 4 || cg_drawStatus.integer == 0 ) {
		y = 475;
	} else {
		y = 432;
	}

	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 3 ) {
		y = CG_DrawTeamOverlay( y, qfalse, qfalse );
	}

	y = CG_OSPDrawCPMPickupItem( y );
}

/*
CG_OSPDrawSplitter: a 76x1 white line between the blocks of the weapon lists 6 / 7
*/
float CG_OSPDrawSplitter( float x, float y ) {
	CG_DrawPic( x, y, 76, 1, cgs.media.whiteShader );
	y += 4;
	return y;
}

/*
CG_OSPDrawWeapons67: the weapon column of the weapon lists 6 / 7 (icon and ammo per weapon, the
selected one bigger; new weapons flash), then the holdable item
*/
float CG_OSPDrawWeapons67( float x, float y ) {
	int			weaponNum;
	int			size;
	int			ammo;
	vec4_t		color;
	qhandle_t	item;
	float		h;
	float		w;
	char		str[10];

	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weaponNum == 10 ) {
			continue;
		}
		if ( ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) || ( ch_weaponListDrawAll.integer && cg_weapons[weaponNum].registered ) ) {
			ammo = cg.snap->ps.ammo[weaponNum];
			color[0] = 1.0f;
			color[1] = color[2] = ( ammo < 10 ) ? 0.3f : 1.0f;
			item = cg_weapons[weaponNum].ammoIcon;
			if ( cg.weaponSelect == weaponNum ) {
				size = 24;
				w = 16;
				h = 16;
				wepSwitch67[weaponNum] = 1;
				if ( ch_weaponListBackground.integer ) {
					CG_FillRect( x, y, 82, 24, wepSwitchColor1 );
				}
			} else {
				size = 16;
				w = 8;
				h = 8;
				if ( wepSwitch67[weaponNum] == 0 && ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
					wepSwitch67[weaponNum] = 900;
				}
				if ( wepSwitch67[weaponNum] > 1 ) {
					wepSwitchFlash = wepSwitch67[weaponNum] % 300;
					wepSwitchColor2[2] = wepSwitchFlash / 600.0f;
					wepSwitchColor2[3] = wepSwitchFlash / 300.0f;
					if ( ch_weaponListFlash.integer ) {
						CG_FillRect( x, y, 76, 16, wepSwitchColor2 );
					}
					if ( ( wepSwitch67[weaponNum] -= osp_frameTime ) < 1 ) {
						wepSwitch67[weaponNum] = 1;
					}
				}
			}
			CG_DrawPic( x + 4, y, size, size, item );
			if ( ammo == 0 || ( ch_weaponListDrawAll.integer && !( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) ) {
				CG_DrawPic( x + 4, y, size, size, cgs.media.noammoShader );
			}
			if ( cg.weaponSelect == weaponNum ) {
				color[3] = 1.0f;
			} else {
				color[3] = 0.5f;
			}
			if ( ammo >= 0 ) {
				Com_sprintf( str, 10, "%i", ammo );
				CG_DrawStringExt( x + 8 + size, y + ( size / 2 - h / 2 ), str, color, qfalse, qtrue, w, h, 3 );
			}
			y += size + 4;
		}
		if ( !( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
			wepSwitch67[weaponNum] = 0;
		}
	}

	ammo = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
	if ( ammo > 0 && ammo < bg_numItems ) {
		CG_RegisterItemVisuals( ammo );
		size = 16;
		CG_DrawPic( x + 4, y, size, size, cg_items[ammo].icon );
		y += size + 4;
	}

	y = CG_OSPDrawSplitter( x, y );
	return y;
}

/*
CG_OSPDrawPowerup67: the powerups of the weapon lists 6 / 7, sorted by the time left (id's
CG_DrawPowerups list with its pulse), each with its seconds
*/
float CG_OSPDrawPowerup67( float x, float y ) {
	int		sorted[MAX_POWERUPS];
	int		sortedTime[MAX_POWERUPS];
	int		i, j, k;
	int		active;
	playerState_t	*ps;
	int		t;
	gitem_t	*item;
	float	size;
	float	f;
	int		iconSize;
	int		colorIndex;
	char	num[10];

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	// sort the list by time remaining
	active = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( !ps->powerups[ i ] ) {
			continue;
		}
		t = ps->powerups[ i ] - cg.time;
		// ZOID--don't draw if the power up has unlimited time (999 seconds)
		// This is true of the CTF flags
		if ( t < 0 || t > 999000) {
			continue;
		}

		// insert into the list
		for ( j = 0 ; j < active ; j++ ) {
			if ( sortedTime[j] >= t ) {
				for ( k = active - 1 ; k >= j ; k-- ) {
					sorted[k+1] = sorted[k];
					sortedTime[k+1] = sortedTime[k];
				}
				break;
			}
		}
		sorted[j] = i;
		sortedTime[j] = t;
		active++;
	}

	// draw the icons and timers
	iconSize = ICON_SIZE / 2;
	for ( i = 0 ; i < active ; i++ ) {
		item = BG_FindItemForPowerup( sorted[i] );

		if ( cg.powerupActive == sorted[i] && cg.time - cg.powerupTime < PULSE_TIME ) {
			f = 1.0 - ( ( (float)cg.time - cg.powerupTime ) / PULSE_TIME );
			size = 24 * ( 0.5 * f + 1.0 );
		} else {
			size = 24;
		}

		CG_DrawPic( x + 4, y - ( size - 24 ) / 2, size, size, trap_R_RegisterShader( item->icon ) );
		Com_sprintf( num, sizeof( num ), "%i", sortedTime[i] / 1000 );
		colorIndex = 1;
		CG_DrawStringExt( x + 24, y + 4, num, powerupColors67[colorIndex], qfalse, qtrue, 16, 16, 2 );
		y += 28;
	}

	if ( active ) {
		y = CG_OSPDrawSplitter( x, y );
	}

	return y;
}

/*
CG_OSPDrawCustomPowerup: a 24x24 powerup / flag icon (blinking when asked), framed while the
own player has the powerup
*/
float CG_OSPDrawCustomPowerup( float x, float y, int powerup, int blink, qhandle_t shader ) {
	gitem_t	*item;

	item = BG_FindItemForPowerup( powerup );
	if ( item ) {
		if ( !blink || cg.time % 1000 < 500 ) {
			CG_DrawPic( x, y, 24, 24, shader );
		}
		if ( cg.predictedPlayerState.powerups[powerup] ) {
			CG_DrawPic( x, y, 24, 24, cgs.media.selectShader );
		}
	}
	return y;
}

/*
CG_OSPDrawFlags67: the CTF flag states (or the carried flag) at the top of the weapon lists 6 / 7
*/
float CG_OSPDrawFlags67( float x, float y ) {
	if ( cgs.gametype == GT_CTF ) {
		if ( cgs.blueflag < 0 || cgs.blueflag > 2 ) {
			if ( cg.predictedPlayerState.powerups[PW_BLUEFLAG] ) {
				CG_OSPDrawCustomPowerup( x + 4, y, PW_BLUEFLAG, 1, cgs.media.blueFlagShader[1] );
				y += 28;
				y = CG_OSPDrawSplitter( x, y );
			} else if ( cg.predictedPlayerState.powerups[PW_REDFLAG] ) {
				CG_OSPDrawCustomPowerup( x + 4, y, PW_REDFLAG, 1, cgs.media.redFlagShader[1] );
				y += 28;
				y = CG_OSPDrawSplitter( x, y );
			}
			return y;
		}
		CG_OSPDrawCustomPowerup( x + 4, y, PW_BLUEFLAG, cgs.blueflag, cgs.media.blueFlagShader[cgs.blueflag] );
		CG_OSPDrawCustomPowerup( x + 20 + 24, y, PW_REDFLAG, cgs.redflag, cgs.media.redFlagShader[cgs.redflag] );
		y += 28;
		y = CG_OSPDrawSplitter( x, y );
	}
	return y;
}

/*
CG_OSPDrawHealthArmor67: health and armor of the weapon list 6 (ch_statusbarStyle 4 only)
*/
float CG_OSPDrawHealthArmor67( float x, float y ) {
	int			value;
	int			colorIndex;
	qhandle_t	armorIcon;

	armorIcon = ( &cgs.media.armorIconGreen )[ cg.snap->ps.stats[STAT_ARMOR_TYPE] ];
	if ( ch_statusbarStyle.integer != 4 ) {
		return y;
	}

	value = cg.snap->ps.stats[STAT_HEALTH];
	if ( cg_drawIcons.integer )
		if ( value > 30 || cg.time % 1000 < 500 )
			CG_DrawPic( x + 4, y, 24, 24, cgs.clientinfo[cg.snap->ps.clientNum].modelIcon );
	colorIndex = value > 100 ? 2 : ( value > 30 ? 0 : 1 );
	trap_R_SetColor( healthColors67[colorIndex] );
	CG_DrawField( x + 4 + 24, y, 3, value, 16, 24 );
	y += 28;

	if ( cg_drawIcons.integer )
		CG_DrawPic( x + 4, y, 24, 24, armorIcon );
	value = cg.snap->ps.stats[STAT_ARMOR];
	colorIndex = value > 100 ? 2 : 0;
	trap_R_SetColor( healthColors67[colorIndex] );
	CG_DrawField( x + 4 + 24, y, 3, value, 16, 24 );
	trap_R_SetColor( NULL );
	y += 28;

	y = CG_OSPDrawSplitter( x, y );
	return y;
}

/*
CG_OSPDrawWeaponBarCPM1: the horizontal CPM weapon bar centred at the bottom (icon, ammo; the
selected weapon on a background, new weapons flash)
*/
void CG_OSPDrawWeaponBarCPM1( int y ) {
	int			weaponNum;
	int			size;
	int			x;
	vec4_t		color;
	int			ammo;
	float		w;
	qhandle_t	item;
	float		h;
	char		str[4];

	x = 320;
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weaponNum == 10 ) {
			continue;
		}
		if ( ( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) || ( ch_weaponListDrawAll.integer && cg_weapons[weaponNum].registered ) ) {
			x -= 24;
		}
	}
	x -= 20;

	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weaponNum == 10 ) {
			continue;
		}
		if ( ( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) || ( ch_weaponListDrawAll.integer && cg_weapons[weaponNum].registered ) ) {
			ammo = cg.predictedPlayerState.ammo[weaponNum];
			color[0] = 1.0f;
			color[1] = color[2] = ( ammo < 10 ) ? 0.3f : 1.0f;
			item = cg_weapons[weaponNum].ammoIcon;
			if ( cg.weaponSelect == weaponNum ) {
				size = 24;
				w = 16;
				h = 16;
				if ( ch_weaponListBackground.integer ) {
					CG_FillRect( x - 4, y, size + 5 + 3 * w + 4, 24, wepSwitchColor1 );
				}
				wepSwitch67[weaponNum] = 1;
			} else {
				size = 16;
				w = 8;
				h = 8;
				if ( wepSwitch67[weaponNum] == 0 && ( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
					wepSwitch67[weaponNum] = 900;
				}
				if ( wepSwitch67[weaponNum] > 1 ) {
					wepSwitchFlash = wepSwitch67[weaponNum] % 300;
					wepSwitchColor2[2] = wepSwitchFlash / 600.0f;
					wepSwitchColor2[3] = wepSwitchFlash / 300.0f;
					if ( ch_weaponListFlash.integer )
						CG_FillRect( x - 2, y - 2, size + 3 * w + 4, 20, wepSwitchColor2 );
					if ( ( wepSwitch67[weaponNum] -= osp_frameTime ) < 1 ) {
						wepSwitch67[weaponNum] = 1;
					}
				}
			}
			CG_DrawPic( x, y, size, size, item );
			if ( ammo == 0 || ( ch_weaponListDrawAll.integer && !( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) ) {
				CG_DrawPic( x, y, size, size, cgs.media.noammoShader );
			}
			if ( cg.weaponSelect == weaponNum ) {
				color[3] = 1.0f;
			} else {
				color[3] = 0.5f;
			}
			if ( ammo >= 0 ) {
				Com_sprintf( str, 4, "%i", ammo );
				CG_DrawStringExt( x + 4 + size, y + ( size / 2 - h / 2 ), str, color, qfalse, qtrue, w, h, 3 );
			}
			x += size + 3 * w + 8;
		}
		if ( !( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
			wepSwitch67[weaponNum] = 0;
		}
	}
}

/*
CG_OSPDrawWeaponBarCPM2: the CPM weapon bar in rows of five weapons from ( x, y )
*/
void CG_OSPDrawWeaponBarCPM2( int x, int y ) {
	int			weaponNum;
	int			size;
	vec4_t		color;
	int			ammo;
	int			line;
	float		w;
	qhandle_t	item;
	float		h;
	char		str[4];
	int			startX;

	startX = x;
	line = 0;
	for ( weaponNum = 1; weaponNum < MAX_WEAPONS; weaponNum++ ) {
		if ( weaponNum == 10 ) {
			continue;
		}
		if ( ( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) || ( ch_weaponListDrawAll.integer && cg_weapons[weaponNum].registered ) ) {
			if ( line == 5 ) {
				y += 28;
				x = startX;
				line = 0;
			}
			ammo = cg.predictedPlayerState.ammo[weaponNum];
			color[0] = 1.0f;
			color[1] = color[2] = ( ammo < 10 ) ? 0.3f : 1.0f;
			item = cg_weapons[weaponNum].ammoIcon;
			if ( cg.weaponSelect == weaponNum ) {
				size = 24;
				w = 16;
				h = 16;
				if ( ch_weaponListBackground.integer ) {
					CG_FillRect( x - 4, y, 81, 24, wepSwitchColor1 );
				}
				wepSwitch67[weaponNum] = 1;
			} else {
				size = 16;
				w = 8;
				h = 8;
				if ( wepSwitch67[weaponNum] == 0 && ( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
					wepSwitch67[weaponNum] = 900;
				}
				if ( wepSwitch67[weaponNum] > 1 ) {
					wepSwitchFlash = wepSwitch67[weaponNum] % 300;
					wepSwitchColor2[2] = wepSwitchFlash / 600.0f;
					wepSwitchColor2[3] = wepSwitchFlash / 300.0f;
					if ( ch_weaponListFlash.integer ) {
						CG_FillRect( x - 2, y, size + 3 * w + 4, 16, wepSwitchColor2 );
					}
					if ( ( wepSwitch67[weaponNum] -= osp_frameTime ) < 1 ) {
						wepSwitch67[weaponNum] = 1;
					}
				}
			}
			CG_DrawPic( x, y, size, size, item );
			if ( ammo == 0 || ( ch_weaponListDrawAll.integer && !( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) ) {
				CG_DrawPic( x, y, size, size, cgs.media.noammoShader );
			}
			if ( cg.weaponSelect == weaponNum ) {
				color[3] = 1.0f;
			} else {
				color[3] = 0.5f;
			}
			if ( ammo >= 0 ) {
				Com_sprintf( str, 4, "%i", ammo );
				CG_DrawStringExt( x + 4 + size, y + ( size / 2 - h / 2 ), str, color, qfalse, qtrue, w, h, 3 );
			}
			x += size + 3 * w + 8;
			line++;
		}
		if ( !( cg.predictedPlayerState.stats[STAT_WEAPONS] & ( 1 << weaponNum ) ) ) {
			wepSwitch67[weaponNum] = 0;
		}
	}
}

/*
CG_OSPDrawWeaponBar_6: the weapon list 6 on the left side (flags, weapons, health / armor,
powerups); returns 0 (the powerups and the holdable item are drawn here)
*/
int CG_OSPDrawWeaponBar_6( float y ) {
	float	x;

	x = 4;
	y = 48;
	y = CG_OSPDrawSplitter( x, y );
	y = CG_OSPDrawFlags67( x, y );
	y = CG_OSPDrawWeapons67( x, y );
	y = CG_OSPDrawHealthArmor67( x, y );
	y = CG_OSPDrawPowerup67( x, y );
	cpmDrawWeaponSelect = 1;
	return 0;
}

/*
CG_OSPDrawWeaponBar_7: the weapon list 7 on the right side from y (at least 48), then the
attacker; returns 0
*/
int CG_OSPDrawWeaponBar_7( float y ) {
	float	x;

	x = 559;
	y = ( y > 48 ) ? y : 48;
	y = CG_OSPDrawSplitter( x, y );
	y = CG_OSPDrawFlags67( x, y );
	y = CG_OSPDrawWeapons67( x, y );
	y = CG_OSPDrawHealthArmor67( x, y );
	y = CG_OSPDrawPowerup67( x, y );
	if ( cg_drawAttacker.integer ) {
		y = CG_OSPDrawWeaponSwitchAttacker( y );
	}
	cpmDrawWeaponSelect = 1;
	return 0;
}

/*
CG_OSPDrawCPMWeaponList: ch_weaponList: 0 id's weapon select, 1 the CPM weapon bar (CPM1 / CPM2
by ch_statusbarStyle; the list 6 with style 3), 2 the list 7, 3 the CPM bar; returns whether
the powerups and the holdable item still have to be drawn
*/
int CG_OSPDrawCPMWeaponList( float y ) {
	if ( !ch_weaponList.integer ) {
		cpmDrawWeaponSelect = 1;
		return 1;
	}
	if ( ch_weaponList.integer == 1 && ch_statusbarStyle.integer != 3 ) {
		return CG_OSPDrawWeaponBar_6( y );
	}
	if ( ch_weaponList.integer == 2 ) {
		return CG_OSPDrawWeaponBar_7( y );
	}
	if ( ch_weaponList.integer == 3 || ( ch_statusbarStyle.integer == 3 && ch_weaponList.integer == 1 ) ) {
		switch ( ch_statusbarStyle.integer ) {
		default:
		case 0:
		case 1:
			CG_OSPDrawWeaponBarCPM1( 380 );
			cpmDrawWeaponSelect = 0;
			break;
		case 2:
			CG_OSPDrawWeaponBarCPM2( 285, 425 );
			cpmDrawWeaponSelect = 0;
			break;
		case 3:
			CG_OSPDrawWeaponBarCPM2( 110, 425 );
			cpmDrawWeaponSelect = 0;
			break;
		}
		return 1;
	}
	return 1;
}

/*
CG_OSPDrawCPMTeamInfo: the team chat with ch_teamBackground (id's CG_DrawTeamInfo)
*/
void CG_OSPDrawCPMTeamInfo( void ) {
	int w, h;
	int i, len;
	vec4_t		hcolor;
	int		chatHeight;

#define CHATLOC_Y 420 // bottom end
#define CHATLOC_X 0

	if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT)
		chatHeight = cg_teamChatHeight.integer;
	else
		chatHeight = TEAMCHAT_HEIGHT;
	if (chatHeight <= 0)
		return; // disabled

	if (cgs.teamLastChatPos != cgs.teamChatPos) {
		if (cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer) {
			cgs.teamLastChatPos++;
		}

		if ( ch_teamBackground.integer ) {
			h = (cgs.teamChatPos - cgs.teamLastChatPos) * TINYCHAR_HEIGHT;

			w = 0;

			for (i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++) {
				len = CG_DrawStrlen(cgs.teamChatMsgs[i % chatHeight]);
				if (len > w)
					w = len;
			}
			w *= TINYCHAR_WIDTH;
			w += TINYCHAR_WIDTH * 2;

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
			CG_DrawPic( CHATLOC_X, CHATLOC_Y - h, 640, h, cgs.media.teamStatusBar );
			trap_R_SetColor( NULL );
		}

		hcolor[0] = hcolor[1] = hcolor[2] = 1.0f;
		hcolor[3] = 1.0f;

		for (i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i--) {
			CG_DrawStringExt( CHATLOC_X + TINYCHAR_WIDTH,
				CHATLOC_Y - (cgs.teamChatPos - i)*TINYCHAR_HEIGHT,
				cgs.teamChatMsgs[i % chatHeight], hcolor, qfalse, qfalse,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
	}
}

/*
CG_OSPDrawSpeed: ch_drawSpeed: the horizontal speed ("%fups") at the right edge; returns the y
below it
*/
float CG_OSPDrawSpeed( float y ) {
	char		*s;
	int			w;

	s = va( "%fups", cg.xyspeed );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	CG_DrawBigString( 639 - w, y + 2, s, 1.0F );

	return y + BIGCHAR_HEIGHT + 4;
}

/*
CG_OSPDrawCPMHoldableItem: the holdable item at the right edge (id's CG_DrawHoldableItem)
*/
void CG_OSPDrawCPMHoldableItem( void ) {
	int		value;

	value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
	if ( value > 0 && value < bg_numItems ) {
		CG_RegisterItemVisuals( value );
		CG_DrawPic( 640 - ICON_SIZE, ( SCREEN_HEIGHT - ICON_SIZE ) / 2, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
	}
}

/*
CG_OSPDrawCPMCrosshairNames: the name under the crosshair; for team mates also their health /
armor (ch_crosshairTeamInfo) with the model and armor icons
*/
void CG_OSPDrawCPMCrosshairNames( void ) {
	clientInfo_t	ci;
	float		x;
	float		*color;
	char		*name;
	char		str[128];
	qhandle_t	armorIcon;

	armorIcon = cgs.media.armorIcon;
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

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	// draw the name of the player being looked at
	color = CG_FadeColor( cg.crosshairClientTime, 1000 );
	if ( !color ) {
		trap_R_SetColor( NULL );
		return;
	}

	ci = cgs.clientinfo[ cg.crosshairClientNum ];
	name = ci.name;
	x = CG_DrawStrlen( name ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - x / 2, 270, name, color[3] * 0.5f );

	if ( cgs.gametype >= GT_TEAM && ci.team == cgs.clientinfo[cg.snap->ps.clientNum].team && ch_crosshairTeamInfo.integer && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		vec4_t	hcolor;

		Com_sprintf( str, sizeof( str ), "[  %3i/  %3i]", ci.health, ci.armor );
		x = 320 - CG_DrawStrlen( str ) * 8 / 2;
		CG_GetColorForHealth( ci.health, ci.armor, hcolor );
		hcolor[3] = color[3];
		CG_OSPDrawString( x, 286, str, 8, 16, hcolor, 0, 0 );
		x += 8;
		CG_DrawPic( x, 286, 16, 16, ci.modelIcon );
		x += 48;
		CG_DrawPic( x, 286, 16, 16, armorIcon );
	}
	trap_R_SetColor( NULL );
}

/*
CG_OSPDrawCPMHUD: the CPM HUD (PROMODE: CG_DrawActive / CG_DrawActiveFrame call it instead of
CG_Draw2D)
*/
void CG_OSPDrawCPMHUD( void ) {
	float	y;

	// if we are taking a levelshot for the menu, don't draw anything
	if ( cg.levelShot ) {
		return;
	}

	if ( cg_draw2D.integer == 0 ) {
		return;
	}

	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		CG_OSPDrawIntermission();
		CG_OSPDrawVote();
		return;
	}

	y = CG_OSPDrawCPMUpperRight();

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		CG_DrawSpectator();
		CG_OSPDrawCPMCrosshairNames();
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if ( cg.snap->ps.stats[STAT_HEALTH] > 0 ) {
			CG_OSPDrawCPMStatusbar();
			CG_DrawAmmoWarning();
			cpmDrawPowerups = CG_OSPDrawCPMWeaponList( y );
			if ( cpmDrawPowerups ) {
				CG_OSPDrawCPMHoldableItem();
			}
			CG_OSPDrawCrosshair();
			CG_OSPDrawCPMCrosshairNames();
			if ( cpmDrawWeaponSelect ) {
				CG_DrawWeaponSelect();
			}
			CG_DrawReward();
		}
		if ( cgs.gametype >= GT_TEAM ) {
			CG_OSPDrawCPMTeamInfo();
		}
	}

	CG_OSPDrawVote();

	if ( cg_lagometer.integer == 1 ) {
		CG_DrawLagometer( 428 - ( cg_drawPing.integer > 0 ? 14 : 0 ) );
	}

	CG_OSPDrawCPMLowerRight();
	CG_OSPDrawCPMLowerLeft();

	if ( !CG_DrawFollow() )
		CG_DrawWarmup();

	// don't draw center string if scoreboard is up
	if ( !CG_DrawScoreboard() ) {
		CG_OSPDrawCenterString();
	}
}

/*
CG_OSPSetHUDStyle: "hudstyle" (PROMODE): one of the 8 CPM HUD presets; out-of-range styles
wrap around. Each style sets ch_statusbarStyle, ch_graphs, ch_teamBackground, ch_weaponList,
cg_drawIcons and cg_drawTeamOverlay; returns the style set
*/
int CG_OSPSetHUDStyle( int style ) {
	char	*styles[8][6] = {
		{ "0", "0", "1", "0", "1", "1" },
		{ "1", "1", "1", "1", "1", "1" },
		{ "2", "1", "1", "3", "1", "1" },
		{ "3", "1", "1", "3", "1", "2" },
		{ "4", "0", "1", "1", "1", "3" },
		{ "5", "0", "0", "3", "1", "1" },
		{ "3", "1", "1", "0", "1", "2" },
		{ "5", "1", "1", "2", "1", "1" }
	};

	if ( style >= 8 ) {
		style = 0;
	} else if ( style < 0 ) {
		style = 7;
	}

	trap_Cvar_Set( "cg_draw2D", "1" );
	trap_Cvar_Set( "cg_drawStatus", "1" );
	trap_Cvar_Set( "ch_crosshairTeamInfo", "1" );
	trap_Cvar_Set( "cg_draw3dIcons", "1" );
	trap_Cvar_Set( "ch_statusbarStyle", styles[style][0] );
	trap_Cvar_Set( "ch_graphs", styles[style][1] );
	trap_Cvar_Set( "ch_teamBackground", styles[style][2] );
	trap_Cvar_Set( "ch_weaponList", styles[style][3] );
	trap_Cvar_Set( "cg_drawIcons", styles[style][4] );
	trap_Cvar_Set( "cg_drawTeamOverlay", styles[style][5] );

	return style;
}

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
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include "../client/client.h"
#include "sdl_glw.h"

#if defined(USE_Q3IDE) && defined(MACOS_X)
#include <CoreGraphics/CoreGraphics.h>
#endif

static Uint16 r[256];
static Uint16 g[256];
static Uint16 b[256];

void GLimp_InitGamma( glconfig_t *config )
{
	config->deviceSupportsGamma = qfalse;

	if ( SDL_GetWindowGammaRamp( SDL_window, r, g, b ) == 0 )
	{
		config->deviceSupportsGamma = SDL_SetWindowBrightness( SDL_window, 1.0f ) >= 0 ? qtrue : qfalse;
	}
}


/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	Uint16 table[3][256];
	int i, j;

	for ( i = 0; i < 256; i++ )
	{
		table[0][i] = ( ( ( Uint16 ) red[i] ) << 8 ) | red[i];
		table[1][i] = ( ( ( Uint16 ) green[i] ) << 8 ) | green[i];
		table[2][i] = ( ( ( Uint16 ) blue[i] ) << 8 ) | blue[i];
	}

#ifdef _WIN32
#include <windows.h>

	// Win2K and newer put this odd restriction on gamma ramps...
	{
		//OSVERSIONINFO	vinfo;
		//vinfo.dwOSVersionInfoSize = sizeof( vinfo );
		//GetVersionEx( &vinfo );
		//if( vinfo.dwMajorVersion >= 5 && vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
		{
			qboolean clamped = qfalse;
			for( j = 0 ; j < 3 ; j++ )
			{
				for( i = 0 ; i < 128 ; i++ )
				{
					if( table[ j ] [ i] > ( ( 128 + i ) << 8 ) )
					{
						table[ j ][ i ] = ( 128 + i ) << 8;
						clamped = qtrue;
					}
				}

				if( table[ j ] [127 ] > 254 << 8 )
				{
					table[ j ][ 127 ] = 254 << 8;
					clamped = qtrue;
				}
			}
			if ( clamped )
			{
				Com_DPrintf( "performing gamma clamp.\n" );
			}
		}
	}
#endif

	// enforce constantly increasing
	for ( j = 0; j < 3; j++ )
	{
		for (i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i-1])
				table[j][i] = table[j][i-1];
		}
	}

	if ( SDL_SetWindowGammaRamp( SDL_window, table[0], table[1], table[2] ) < 0 )
	{
		Com_DPrintf( "SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError() );
	}

#if defined(USE_Q3IDE) && defined(MACOS_X)
	/* SDL_SetWindowGammaRamp only touches the display the window is on.
	 * In multi-monitor mode, broadcast the same LUT to all active displays
	 * so side monitors match the center monitor's brightness. */
	if ( Cvar_VariableIntegerValue( "r_multiMonitor" ) )
	{
		CGGammaValue gr[256], gg[256], gb[256];
		CGDirectDisplayID displays[16];
		uint32_t count, di, j;

		for ( j = 0; j < 256; j++ )
		{
			gr[j] = table[0][j] / 65535.0f;
			gg[j] = table[1][j] / 65535.0f;
			gb[j] = table[2][j] / 65535.0f;
		}

		if ( CGGetActiveDisplayList( 16, displays, &count ) == kCGErrorSuccess )
		{
			for ( di = 0; di < count; di++ )
				CGSetDisplayTransferByTable( displays[di], 256, gr, gg, gb );
		}
	}
#endif
}


/*
** GLW_RestoreGamma
*/
void GLW_RestoreGamma( void )
{
#if defined(USE_Q3IDE) && defined(MACOS_X)
	/* Restore all displays to their macOS ICC color calibration on app exit.
	 * SDL restores the primary display's gamma automatically on window close,
	 * but we explicitly call this to ensure ICC profiles are re-enabled. */
	CGDisplayRestoreColorSyncSettings();
#endif
}

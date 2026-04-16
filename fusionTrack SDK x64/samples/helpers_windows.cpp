// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *   \file helpers_windows.cpp
 *   \brief Helping functions used by sample applications
 *
 */
// ============================================================================

#include "helpers.hpp"

#include <conio.h>
#include <iostream>
#include <stdio.h>
#include <windows.h>

using namespace std;

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

bool isLaunchedFromExplorer()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if ( !GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &csbi ) )
    {
        printf( "GetConsoleScreenBufferInfo failed: %lu\n", GetLastError() );
        return FALSE;
    }

    // if cursor position is (0,0) then we were launched in a separate console
    return ( ( !csbi.dwCursorPosition.X ) && ( !csbi.dwCursorPosition.Y ) );
}

#ifdef ATR_BORLAND
#define _kbhit kbhit
#endif

// Wait for a keyboard hit
void waitForKeyboardHit()
{
    while ( !_kbhit() )
    {
        Sleep( 100 );
    }
    _getch();
}

#ifdef ATR_BORLAND
#undef_kbhit
#endif

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

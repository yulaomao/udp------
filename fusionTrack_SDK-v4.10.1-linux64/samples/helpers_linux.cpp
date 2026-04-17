// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *   \file helpers_linux.cpp
 *   \brief Helping functions used by sample applications
 *
 */
// =============================================================================

#include "helpers.hpp"

#include <ftkInterface.h>
#include <ftkTypes.h>

#include <pthread.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <termios.h>
#include <unistd.h>

bool isLaunchedFromExplorer()
{
    return false;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

int detectKeyboardHit()
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    oldf = fcntl( STDIN_FILENO, F_GETFL, 0 );
    fcntl( STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK );

    ch = getchar();

    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    fcntl( STDIN_FILENO, F_SETFL, oldf );

    if ( ch != EOF )
    {
        ungetc( ch, stdin );
        return 1;
    }

    return 0;
}

void waitForKeyboardHit()
{
    while ( detectKeyboardHit() == 0 )
    {
        usleep( 100000 );
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

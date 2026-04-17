// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2021 by Atracsys LLC. All rights reserved.
 *
 *   \file stereo1_ListOptions.cpp
 *   \brief Display fusionTrack/spryTrack  options in the console
 *
 *   This sample aims to present the following driver features:
 *   - Open/close the driver
 *   - Enumerate devices
 *   - List options
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack/spryTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *
 */
// =============================================================================

#include "helpers.hpp"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

ftkLibrary lib = NULL;
bool isNotFromConsole = true;

// ---------------------------------------------------------------------------
// Callback function for options

void optionEnumeratorDiplay( uint64 sn, void* user, ftkOptionsInfo* oi )
{
    cout << "Option " << oi->id << "  " << oi->name << endl;

    switch ( oi->component )
    {
    case ftkComponent::FTK_LIBRARY:
        cout << "\tCOMP:  library" << endl;
        break;

    case ftkComponent::FTK_DEVICE:
        cout << "\tCOMP:  device" << endl;
        break;

    case ftkComponent::FTK_DETECTOR:
        cout << "\tCOMP:  detector" << endl;
        break;

    case ftkComponent::FTK_MATCH2D3D:
        cout << "\tCOMP:  matching" << endl;
        break;

    case ftkComponent::FTK_DEVICE_WIRELESS:
        cout << "\tCOMP:  wireless management" << endl;
        break;

    case ftkComponent::FTK_DEVICE_PTP:
        cout << "\tCOMP:  PTP handling" << endl;
        break;

    case ftkComponent::FTK_DEVICE_EIO:
        cout << "\tCOMP:  EIO port" << endl;
        break;

    default:
        cout << "\tCOMP:  ????" << endl;
        break;
    }

    cout << "\tDESC:  " << oi->description << endl;
    if ( oi->unit )
    {
        cout << "\tUNIT:  " << oi->unit << endl;
    }

    cout << "\tSTAT:  ";
    if ( oi->status.read )
    {
        cout << "(READ)";
    }
    if ( oi->status.write )
    {
        cout << "(WRITE)";
    }
    cout << endl;

    switch ( oi->type )
    {
    case ftkOptionType::FTK_INT32:
        cout << "\tTYPE:  int32" << endl;
        if ( strcmp( oi->name, "Challenge result" ) == 0 )
        {
            break;
        }
        if ( oi->status.read && oi->status.write )
        {
            int32 out;
            if ( ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MIN_VAL ) != ftkError::FTK_OK )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMIN:   " << out << endl;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MAX_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMAX:   " << out << endl;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_DEF_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tDEF:   " << out << endl;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_VALUE ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tVAL:   " << out << endl;
        }
        else if ( oi->status.read )
        {
            int32 out;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_VALUE ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tVAL:   " << out << endl;
        }
        else if ( oi->status.write )
        {
            int32 out;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MIN_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMIN:   " << out << endl;
            if ( ftkError::FTK_OK != ftkGetInt32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MAX_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMAX:   " << out << endl;
        }
        break;

    case ftkOptionType::FTK_FLOAT32:
        cout << "\tTYPE:  float32" << endl;
        if ( oi->status.read && oi->status.write )
        {
            float32 out;
            ftkError err( ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MIN_VAL ) );
            if ( err == ftkError::FTK_OK )
            {
                cout << "\tMIN:   " << out << endl;
            }
            err = ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MAX_VAL );
            if ( err == ftkError::FTK_OK )
            {
                cout << "\tMAX:   " << out << endl;
            }
            err = ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_DEF_VAL );
            if ( err == ftkError::FTK_OK )
            {
                cout << "\tDEF:   " << out << endl;
            }
            if ( ftkError::FTK_OK != ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_VALUE ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tVAL:   " << out << endl;
        }
        else if ( oi->status.read )
        {
            float32 out;
            if ( ftkError::FTK_OK != ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_VALUE ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tVAL:   " << out << endl;
        }
        else if ( oi->status.write )
        {
            float32 out;
            if ( ftkError::FTK_OK != ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MIN_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMIN:   " << out << endl;
            if ( ftkError::FTK_OK != ftkGetFloat32( lib, sn, oi->id, &out, ftkOptionGetter::FTK_MAX_VAL ) )
            {
                checkError( lib, !isNotFromConsole, false );
            }
            cout << "\tMAX:   " << out << endl;
        }
        break;

    case ftkOptionType::FTK_DATA:
        cout << "\tTYPE:  data" << endl;
        if ( oi->status.read )
        {
            ftkBuffer buffer;
            if ( ftkError::FTK_OK != ftkGetData( lib, sn, oi->id, &buffer ) )
            {
                cerr << "\tVAL: could not read value:" << endl;
                checkError( lib, !isNotFromConsole, false );
            }
            else
            {
                cout << "\tVAL:   " << buffer.sData << endl;
            }
        }
        break;

    default:
        cout << "\tTYPE:  ????" << endl;
        break;
    }

    cout << "" << endl;
}

// ---------------------------------------------------------------------------
// Enumerate all the available options

inline void enumerateOptions( ftkLibrary lib, uint64 sn )
{
    ftkError status( ftkEnumerateOptions( lib, sn, optionEnumeratorDiplay, nullptr ) );
    if ( ftkError::FTK_OK != status && ftkError::FTK_WAR_OPT_GLOBAL_ONLY != status )
    {
        checkError( lib, !isNotFromConsole );
    }
}

// ---------------------------------------------------------------------------
// main function

int main( int argc, char** argv )
{
    isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << "This is a demonstration on how to enumerate options." << endl;

    deque< string > args;
    for ( int i( 1 ); i < argc; ++i )
    {
        args.emplace_back( argv[ i ] );
    }

    bool showHelp( false );

    if ( !args.empty() )
    {
        showHelp = ( find_if( args.cbegin(), args.cend(), []( const string& val ) {
                         return val == "-h" || val == "--help";
                     } ) != args.cend() );
    }

    string cfgFile( "" );

    if ( showHelp || args.empty() )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-c/--config F] " << flush << "JSON config file. Type "
             << "std::string, default = none" << endl;
    }

    cout << "Copyright (c) Atracsys LLC 2003-2021" << endl;
    if ( showHelp )
    {
#ifdef ATR_WIN
        if ( isLaunchedFromExplorer() )
        {
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
        }
#endif
        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-c" || val == "--config"; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        cfgFile = *pos;
    }

    // ----------------------------------------------------------------------
    // Initialize driver

    ftkBuffer buffer;

    lib = ftkInitExt( cfgFile.empty() ? nullptr : cfgFile.c_str(), &buffer );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialize driver", !isNotFromConsole );
    }

    // ----------------------------------------------------------------------
    // List global options (i.e. options which don't require a device)

    cout << "List global options:" << endl << endl;
    enumerateOptions( lib, 0uLL );

    // ----------------------------------------------------------------------
    // Retrieve the device

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    // Set current temperature index
    ftkSetInt32( lib, sn, 30, 1 );

    cout << "Waiting for 2 seconds" << endl;
    sleep( 2500L );
    ftkFrameQuery* frame( ftkCreateFrame() );
    ftkGetLastFrame( lib, sn, frame, 1000u );
    ftkDeleteFrame( frame );

    // Copying the counter of lost frames
    ftkSetInt32( lib, sn, 68, 1 );

    // ----------------------------------------------------------------------
    // Enumerate options of the device

    cout << "List available options:" << endl << endl;
    enumerateOptions( lib, sn );

    cout << "\tSUCCESS" << endl;

    // ----------------------------------------------------------------------
    // Close driver

    if ( ftkError::FTK_OK != ftkClose( &lib ) )
    {
        checkError( lib, !isNotFromConsole );
    }

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif
    return EXIT_SUCCESS;
}

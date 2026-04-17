// =============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2022 by Atracsys LLC. All rights reserved.
 *
 *   \fTk36_TaggerSingleMode.cpp
 *   \brief Demonstrate how to use Tagging in both single and dual mode with the fusionTrack.
 *
 *   How to compile this example:
 *   - Follow instructions in README.txt
 *
 *   How to run this example:
 *   - Install the fusionTrack driver (see documentation)
 *   - Switch on device
 *   - Run the resulting executable
 *
 */
// =============================================================================
#include "helpers.hpp"

#include <ftkInterface.h>
#include <ftkPlatform.h>

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

#define TAG_TOL_LIM 1  //1us

struct MatchingInfo
{
    uint64
      LocaltagTimestamp;  //Local fTk timestamps corresponding to the tag of an external event (trigger) is received
    uint64
      CorrespondindExtInfo;  // External device information sampled at the same time given by the tag timestamp above
};

bool setOption( ftkLibrary lib, uint64 sn, uint32 optId, uint32 optValue );

#define ACN_ENABLE 1
#define ACN_DISABLE 0
#define TRIG_DISABLE 0
#define TRIG_DUR_10US 10
#define EIO_PORTS_DISABLE 7
#define EIO_PORTS_TRIG_MODE 3
#define EIO_PORTS_SINGLE_TAG_MODE 5
#define EIO_PORTS_DUAL_TAG_MODE 6

int main( int argc, char* argv[] )
{
    const bool isNotFromConsole = isLaunchedFromExplorer();

    // -----------------------------------------------------------------------
    // Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
    // set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    cout << endl;  // For clean output start on the console.

    // ----------------------------------------------------------------------
    // Input arguments handling

    deque< string > args;
    for ( int i( 1 ); i < argc; ++i )
    {
        args.emplace_back( argv[ i ] );
    }

    bool showHelp( false );

    if ( !args.empty() )
    {
        showHelp =
          ( find_if( args.cbegin(),
                     args.cend(),
                     []( const string& val ) { return val == "-h" || val == "--help"; } ) != args.cend() );
    }

    uint32 nbTagsToDetect( 100 );

    string portTagger( "EIO_1" );
    string portTrigger( "EIO_2" );
    string taggerPortModeOption( "aCn EIO 1 mode" );
    string triggerPortModeOption( "aCn EIO 2 mode" );

    bool arg_single_check( false );
    bool arg_dual_check( false );

    uint32 port_mode( EIO_PORTS_SINGLE_TAG_MODE );

    if ( showHelp )
    {
        cout << setw( 30u ) << "[-h/--help] " << flush << "Displays this help and exits." << endl;
        cout << setw( 30u ) << "[-t/--nbTags] " << flush
             << "Number of tags to be detected. Type = uint32, default = 100" << endl;
        cout << setw( 30u ) << "[-p/--portTagger] " << flush
             << "Port used as tagger (other one sends triggers). Type = std::string, default = EIO_1" << endl;
        cout << setw( 30u ) << "[-s/--single] " << flush
             << "Enable the single mode. Type = No argument, default = none" << endl;
        cout << setw( 30u ) << "[-d/--dual] " << flush
             << "Enable the dual mode. Type = No argument, default = none" << endl
             << endl;

        cout << "The default mode is the single tagging mode." << endl << endl;

#ifdef ATR_WIN
        cout << "Press the \"ANY\" key to quit" << endl;
        waitForKeyboardHit();
#endif

        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-s" || val == "--single"; } ) );
    if ( pos != args.cend() )
    {
        arg_single_check = true;
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-d" || val == "--dual"; } );
    if ( pos != args.cend() )
    {
        arg_dual_check = true;
        port_mode = EIO_PORTS_DUAL_TAG_MODE;
    }
    else
    {
        arg_single_check = true;  // Set the arg_single_check to true in case no argument is called
    }

    if ( arg_single_check && arg_dual_check )
    {
        cout << "Arguments error: both [-s/--single] and [-d/--dual] arguments detected, " << endl
             << "only one mode can be selected." << endl
             << endl;

#ifdef ATR_WIN
        cout << "Press the \"ANY\" key to quit" << endl;
        waitForKeyboardHit();
#endif

        return 1;
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-t" || val == "--nbTags"; } );
    if ( pos != args.cend() )
    {
        if ( ++pos != args.cend() )
        {
            stringstream s( *pos );

            for ( auto it( ( *pos ).begin() ); it != ( *pos ).end(); it++ )
            {
                if ( !isdigit( *it ) )
                {
                    cout << "Arguments error: Please enter a positive number only for the number of tags to "
                            "detect."
                         << endl
                         << endl;

#ifdef ATR_WIN
                    cout << "Press the \"ANY\" key to quit" << endl;
                    waitForKeyboardHit();
#endif

                    return 1;
                }
            }

            s >> nbTagsToDetect;

            if ( nbTagsToDetect == 0 )
            {
                cout << "Arguments error: Please enter a number bigger than \"0\" for the number of tags to "
                        "detect."
                     << endl
                     << endl;

#ifdef ATR_WIN
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
#endif

                return 1;
            }
        }
        else
        {
            cout << "Arguments error: No argument given for the number of tags to detect." << endl << endl;

#ifdef ATR_WIN
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
#endif

            return 1;
        }
    }

    pos = find_if(
      args.cbegin(), args.cend(), []( const string& val ) { return val == "-p" || val == "--portTagger"; } );
    if ( pos != args.cend() )
    {
        if ( ++pos != args.cend() )
        {
            if ( *pos == "EIO_2" )
            {
                portTagger = "EIO_2";
                portTrigger = "EIO_1";
                taggerPortModeOption = "aCn EIO 2 mode";
                triggerPortModeOption = "aCn EIO 1 mode";
            }
            else if ( *pos != "EIO_1" )
            {
                cout
                  << "Arguments error: Only the following arguments are expected for the tagger port choice:"
                  << endl
                  << endl
                  << "\t\"EIO_1\": Choose the EIO_1 port as tagger and the EIO 2 port to send triggers."
                  << endl
                  << "\t\"EIO_2\": Choose the EIO_2 port as tagger and the EIO 1 port to send triggers."
                  << endl
                  << endl;

#ifdef ATR_WIN
                cout << "Press the \"ANY\" key to quit" << endl;
                waitForKeyboardHit();
#endif

                return 1;
            }
        }
        else
        {
            cout << "Arguments error: No argument given for the tagger port choice." << endl << endl;

#ifdef ATR_WIN
            cout << "Press the \"ANY\" key to quit" << endl;
            waitForKeyboardHit();
#endif

            return 1;
        }
    }

    // ----------------------------------------------------------------------
    // Initialize driver

    if ( arg_single_check )
    {
        cout << "This is a demonstration on how to use the Tagger module in single mode." << endl << endl;
    }
    else if ( arg_dual_check )
    {
        cout << "This is a demonstration on how to use the Tagger module in dual mode." << endl << endl;
    }

    ftkBuffer buffer{};

    ftkLibrary lib( ftkInitExt( nullptr, &buffer ) );
    if ( lib == nullptr )
    {
        cerr << buffer.data << endl;
        error( "Cannot initialise library", !isNotFromConsole );
    }
    else
    {
        cout << "Library initialised" << endl;
    }

    DeviceData device( retrieveLastDevice( lib, true, false, !isNotFromConsole ) );
    uint64 sn( device.SerialNumber );

    map< string, uint32 > optionsMap;
    deque< string > taggerTestOptions(
      { "aCn module enable", "aCn trigger duration", "aCn EIO 1 mode", "aCn EIO 2 mode" } );

    ftkError err( ftkEnumerateOptions( lib, sn, optionEnumerator, &optionsMap ) );

    if ( err != ftkError::FTK_OK )
    {
        checkError( lib, !isNotFromConsole );
    }

    for ( auto it( taggerTestOptions.begin() ); it != taggerTestOptions.end(); it++ )
    {
        if ( optionsMap.find( *it ) == optionsMap.end() )
        {
            cerr << "Unable to retrieve the following option: " << *it << endl;
#ifdef ATR_WIN
            waitForKeyboardHit();
#endif
            return 1;
        }
    }

    ftkFrameQuery* frame( ftkCreateFrame() );
    err = ftkSetFrameOptions( false, 10u, 0u, 0u, 0u, 0u, frame );

    // ----------------------------------------------------------------------
    // Options configuration
    buffer.reset();
    ErrorReader errReader;

    // TODO: Use this to set options. The names of the options are available by running the sample 1: ListOptions.
    //Enable the aCn hardware module.
    if ( !setOption( lib, sn, optionsMap[ "aCn module enable" ], ACN_ENABLE ) )
    {
        return 1;
    }

    //Set the tagger and trigger port in disable mode such that any
    //connection or deconnection between devices can be applied safely.
    if ( !setOption( lib, sn, optionsMap[ taggerPortModeOption ], EIO_PORTS_DISABLE ) )
    {
        return 1;
    }

    if ( !setOption( lib, sn, optionsMap[ triggerPortModeOption ], EIO_PORTS_DISABLE ) )
    {
        return 1;
    }

    cout << endl << endl;
    cout << "................aCn CONNEXION SETUP : " << portTagger << " as tagger, " << portTrigger
         << " as trigger................" << endl;
    cout << "Please, remove any previous connections made on the aCn and press any key to continue" << endl
         << "when this is done." << endl
         << endl;
    waitForKeyboardHit();
    cout << "Please, connect a jumper between the \"vcc3.3\" pin and the \"TE\" pin for the " << portTagger
         << endl
         << "aCn port (see the four pins above " << portTagger << " pins) and press any key to continue when"
         << endl
         << "this is done." << endl
         << endl;
    waitForKeyboardHit();
    cout << "Please, remove any jumper for the " << portTrigger << " aCn port (see the four pins above "
         << portTrigger << " pins)" << endl
         << "and press any key to continue when this is done." << endl
         << endl;
    waitForKeyboardHit();
    cout << "Please connect the aCn " << portTrigger << " \"R\" pin to the " << portTagger
         << " \"T\" pin and press any key to run" << endl
         << "the tagging operation for this setup." << endl;
    waitForKeyboardHit();
    cout << "......................................................................................." << endl
         << endl
         << endl;

    // Using the trigger port for generating the trigger signal.
    if ( !setOption( lib, sn, optionsMap[ triggerPortModeOption ], EIO_PORTS_TRIG_MODE ) )
    {
        return 1;
    }

    // Set the trigger duration to 0us to disable it and have a clean
    // ground on the line (fresh start for the tagging operation).
    if ( !setOption( lib, sn, optionsMap[ "aCn trigger duration" ], TRIG_DISABLE ) )
    {
        return 1;
    }

    // Set the tagger port in single or dual mode depending on the input arguments.
    if ( !setOption( lib, sn, optionsMap[ taggerPortModeOption ], port_mode ) )
    {
        return 1;
    }

    // Set the trigger duration to 10us. Right after this setting is effective, the triggers pulse
    // are transmitted from the trigger port as this one is set to the corresponding trigger mode.
    if ( !setOption( lib, sn, optionsMap[ "aCn trigger duration" ], TRIG_DUR_10US ) )
    {
        return 1;
    }

    // Note:
    // The id of the tags increase for each detected trigger once the mode is enabled.
    // The id of the triggers increase as soon as these ones are sent in that example.
    // To match the ids together, always enable the tagger before sending triggers.

    // ----------------------------------------------------------------------
    // Frame acquisition, EIO tags events and EIO trigger events analysis

    const ftkEvent* evt( nullptr );
    const EvtEioTaggingV1Payload* eioTaggingData( nullptr );
    const EvtEioTriggerInfoV1Payload* eioTriggerData( nullptr );
    uint32_t counter( 0u );

    map< uint32_t, uint64 > trigTS;
    //Note:
    // Here, we are interested in recovering the trigger sending timestamp
    // to oberve if this one match witch the tagged timestamps. This can be
    // done because the same device is used for the trigger and the tagging
    // process. Usually the trigger sender is an external device and the
    // trigger and tagger timstamps are different as coming from different
    // devices. Only the triggers and tagging IDs should match to allow the
    // informations coming from different devices to be matched later on.

    map< uint32_t, uint64 > tagTimestamps;
    deque< MatchingInfo > matchingInfoAcq;
    MatchingInfo matchingInfoTamp;

    for ( unsigned int n( 0u ); n < 1000u; ++n )
    {
        err = ftkGetLastFrame( lib, sn, frame, 100u );
        if ( err > ftkError::FTK_OK )
        /* block until next frame is available */
        {
            cout << ".";
            continue;
        }
        else if ( err == ftkError::FTK_WAR_NO_FRAME )
        {
            continue;
        }

        for ( uint32_t iEvt( 0u ); iEvt < frame->eventsCount; ++iEvt )
        {
            evt = frame->events[ iEvt ];
            if ( evt->Type == FtkEventType::fetEioTaggingV1 )
            {
                eioTaggingData = reinterpret_cast< EvtEioTaggingV1Payload* >( evt->Data );

                // Note:
                // No matter the mode (single or dual) all the usefull information is kept
                // and duplicated information is discarded thank to the insert function.

                //The tags start with "ID = 1", "ID = 0" indicated no tags are received yet.
                if ( eioTaggingData->TaggingInfo[ 0 ].EioTagId != 0 )
                {
                    // Store the first part of the tagging info
                    if ( tagTimestamps
                           .insert( make_pair( eioTaggingData->TaggingInfo[ 0 ].EioTagId,
                                               eioTaggingData->TaggingInfo[ 0 ].EioTagTimestamp ) )
                           .second )
                    {
                        ++counter;  // Increase the counter only when a new tag is recorded.
                        n = 0;      // The guard (timeout) counter can be reset.
                    }
                }

                // The tags start with "ID = 1", "ID = 0" indicated no tags are received yet.
                if ( eioTaggingData->TaggingInfo[ 1 ].EioTagId != 0 )
                {
                    // Store the second part of the tagging info
                    if ( tagTimestamps
                           .insert( make_pair( eioTaggingData->TaggingInfo[ 1 ].EioTagId,
                                               eioTaggingData->TaggingInfo[ 1 ].EioTagTimestamp ) )
                           .second )
                    {
                        ++counter;  // Increase the counter only when a new tag is recorded.
                        n = 0;      // The guard (timeout) counter can be reset.
                    }
                }
            }
            else if ( evt->Type == FtkEventType::fetTriggerInfoV1 )  // Future event to be defined
            {
                eioTriggerData = reinterpret_cast< EvtEioTriggerInfoV1Payload* >( evt->Data );

                // Note:
                // Again, no matter the mode (single or dual) only the usefull information
                // is kept.

                //No need to filter the triggers as the tags are already filtered.
                trigTS.insert( make_pair( eioTriggerData->TriggerIdEio1,
                                          ( frame->imageHeader->timestampUS ) +
                                            ( static_cast< uint64 >( eioTriggerData->TriggerStartTime ) ) ) );

                // No need to filter the triggers as the tags are already filtered.
                trigTS.insert( make_pair( eioTriggerData->TriggerIdEio2,
                                          ( frame->imageHeader->timestampUS ) +
                                            ( static_cast< uint64 >( eioTriggerData->TriggerStartTime ) ) ) );
            }
        }

        if ( counter == nbTagsToDetect )
        {
            break;
        }
    }

    bool allTimestampsMatch( true );
    bool atLeastOneTag( false );

    uint64 timestampsDiff( 0 );

    // We finally match the external information sampled at the trigger
    // and the corresponding fTk timestamp tagged at this same trigger
    cout << "Retrieved Tag timestamps and corresponding Trigger timestamp..........................." << endl
         << endl;

    for ( auto itTagMap( tagTimestamps.cbegin() ); itTagMap != tagTimestamps.cend(); ++itTagMap )
    {
        atLeastOneTag = true;
        cout << "   Tag ID " << itTagMap->first << " detected" << endl;
        cout << "   ..........................................................." << endl;
        if ( trigTS.find( itTagMap->first ) != trigTS.cend() )
        {
            matchingInfoTamp.LocaltagTimestamp = itTagMap->second;
            matchingInfoTamp.CorrespondindExtInfo = trigTS[ itTagMap->first ];
            matchingInfoAcq.push_back( matchingInfoTamp );

            cout << "    Tag matching Trigger with ID: " << itTagMap->first << endl
                 << endl
                 << "       fTk local Tag timestamp : " << matchingInfoTamp.LocaltagTimestamp << endl
                 << "       Corresponding Trigger timestamp : " << matchingInfoTamp.CorrespondindExtInfo
                 << endl
                 << endl;

            // Comparing the timestamps
            timestampsDiff = matchingInfoTamp.LocaltagTimestamp - matchingInfoTamp.CorrespondindExtInfo;

            // The local tagged timestamp should be identical to the corresponding trigger
            // with a tolerance of 1us as the same device send the trigger and tag it.
            if ( timestampsDiff <= TAG_TOL_LIM )
            {
                cout << "    -----> Timestamps MATCHED with a tolerance of " << TAG_TOL_LIM << "us," << endl
                     << "           tagging process works properly!" << endl;
            }
            else
            {
                cout << "    -----> Timestamps NOT MATCHED, ERROR in the tagging process!" << endl;
                allTimestampsMatch = false;
            }
        }
        else
        {
            cout << "    No Trigger associated, the information may have been lost " << endl
                 << "    or should be received in future frames to be process." << endl;
        }

        cout << "   ..........................................................." << endl << endl << endl;
    }
    cout << "......................................................................................." << endl
         << endl;

    if ( atLeastOneTag )
    {
        if ( allTimestampsMatch )
        {
            cout << "Tagging process SUCCESSFULLY executed, all the timestamps are matching." << endl << endl;
        }
        else
        {
            cout << "ERROR in the tagging process, not all the timestamps are matching." << endl << endl;
        }
    }
    else
    {
        cout << "ERROR in the tagging process, no tags are detected (check your aCn connections)." << endl
             << endl;
    }

    //Note:
    // A final step would consist of using the tagged timestamps (stored in matchingInfoAcq)
    // to interpolate some usefull information of the frames based of the frame timestamps.
    // The resulting interpolated information will match the "CorrespondindExtInfo" stored in
    // the same matchingInfoAcq deque location than the used tagged timestamp to
    // interpolate. This step is ouside the scope of this sample, see sample
    // stereo32_InterpolateFrames.cpp for more information.

    // Set the trigger duration to 0us to disable it.
    if ( !setOption( lib, sn, optionsMap[ "aCn trigger duration" ], TRIG_DISABLE ) )
    {
        return 1;
    }

    // Set the tagger and trigger port in disable mode such that any
    //connection or deconnection between devices can be applied safely.
    if ( !setOption( lib, sn, optionsMap[ taggerPortModeOption ], EIO_PORTS_DISABLE ) )
    {
        return 1;
    }

    if ( !setOption( lib, sn, optionsMap[ triggerPortModeOption ], EIO_PORTS_DISABLE ) )
    {
        return 1;
    }

    // Disable the aCn hardware module.
    if ( !setOption( lib, sn, optionsMap[ "aCn module enable" ], ACN_DISABLE ) )
    {
        return 1;
    }

    cout << endl;

    // Before ending the sample
    cout << "Please, remove the connections made on the aCn for this sample example and press" << endl
         << "any key to continue when this is done." << endl
         << endl;
    waitForKeyboardHit();

    ftkDeleteFrame( frame );
    ftkClose( &lib );

#ifdef ATR_WIN
    if ( isNotFromConsole )
    {
        cout << endl << "*** Hit a key to exit ***" << endl;
        waitForKeyboardHit();
    }
#endif

    return 0;
}

bool setOption( ftkLibrary lib, uint64 sn, uint32 optId, uint32 optValue )
{
    ftkBuffer buffer;
    ErrorReader errReader;

    ftkError err = ftkSetInt32( lib, sn, optId, optValue );

    if ( err != ftkError::FTK_OK )
    {
        ftkGetLastErrorString( lib, sizeof( buffer.data ), buffer.data );
        if ( !errReader.parseErrorString( buffer.data ) )
        {
            cerr << "Cannot interpret error string:" << endl << buffer.data << endl;
            return false;
        }
        else
        {
            cerr << buffer.data << endl;
        }
        return false;
    }

    return true;
}

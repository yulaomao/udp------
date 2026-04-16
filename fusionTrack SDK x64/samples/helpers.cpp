#include "helpers.hpp"

#include <ftkPlatform.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

using namespace std;

void sleep( long ms )
{
    this_thread::sleep_for( chrono::milliseconds( ms ) );
}

void error( const char* message, bool dontWaitForKeyboard )
{
    cerr << "ERROR: " << message << endl;

#ifdef ATR_WIN
    if ( !dontWaitForKeyboard )
    {
        cout << "Press the \"ANY\" key to quit" << endl;
        waitForKeyboardHit();
    }
#endif

    exit( 1 );
}

void deviceEnumerator( uint64 sn, void* user, ftkDeviceType type )
{
    if ( user != 0 )
    {
        DeviceData* ptr = reinterpret_cast< DeviceData* >( user );
        ptr->SerialNumber = sn;
        ptr->Type = type;
    }
}

void fusionTrackEnumerator( uint64 sn, void* user, ftkDeviceType devType )
{
    if ( user != 0 )
    {
        if ( devType != ftkDeviceType::DEV_SIMULATOR )
        {
            DeviceData* ptr = reinterpret_cast< DeviceData* >( user );
            ptr->SerialNumber = sn;
            ptr->Type = devType;
        }
        else
        {
            cerr << "ERROR: This sample cannot be used with the simulator" << endl;
            DeviceData* ptr = reinterpret_cast< DeviceData* >( user );
            ptr->SerialNumber = 0u;
            ptr->Type = ftkDeviceType::DEV_UNKNOWN_DEVICE;
        }
    }
}

void checkError( ftkLibrary lib, bool dontWaitForKeyboard, bool quit )
{
    char message[ 1024u ];
    ftkError err( ftkGetLastErrorString( lib, 1024u, message ) );
    if ( err == ftkError::FTK_OK )
    {
        ErrorReader errReader{};
        if ( !errReader.parseErrorString( message ) )
        {
            cerr << "Malformed error string" << endl;
        }
        else
        {
            errReader.display( cerr );
        }
    }
    else
    {
        cerr << "Uninitialised library handle provided" << endl;
    }

    if ( quit )
    {
#ifdef ATR_WIN
        if ( !dontWaitForKeyboard )
        {
            cout << "Press the \"ANY\" key to exit" << endl;
            waitForKeyboardHit();
        }
#endif
        ftkClose( &lib );
        exit( 1 );
    }
}

DeviceData retrieveLastDevice( ftkLibrary lib, bool allowSimulator, bool quiet, bool dontWaitForKeyboard )
{
    DeviceData device;
    device.SerialNumber = 0uLL;
    // Scan for devices
    ftkError err( ftkError::FTK_OK );
    if ( allowSimulator )
    {
        err = ftkEnumerateDevices( lib, deviceEnumerator, &device );
    }
    else
    {
        err = ftkEnumerateDevices( lib, fusionTrackEnumerator, &device );
    }

    if ( err > ftkError::FTK_OK )
    {
        checkError( lib, dontWaitForKeyboard );
    }
    else if ( err < ftkError::FTK_OK )
    {
        if ( !quiet )
        {
            checkError( lib, dontWaitForKeyboard, false );
        }
    }

    if ( device.SerialNumber == 0uLL )
    {
        error( "No device connected", dontWaitForKeyboard );
    }
    string text;
    switch ( device.Type )
    {
    case ftkDeviceType::DEV_SPRYTRACK_180:
        text = "sTk 180";
        break;
    case ftkDeviceType::DEV_SPRYTRACK_300:
        text = "sTk 300";
        break;
    case ftkDeviceType::DEV_FUSIONTRACK_500:
        text = "fTk 500";
        break;
    case ftkDeviceType::DEV_FUSIONTRACK_250:
        text = "fTk 250";
        break;
    case ftkDeviceType::DEV_SIMULATOR:
        text = "fTk simulator";
        break;
    default:
        text = " UNKNOWN";
        error( "Unknown type", dontWaitForKeyboard );
    }
    if ( !quiet )
    {
        cout << "Detected one " << text;

        cout << " with serial number 0x" << setw( 16u ) << setfill( '0' ) << hex << device.SerialNumber << dec
             << endl
             << setfill( '\0' );
    }

    return device;
}

void optionEnumerator( uint64 sn, void* user, ftkOptionsInfo* oi )
{
    if ( user == nullptr )
    {
        return;
    }

    map< string, uint32 >* mapping( static_cast< map< string, uint32 >* >( user ) );

    if ( mapping == nullptr )
    {
        return;
    }

    mapping->emplace( oi->name, oi->id );
}

ErrorReader::ErrorReader()
    : _ErrorString( "" )
    , _WarningString( "" )
    , _StackMessage( "" )
{
}

ErrorReader::~ErrorReader()
{
}

bool ErrorReader::parseErrorString( const string& str )
{
    if ( str.find( "<ftkError>" ) == string::npos || str.find( "</ftkError>" ) == string::npos )
    {
        cerr << "Cannot find root element <ftkError>" << endl;
        return false;
    }

    size_t locBegin, locEnd;

    string* pseudoRef( 0 );

    for ( const auto& tagIt : vector< string >{ "errors", "warnings", "messages" } )
    {
        if ( tagIt == "errors" )
        {
            pseudoRef = &_ErrorString;
        }
        else if ( tagIt == "warnings" )
        {
            pseudoRef = &_WarningString;
        }
        else if ( tagIt == "messages" )
        {
            pseudoRef = &_StackMessage;
        }
        if ( str.find( "<" + tagIt + " />" ) == string::npos )
        {
            locBegin = str.find( "<" + tagIt + ">" );
            locEnd = str.find( "</" + tagIt + ">" );
            if ( locBegin != string::npos && locEnd != string::npos )
            {
                locBegin += string( "<" + tagIt + ">" ).size();
                if ( locBegin > locEnd )
                {
                    cerr << "Error interpreting " << tagIt << ": end tag is after begin tag" << endl;
                    return false;
                }
                *pseudoRef = str.substr( locBegin, locEnd - locBegin );
            }
            else
            {
                cerr << "Cannot interpret <" << tagIt << ">" << endl;
                return false;
            }
        }
        else
        {
            *pseudoRef = "";
        }

        locBegin = pseudoRef->find( "No errors" );
        if ( locBegin != string::npos )
        {
            *pseudoRef = pseudoRef->replace( locBegin, locBegin + strlen( "No errors" ), "" );
        }
    }

    return true;
}

void ErrorReader::display( ostream& os ) const
{
    if ( _ErrorString.empty() && _WarningString.empty() && _StackMessage.empty() )
    {
        os << "No errors / warnings / messages" << endl;
        return;
    }
    if ( !_ErrorString.empty() )
    {
        os << "Errors:" << endl << _ErrorString << endl;
    }
    if ( !_WarningString.empty() )
    {
        os << "Warning:" << endl << _WarningString << endl;
    }
    if ( !_StackMessage.empty() )
    {
        os << "Stacked messages:" << endl << _StackMessage << endl;
    }
}

bool ErrorReader::hasError( ftkError err ) const
{
    if ( err <= ftkError::FTK_OK )
    {
        return false;
    }
    else if ( _ErrorString.empty() )
    {
        return false;
    }

    stringstream convert;
    convert << int32( err ) << ":";

    return ( _ErrorString.find( convert.str() ) != string::npos );
}

bool ErrorReader::hasWarning( ftkError war ) const
{
    if ( war >= ftkError::FTK_OK )
    {
        return false;
    }
    else if ( _WarningString.empty() )
    {
        return false;
    }

    stringstream convert;
    convert << int32( war ) << ":";

    return ( _WarningString.find( convert.str() ) != string::npos );
}

bool ErrorReader::isOk() const
{
    return _ErrorString.empty() && _WarningString.empty();
}

bool ErrorReader::isError() const
{
    return !_ErrorString.empty();
}

bool ErrorReader::isWarning() const
{
    return !_WarningString.empty();
}
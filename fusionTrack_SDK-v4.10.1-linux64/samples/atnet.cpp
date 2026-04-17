#include <atnetInterface.h>

#include <array>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

#ifdef FORCED_DEVICE_DLL_PATH
#include <Windows.h>
#endif

using namespace std;

struct HelpShowHelper
{
    string OptName;
    bool Seen;
};

bool showAppHelpIfRequested( const deque< string >& args );

void getArgumentValue( const deque< string >& args,
                       const string& shortFlag,
                       const string& longFlag,
                       string& argValue );

void getArgumentPresence( const deque< string >& args,
                          const string& shortFlag,
                          const string& longFlag,
                          bool& argValue );

atnetError showHelp( atnetLib lib, const string& commandName, atnetError& status );

void displayAvailableCommands( const atnetCommandInfo* info, void* user );

void displayCommandOutput( const char* msg, void* userdata );

void displayCommandProgress( const char* msg, int oldValue, int newValue, void* userData );

int main( int argc, char* argv[] )
{
// Defines where to find Atracsys SDK dlls when FORCED_DEVICE_DLL_PATH is
// set.
#ifdef FORCED_DEVICE_DLL_PATH
    SetDllDirectory( (LPCTSTR)FORCED_DEVICE_DLL_PATH );
#endif

    deque< string > args{ argv + 1, argv + argc };

    if ( showAppHelpIfRequested( args ) )
    {
        return 0;
    }

    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& item ) { return item == "-d" || item == "--debug"; } ) );
    bool debuGoutput( pos != args.cend() );

    const char* config( nullptr );
    string timeout( "30" ), maxDevices( "0" ), scriptFileName( "" ), debugString( "" );
    bool quiet{ false };

    getArgumentValue( args, "-t", "--timeout", timeout );
    getArgumentValue( args, "-n", "--devices", maxDevices );
    getArgumentValue( args, "-s", "--script", scriptFileName );
    getArgumentPresence( args, "-q", "--quiet", quiet );

    constexpr size_t errorBufferSize( 1024u );
    array< char, errorBufferSize > errorBuffer{};

    atnetLib handle( atnetInit( config, errorBufferSize, errorBuffer.data() ) );

    if ( handle == nullptr )
    {
        cerr << "Could not create library handle" << endl;
        cerr << errorBuffer.data() << endl;
        return 1;
    }

    atnetError status( atnetError::Ok );

    status = atnetGetVersion( errorBufferSize, errorBuffer.data() );
    if ( status != atnetError::Ok )
    {
        if ( atnetGetError( handle, errorBufferSize, errorBuffer.data() ) == atnetError::Ok )
        {
            cerr << errorBuffer.data() << endl;
        }
        else
        {
            cerr << "Unspecified error" << endl;
        }
        if ( atnetClose( &handle ) != atnetError::Ok )
        {
            cerr << "Error closing the library handle" << endl;
        }
        return 1;
    }

    regex versionShortener( R"x((-g[a-f0-9]{8})[a-f0-9]{32} )x" );
    string versionStr( regex_replace( errorBuffer.data(), versionShortener, "$1 " ) );

    cout << "+------------------------------------------------------------------------------+" << endl;
    cout << "|" << setw( 79u ) << "|" << endl;
    cout << "|       Welcome to atnet " << setw( 54u ) << left << versionStr << "|" << endl;
    cout << "|" << setw( 79u ) << right << "|" << endl;
    cout << "+------------------------------------------------------------------------------+" << endl;

    if ( debuGoutput )
    {
        cout << "[DEBUG] Timeout set to " << timeout << endl;
        cout << "[DEBUG] Number of wanted devices " << maxDevices << endl;
        if ( !scriptFileName.empty() )
        {
            cout << "[DEBUG] Path to script " << scriptFileName << endl;
        }
    }

    string command( "scan " + timeout + " " + maxDevices );

    ifstream scriptFile{};

    if ( scriptFileName.empty() )
    {
        if ( !quiet )
        {
            cout << "Available commands:" << endl;
            showHelp( handle, "", status );
        }
    }
    else
    {
        scriptFile.open( scriptFileName );
        if ( !scriptFile.is_open() )
        {
            cerr << "Could not open wanted script file '" << scriptFileName << "'" << endl;
            return 1;
        }
        if ( debuGoutput )
        {
            cout << "[DEBUG] Reading command(s) from " << scriptFileName << endl;
        }
    }
    if ( status != atnetError::Ok )
    {
        if ( atnetGetError( handle, errorBufferSize, errorBuffer.data() ) == atnetError::Ok )
        {
            cerr << errorBuffer.data() << endl;
        }
        else
        {
            cerr << "Unspecified error" << endl;
        }
        if ( atnetClose( &handle ) != atnetError::Ok )
        {
            cerr << "Error closing the library handle" << endl;
        }
        return 1;
    }

    const regex commandSpecificHelp( R"x(^help +([^ ]+) *)x" );
    smatch match{};

    cout << command << endl;
    while ( command != "exit" )
    {
        if ( command == "help" )
        {
            status = showHelp( handle, "", status );
        }
        else if ( regex_match( command, match, commandSpecificHelp ) )
        {
            status = showHelp( handle, match[ 1u ], status );
        }
        else
        {
            status = atnetExecuteCommand(
              handle, command.c_str(), displayCommandOutput, nullptr, displayCommandProgress, nullptr );
        }
        if ( status == atnetError::Ok )
        {
            cout << "--> OK" << endl;
        }
        else
        {
            if ( command.find( "help" ) == string::npos &&
                 atnetGetError( handle, errorBufferSize, errorBuffer.data() ) == atnetError::Ok )
            {
                cerr << errorBuffer.data() << endl;
            }
            cerr << "--> FAIL" << endl;

            if ( scriptFile.is_open() )
            {
                // If a script file is open and a command fails, the program exits.
                if ( atnetClose( &handle ) != atnetError::Ok )
                {
                    cerr << "Error closing the library handle" << endl;
                }
                return 1;
            }
        }

        command = "";
        if ( scriptFile.is_open() )
        {
            if ( scriptFile.eof() )
            {
                cout << "Reached end of script '" << scriptFileName << "', exiting" << endl;
                break;
            }
            getline( scriptFile, command );
            if ( command.empty() )
            {
                cout << "Empty line read from script '" << scriptFileName << "', exiting" << endl;
                break;
            }
            cout << command << endl;
        }
        else
        {
            while ( command.empty() )
            {
                getline( cin, command );
            }
        }
    }

    cout << "Exiting..." << endl;

    if ( atnetClose( &handle ) != atnetError::Ok )
    {
        cerr << "Error closing the library handle" << endl;
        return 1;
    }

    return 0;
}

bool showAppHelpIfRequested( const deque< string >& args )
{
    auto pos( find_if(
      args.cbegin(), args.cend(), []( const string& item ) { return item == "-h" || item == "--help"; } ) );
    bool showHelp( pos != args.cend() );
    if ( showHelp )
    {
        cout << "atnet executable" << endl << endl;
        cout << "Software interacting with the fTk bootloader" << endl << endl;

        cout << setfill( ' ' );
        cout << "\t" << left << setw( 16u ) << "-t/--timeout"
             << "optional, timeout in seconds for device discovery" << endl;
        cout << "\t" << left << setw( 16u ) << "-n/--devices"
             << "optional, number of devices after which the discovery sequence is aborted" << endl;
        cout << "\t" << left << setw( 16u ) << "-s/--script"
             << "optional, file containing commands to be run sequentially, separated by new lines" << endl;
        cout << "\t" << left << setw( 16u ) << "-d/--debug"
             << "set the debug mode" << endl;
        cout << "\t" << left << setw( 16u ) << "-q/--quiet"
             << "does not print command list when starting the program" << endl;
        cout << "\t" << left << setw( 16u ) << "-h/--help"
             << "show this help message and exit" << endl;
    }

    return showHelp;
}

void getArgumentValue( const deque< string >& args,
                       const string& shortFlag,
                       const string& longFlag,
                       string& argValue )
{
    auto pos( find_if( args.cbegin(),
                       args.cend(),
                       [ & ]( const string& item ) { return item == shortFlag || item == longFlag; } ) );
    if ( pos != args.cend() && ++pos != args.cend() )
    {
        argValue = *pos;
    }
}

void getArgumentPresence( const deque< string >& args,
                          const string& shortFlag,
                          const string& longFlag,
                          bool& argValue )
{
    auto pos( find_if( args.cbegin(),
                       args.cend(),
                       [ & ]( const string& item ) { return item == shortFlag || item == longFlag; } ) );
    argValue = pos != args.cend();
}

atnetError showHelp( atnetLib lib, const string& commandName, atnetError& status )
{
    if ( !commandName.empty() )
    {
        HelpShowHelper helper = { commandName, false };
        cout << endl;
        status = atnetListCommands( lib, displayAvailableCommands, &helper );
        if ( !helper.Seen )
        {
            cerr << "No such command: '" << commandName << "'" << endl;
            return atnetError::ErrUnknownCommand;
        }
    }
    else
    {
        cout << endl;
        cout << "help -> []"
             << ": Shows list of commands" << endl
             << endl;
        cout << "help commandName -> []"
             << ": Shows detailed help on specific command" << endl
             << endl;
        status = atnetListCommands( lib, displayAvailableCommands, nullptr );
        cout << "exit -> []: Exits the program" << endl << endl;
    }

    return atnetError::Ok;
}

void displayAvailableCommands( const atnetCommandInfo* info, void* user )
{
    if ( user != nullptr )
    {
        HelpShowHelper* helper( reinterpret_cast< HelpShowHelper* >( user ) );
        if ( helper == nullptr )
        {
            cerr << "Error casting user parameter of displayAvailableCommands callback" << endl;
            return;
        }
        if ( strcmp( helper->OptName.c_str(), info->Name ) != 0 )
        {
            return;
        }
        helper->Seen = true;
    }
    cout << info->Name;
    if ( strlen( info->InputArguments ) > 0u )
    {
        cout << " " << info->InputArguments;
    }
    cout << " -> ";
    if ( strlen( info->OutputArguments ) > 0u )
    {
        cout << info->OutputArguments;
    }
    else
    {
        cout << "[]";
    }

    if ( user != nullptr )
    {
        cout << endl << "\t" << info->Help << endl << endl;
    }
    else
    {
        cout << ": " << info->Description << endl << endl;
    }
}

void displayCommandOutput( const char* msg, void* )
{
    cout << msg << endl;
}

void displayCommandProgress( const char* msg, int oldValue, int newValue, void* )
{
    if ( newValue == -1 )
    {
        cout << endl;
        return;
    }
    if ( newValue != oldValue )
    {
        if ( oldValue >= 0 )
        {
            cout << "\r";
        }
        cout << msg << newValue << "%";
    }
}

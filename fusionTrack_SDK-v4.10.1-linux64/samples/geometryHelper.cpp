#include "geometryHelper.hpp"

#include <ftkInterface.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <new>
#include <string>

using namespace std;

void getDataDirOptionId( uint64 sn, void* user, ftkOptionsInfo* oi );

int loadGeometry( ftkLibrary lib, const uint64& sn, const string& fileName, ftkGeometry& geometry )
{
    ftkRigidBody tmp{};
    int retVal( loadRigidBody( lib, fileName, tmp ) );
    if ( retVal < 2 )
    {
        geometry = tmp;
    }
    return retVal;
}

int loadRigidBody( ftkLibrary lib, const string& fileName, ftkRigidBody& geometry )
{
    string fullFileName( "" );
    bool fromDataDir( false );
    if ( !getFullFilePath( lib, fileName, fullFileName, &fromDataDir ) )
    {
        return 2;
    }
    ftkBuffer buffer{};
    if ( !loadFileInBuffer( fullFileName, buffer ) )
    {
        return 2;
    }

    ftkError err( ftkLoadRigidBodyFromFile( lib, &buffer, &geometry ) );

    if ( err != ftkError::FTK_OK )
    {
        return 2;
    }

    return fromDataDir ? 1 : 0;
}

bool getFullFilePath( ftkLibrary lib, const string& fileName, string& fullFilePath, bool* fromSystem )
{
    static uint32 FTK_OPT_DATA_DIR( 0u );
    static string OPT_DIR( "" );
    if ( FTK_OPT_DATA_DIR == 0u )
    {
        if ( ftkEnumerateOptions( lib, 0uLL, getDataDirOptionId, &FTK_OPT_DATA_DIR ) !=
             ftkError::FTK_WAR_OPT_GLOBAL_ONLY )
        {
            cerr << "Could not get the data directory option ID" << endl;
            return false;
        }
        else if ( FTK_OPT_DATA_DIR == 0u )
        {
            cerr << "Could not get the data directory option ID" << endl;
            return false;
        }
    }
    if ( OPT_DIR.empty() )
    {
        ftkBuffer buffer{};
        if ( ftkGetData( lib, 0uLL, FTK_OPT_DATA_DIR, &buffer ) != ftkError::FTK_OK || buffer.size < 1u )
        {
            return false;
        }
        OPT_DIR.assign( buffer.data );
    }

    ifstream input{ fileName };
    if ( input.is_open() )
    {
        fullFilePath = fileName;
        if ( fromSystem != nullptr )
        {
            *fromSystem = false;
        }
        return true;
    }
    else
    {
        input.open( OPT_DIR + "/" + fileName );
        if ( input.is_open() )
        {
#ifdef ATR_WIN
            fullFilePath = OPT_DIR + "\\" + fileName;
#else
            fullFilePath = OPT_DIR + "/" + fileName;
#endif
            if ( fromSystem != nullptr )
            {
                *fromSystem = true;
            }
            return true;
        }
    }

    return false;
}

bool loadFileInBuffer( const string& fullFilePath, ftkBuffer& buffer )
{
    ifstream input( fullFilePath, ios::binary | ios::ate );

    if ( !input.is_open() )
    {
        cerr << "Could not open file '" << fullFilePath << "'" << endl;
        return false;
    }

    buffer.reset();
    streampos pos( input.tellg() );
    input.seekg( 0u, ios::beg );
    input.read( buffer.data, static_cast< streamsize >( pos ) );
    if ( input.fail() )
    {
        cerr << "Could not read contents of file '" << fullFilePath << "'" << endl;
        return false;
    }
    buffer.size = static_cast< uint32 >( pos );

    return true;
}

// ----------------------------------------------------------------------------

void getDataDirOptionId( uint64 sn, void* user, ftkOptionsInfo* oi )
{
    uint32_t* ptr( reinterpret_cast< uint32_t* >( user ) );
    if ( ptr == nullptr )
    {
        return;
    }
    if ( strcmp( oi->name, "Data Directory" ) == 0 )
    {
        *ptr = oi->id;
    }
}


#include "CPP_fusionTrack.hpp"

#include "CPP_frame.hpp"
#include "CPP_geometry.hpp"
#include "CPP_helpers.hpp"
#include "CPP_option.hpp"

#include <fstream>
#include <matrix.h>
#include <mex.h>

FusionTrack::~FusionTrack()
{
    check( ftkClose( &handle ) );
    ftkDeleteFrame( frame );
}

bool FusionTrack::initialise( const std::string& configurationFile )
{
    const char* params( nullptr );
    if ( !configurationFile.empty() )
    {
        params = configurationFile.c_str();
    }
    ftkBuffer errs;
    handle = ftkInitExt( params, &errs );
    if ( handle == nullptr )
    {
        mexPrintf( "ERROR: cannot initialise library handle\n" );
        mexPrintf( "%s\n", errs.data );
        return false;
    }

    frame = ftkCreateFrame();
    if ( frame == nullptr )
    {
        mexPrintf( "ERROR: cannot allocate frame\n" );
        return false;
    }

    if ( ftkSetFrameOptions( false, 10u, 0u, 0u, cstMax3DFiducials, cstMaxMarkers, frame ) !=
         ftkError::FTK_OK )
    {
        mexPrintf( "ERROR: cannot set frame options\n" );
        return false;
    }

    return true;
}

std::vector< uint64 > FusionTrack::devices()
{
    std::vector< uint64 > vec;
    check( ftkEnumerateDevices( handle, devicesCallback, &vec ) );
    return vec;
}

std::vector< ftkRigidBody > FusionTrack::geometries( uint64 device )
{
    std::vector< ftkRigidBody > vec;
    check( ftkEnumerateRigidBodies( handle, device, geometriesCallback, &vec ) );
    return vec;
}

void FusionTrack::clearGeometry( uint64 device, uint32 id )
{
    check( ftkClearGeometry( handle, device, id ) );
}

void FusionTrack::setGeometry( uint64 device, const ftkRigidBody& in )
{
    check( ftkSetRigidBody( handle, device, const_cast< ftkRigidBody* >( &in ) ) );
}

void FusionTrack::loadGeometry( uint64 device, const std::string& fileName, ftkRigidBody& geometryOut )
{
    std::vector< ftkOptionsInfo > optionsVector = options( device );

    // Get data directory
    std::string DATA_DIR( "" );
    ftkBuffer buffer;
    for ( auto& it : optionsVector )
    {
        if ( strcmp( it.name, "Data Directory" ) == 0 )
        {
            check( ftkGetData( handle, device, it.id, &buffer ) );
            if ( buffer.size < 1u )
            {
                mexErrMsgIdAndTxt( "FusionTrack:LOADGEOMETRY", "Could not get data directory." );
                return;
            }
            else
            {
                DATA_DIR.assign( buffer.data );
                break;
            }
        }
    }
    // Get geometry file directory
    std::ifstream input{ fileName };
    if ( !input.is_open() )
    {
        input.open( DATA_DIR + "/" + fileName );
        if ( !input.is_open() )
        {
            std::string err( "Couldn't open geometry file : " + DATA_DIR + "/" + fileName );
            mexErrMsgIdAndTxt( "FusionTrack:LOADGEOMETRY", err.c_str() );
        }
    }

    // Get geometry file content
    buffer.reset();
    input.seekg( 0u, std::ios::end );
    std::streampos pos( input.tellg() );
    input.seekg( 0u, std::ios::beg );
    input.read( buffer.data, static_cast< std::streamsize >( pos ) );
    if ( input.fail() )
    {
        mexErrMsgIdAndTxt( "FusionTrack:LOADGEOMETRY", "Error while reading geometry file." );
    }
    buffer.size = static_cast< uint32 >( pos );

    check( ftkLoadRigidBodyFromFile( handle, &buffer, &geometryOut ) );
}

std::vector< ftkOptionsInfo > FusionTrack::options( uint64 device )
{
    std::vector< ftkOptionsInfo > vec;
    check( ftkEnumerateOptions( handle, device, optionsCallback, &vec ) );
    return vec;
}

int32 FusionTrack::getInt32( uint64 device, uint32 optId, ftkOptionGetter what )
{
    int32 out;
    check( ftkGetInt32( handle, device, optId, &out, what ) );
    return out;
}

void FusionTrack::setInt32( uint64 device, uint32 optId, int32 val )
{
    check( ftkSetInt32( handle, device, optId, val ) );
}

float32 FusionTrack::getFloat32( uint64 device, uint32 optId, ftkOptionGetter what )
{
    float32 out;
    check( ftkGetFloat32( handle, device, optId, &out, what ) );
    return out;
}

void FusionTrack::setFloat32( uint64 device, uint32 optId, float32 val )
{
    check( ftkSetFloat32( handle, device, optId, val ) );
}

std::string FusionTrack::getData( uint64 device, uint32 optId )
{
    ftkBuffer buffer;
    check( ftkGetData( handle, device, optId, &buffer ) );
    return std::string( buffer.data, buffer.size );
}

void FusionTrack::setData( uint64 device, uint32 optId, const std::string &data )
{
    ftkBuffer buffer{};
    buffer.reset();
    if ( data.length() >= sizeof( buffer.data ) )
    {
        mexErrMsgIdAndTxt( "FusionTrack:SETDATA", "Data size is too big" );
    }
    copy( data.cbegin(), data.cend(), buffer.data );
    buffer.size = static_cast< uint32_t >( data.length() );

    check( ftkSetData( handle, device, optId, &buffer ) );
}

void FusionTrack::getLastFrame( uint64 device, uint32 timeout )
{
    check( ftkGetLastFrame( handle, device, frame, timeout ) );
}

const ftkFrameQuery& FusionTrack::getFrame() const
{
    return *frame;
}

void FusionTrack::devicesCallback( uint64 sn, void* user, ftkDeviceType )
{
    if ( user == nullptr )
    {
        return;
    }
    std::vector< uint64 >* vec( reinterpret_cast< std::vector< uint64 >* >( user ) );
    if ( vec != nullptr )
    {
        vec->emplace_back( sn );
    }
}

void FusionTrack::geometriesCallback( uint64 sn, void* user, ftkRigidBody* in )
{
    if ( user == nullptr )
    {
        return;
    }
    std::vector< ftkRigidBody >* vec( reinterpret_cast< std::vector< ftkRigidBody >* >( user ) );
    if ( vec != nullptr )
    {
        vec->emplace_back( *in );
    }
}

void FusionTrack::optionsCallback( uint64 sn, void* user, ftkOptionsInfo* in )
{
    if ( user == nullptr )
    {
        return;
    }
    std::vector< ftkOptionsInfo >* vec( reinterpret_cast< std::vector< ftkOptionsInfo >* >( user ) );
    if ( vec != nullptr )
    {
        vec->emplace_back( *in );
    }
}

void FusionTrack::check( ftkError err )
{
    if ( err != ftkError::FTK_OK )
    {
        ftkBuffer toto;
        if ( ftkGetLastErrorString( handle, sizeof( toto.data ), toto.data ) == ftkError::FTK_OK )
        {
            if ( err > ftkError::FTK_OK )
            {
                mexErrMsgIdAndTxt( "FusionTrack:check", toto.data );
            }
            else
            {
                mexWarnMsgIdAndTxt( "FusionTrack:check", toto.data );
            }
        }
        else
        {
            sprintf(
              toto.data, "Unknown error (%i), could not retrieved detailed error info\n", int32( err ) );
            if ( err > ftkError::FTK_OK )
            {
                mexErrMsgIdAndTxt( "FusionTrack:check", toto.data );
            }
            else
            {
                mexWarnMsgIdAndTxt( "FusionTrack:check", toto.data );
            }
        }
    }
}

void mexFunction( int nOut, mxArray* pOut[], int nIn, const mxArray* pIn[] )
{
    // Get the command string
    char cmd[ 64 ];
    if ( nIn < 1 || mxGetString( pIn[ 0 ], cmd, sizeof( cmd ) ) )
    {
        mexErrMsgIdAndTxt( "FusionTrack:main",
                           "First input should be a command string less than 64 characters long." );
    }

    if ( nOut < 0 )
    {
        mexErrMsgIdAndTxt( "FusionTrack:main", "DEVICES: Invalid output arguments." );
    }

    // --------------- New ---------------------

    if ( !strcmp( "new", cmd ) )
    {
        // Check parameters
        if ( nOut != 1 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:NEW", "One output expected." );
        }
        // Return a handle to a new C++ instance
        auto ptr( new FusionTrack );
        char file[ 256u ] = { 0 };
        if ( nIn > 1 )
        {
            mxGetString( pIn[ 1 ], file, sizeof( file ) );
        }
        if ( ptr->initialise( static_cast< std::string >( file ) ) )
        {
            pOut[ 0 ] = ptr2mat< FusionTrack >( ptr );
        }
        else
        {
            mexErrMsgIdAndTxt( "FusionTrack:NEW", "Cannot initialise FusionTrack class." );
        }
        return;
    }

    // Check there is a second input, which should be the class instance handle
    if ( nIn < 2 )
    {
        mexErrMsgIdAndTxt( "FusionTrack:main", "Second input should be a class instance handle." );
    }

    // --------------- Delete ---------------------

    if ( !strcmp( "delete", cmd ) )
    {
        destroy< FusionTrack >( pIn[ 1 ] );
        // Warn if other commands were ignored
        if ( nOut != 0 || nIn != 2 )
        {
            mexWarnMsgIdAndTxt( "FusionTrack:DELETE", "Unexpected arguments ignored." );
        }
        return;
    }

    // Get the class instance pointer from the second input
    FusionTrack* pFusionTrack = mat2ptr< FusionTrack >( pIn[ 1 ] );

    // --------------- METHODS ---------------------

    if ( !strcmp( "devices", cmd ) )
    {
        std::vector< uint64 > devices = pFusionTrack->devices();
        pOut[ 0 ] = vec2mat< uint64 >( devices );
        return;
    }

    if ( !strcmp( "geometries", cmd ) )
    {
        if ( nIn < 3 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GEOMETRIES", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        std::vector< ftkRigidBody > geometries( pFusionTrack->geometries( device ) );
        pOut[ 0 ] = geom2mat( geometries );
        return;
    }

    if ( !strcmp( "options", cmd ) )
    {
        if ( nIn < 3 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:OPTIONS", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        std::vector< ftkOptionsInfo > options( pFusionTrack->options( device ) );
        pOut[ 0 ] = opt2mat( options );
        return;
    }

    if ( !strcmp( "cleargeometry", cmd ) )
    {
        if ( nIn < 4 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:CLEARGEOMETRY", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 geomId = mat2base< uint32 >( pIn[ 3 ] );
        pFusionTrack->clearGeometry( device, geomId );
        return;
    }

    if ( !strcmp( "setgeometry", cmd ) )
    {
        if ( nIn < 4 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:SETGEOMETRY", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        
        ftkRigidBody geom;
        for ( uint32 i( 3 ); i < nIn ; ++i )
        {   
            if ( !mat2geom( pIn[ i ], geom ) )
            {
                mexErrMsgIdAndTxt( "FusionTrack:SETGEOMETRY", "Invalid geometry." );
            }
            pFusionTrack->setGeometry( device, geom );
        }
        
        return;
    }

    if ( !strcmp( "getint32", cmd ) )
    {
        if ( nIn < 4 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETINT32", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );
        ftkOptionGetter what( nIn < 5 ? ftkOptionGetter::FTK_VALUE
                                      : static_cast< ftkOptionGetter >( mat2base< uint8 >( pIn[ 4 ] ) ) );
        pOut[ 0 ] = base2mat< int32 >( pFusionTrack->getInt32( device, optId, what ) );
        return;
    }

    if ( !strcmp( "setint32", cmd ) )
    {
        if ( nIn < 5 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:SETINT32", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );
        int32 val = mat2base< int32 >( pIn[ 4 ] );
        pFusionTrack->setInt32( device, optId, val );
        return;
    }

    if ( !strcmp( "getfloat32", cmd ) )
    {
        if ( nIn < 4 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETFLOAT32", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );
        ftkOptionGetter what( nIn < 5 ? ftkOptionGetter::FTK_VALUE
                                      : static_cast< ftkOptionGetter >( mat2base< uint8 >( pIn[ 4 ] ) ) );
        pOut[ 0 ] = base2mat< float32 >( pFusionTrack->getFloat32( device, optId, what ) );
        return;
    }

    if ( !strcmp( "setfloat32", cmd ) )
    {
        if ( nIn < 5 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:SETFLOAT32", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );
        float32 val = mat2base< float32 >( pIn[ 4 ] );
        pFusionTrack->setFloat32( device, optId, val );
        return;
    }

    if ( !strcmp( "getdata", cmd ) )
    {
        if ( nIn < 3 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETDATA", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );
        pOut[ 0 ] = mxCreateString( pFusionTrack->getData( device, optId ).c_str() );
        return;
    }

    if ( !strcmp( "setdata", cmd ) )
    {
        if ( nIn < 5 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETDATA", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        uint32 optId = mat2base< uint32 >( pIn[ 3 ] );

        char data[ 1024 ] = {0};
        if ( mxGetString( pIn[ 4 ], data, sizeof( data ) ) )
        {
            mexErrMsgIdAndTxt( "FusionTrack:SETDATA", "Invalid data, char array needed" );
        }
        pFusionTrack->setData( device, optId, static_cast< std::string >( data ) );

        return;
    }

    if ( !strcmp( "getframe", cmd ) )
    {
        if ( nIn < 3 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETFRAME", "Invalid number of input arguments." );
        }
        uint64 device = mat2base< uint64 >( pIn[ 2 ] );
        pFusionTrack->getLastFrame( device );
        pOut[ 0 ] = frame2mat( pFusionTrack->getFrame() );
        return;
    }

    if ( !strcmp( "getgeometriesfromfile", cmd ) )
    {
        char file[ 256u ] = { 0 };   
        
        if ( nIn < 4 )
        {
            mexErrMsgIdAndTxt( "FusionTrack:GETRIGIDBODIES", "Invalid number of input arguments." );
        }

        uint64 device = mat2base< uint64 >( pIn[ 2 ] );

        std::vector< ftkRigidBody > geometries{};
        for ( uint32 i( 3 ); i < nIn; ++i )
        {
            if ( mxGetString( pIn[ i ], file, sizeof( file ) ) )
            {
                mexErrMsgIdAndTxt( "FusionTrack:GETRIGIDBODIES", "Invalid file name, char array needed" );
            }

            ftkRigidBody rigidBody{};
            pFusionTrack->loadGeometry( device, static_cast< std::string >( file ), rigidBody );
            geometries.emplace_back( rigidBody );
        }
       
        pOut[ 0 ] = geom2mat( geometries );

        return;
    }
}

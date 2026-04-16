#include "CPP_geometry.hpp"

#include "CPP_helpers.hpp"

#include <sstream>
#include <matrix.h>
#include <mex.h>

static const char* acGeometry[] = { "geometryId", "version", "fiducials", "divots" };
static const char* acFiducial[] = { "positions", "normalVectors" };

#define MEX_FIND( var, name, par )                       \
    mxArray* var = mxGetField( par, 0, name );           \
    if ( !var )                                          \
    {                                                    \
        char msg[ 1000 ];                                \
        sprintf( msg, "cannot find '%s' field.", name ); \
        mexErrMsgIdAndTxt( "Geometry:mat2geom", msg );   \
        return false;                                    \
    }

bool mat2geom( const mxArray* pMexArray, ftkRigidBody& out )
{
    MEX_FIND( id, acGeometry[ 0 ], pMexArray );
    MEX_FIND( version, acGeometry[ 1 ], pMexArray );
    MEX_FIND( fids, acGeometry[ 2 ], pMexArray );
    MEX_FIND( divots, acGeometry[ 3 ], pMexArray );

    MEX_FIND( positions, acFiducial[ 0 ], fids );
    MEX_FIND( normalVectors, acFiducial[ 1 ], fids );

    out.geometryId = mat2base< uint32 >( id );
    out.version = mat2base< uint32 >( version );

    std::vector< std::pair< size_t, size_t > > sizes;
    sizes.emplace_back( mxGetM( positions ), mxGetN( positions ) );
    sizes.emplace_back( mxGetM( normalVectors ), mxGetN( normalVectors ) );
    sizes.emplace_back( mxGetM( divots ), mxGetN( divots ) );
    for (auto &it : sizes )
    {
        if ( it.first != 3u )
        {
            mexErrMsgIdAndTxt( "Geometry:mat2geom", "Matrix of points is probabely transposed." );
        }
        if ( it.second > FTK_MAX_FIDUCIALS )
        {
            mexErrMsgIdAndTxt( "Geometry:mat2geom", "Invalid number of points." );
        }
    }
    
    out.pointsCount = static_cast< uint32 >( sizes[ 0 ].second );
    double* ptr( mxGetPr( positions ) );
    for ( uint32 u( 0u ); u < out.pointsCount; ++u )
    {
        out.fiducials[ u ].position.x = static_cast< floatXX >( *ptr++ );
        out.fiducials[ u ].position.y = static_cast< floatXX >( *ptr++ );
        out.fiducials[ u ].position.z = static_cast< floatXX >( *ptr++ );
    }

    ptr = mxGetPr( normalVectors );
    for ( uint32 u( 0u ); u < static_cast< uint32 >( sizes[ 1 ].second ); ++u )
    {
        out.fiducials[ u ].normalVector.x = static_cast< floatXX >( *ptr++ );
        out.fiducials[ u ].normalVector.y = static_cast< floatXX >( *ptr++ );
        out.fiducials[ u ].normalVector.z = static_cast< floatXX >( *ptr++ );
    }

    out.divotsCount = static_cast< uint32 >( sizes[ 2 ].second );
    ptr = mxGetPr( divots );
    for ( uint32 u( 0u ); u < out.divotsCount; ++u )
    {
        out.divotPositions[ u ].x = static_cast< floatXX >( *ptr++ );
        out.divotPositions[ u ].y = static_cast< floatXX >( *ptr++ );
        out.divotPositions[ u ].z = static_cast< floatXX >( *ptr++ );
    }

    return true;
}

mxArray* geom2mat( const std::vector< ftkRigidBody >& in )
{
    if ( in.empty() )
    {
        return EMPTY_ARRAY;
    }

    mxArray* pMexGeometry = mxCreateStructMatrix( static_cast< mwSize >( in.size() ), 1, 4, acGeometry );
    
    for ( size_t v( 0u ); v < in.size(); ++v )
    {
        const ftkRigidBody& geom( in[ v ] );
        if ( geom.pointsCount == 0u )
        {
            mexErrMsgIdAndTxt( "Geometry:geom2mat", "Invalid number of points" );
        }

        mxArray* pMexFiducials = mxCreateStructMatrix( 1, 1, 2, acFiducial );

        mxArray* id = base2mat< uint32 >( geom.geometryId );
        mxArray* version = base2mat< uint32 >( geom.version );

        mxArray* positions = mxCreateDoubleMatrix( 3, geom.pointsCount, mxREAL );
        mxArray* normalVectors = mxCreateDoubleMatrix( 3, geom.pointsCount, mxREAL );
        if ( positions == nullptr || normalVectors == nullptr )
        {
            mexErrMsgIdAndTxt( "Geometry:geom2mat", "Can't create geometry position field" );
        }

        double* ptrPos = mxGetPr( positions );
        double* ptrNorm = mxGetPr( normalVectors );
        for ( uint32 u( 0u ); u < geom.pointsCount; ++u )
        {
            *ptrPos++ = geom.fiducials[ u ].position.x;
            *ptrPos++ = geom.fiducials[ u ].position.y;
            *ptrPos++ = geom.fiducials[ u ].position.z;

            *ptrNorm++ = geom.fiducials[ u ].normalVector.x;
            *ptrNorm++ = geom.fiducials[ u ].normalVector.y;
            *ptrNorm++ = geom.fiducials[ u ].normalVector.z;
        }

        mxArray* divots = mxCreateDoubleMatrix( 3, geom.divotsCount, mxREAL );
        double* ptrDivots = mxGetPr( divots );
        for ( uint32 u( 0u ); u < geom.divotsCount; ++u )
        {
            *ptrDivots++ = geom.divotPositions[ u ].x;
            *ptrDivots++ = geom.divotPositions[ u ].y;
            *ptrDivots++ = geom.divotPositions[ u ].z;
        }

        mxSetField( pMexFiducials, static_cast< mwIndex >( 0u ), acFiducial[ 0 ], positions );
        mxSetField( pMexFiducials, static_cast< mwIndex >( 0u ), acFiducial[ 1 ], normalVectors );

        mxSetField( pMexGeometry, static_cast< mwIndex >( v ), acGeometry[ 0 ], id );
        mxSetField( pMexGeometry, static_cast< mwIndex >( v ), acGeometry[ 1 ], version );
        mxSetField( pMexGeometry, static_cast< mwIndex >( v ), acGeometry[ 2 ], pMexFiducials );
        mxSetField( pMexGeometry, static_cast< mwIndex >( v ), acGeometry[ 3 ], divots );
    }
    return pMexGeometry;
}

#pragma once

#include <ftkInterface.h>

#include <string>
#include <typeinfo>
#include <vector>

#include <mex.h>

#define CLASS_HANDLE_SIGNATURE 0xFF00F0A5

template< class base >

mxClassID getTypeId()
{
    if ( typeid( int8 ) == typeid( base ) )
    {
        return mxINT8_CLASS;
    }
    if ( typeid( uint8 ) == typeid( base ) )
    {
        return mxUINT8_CLASS;
    }
    if ( typeid( int16 ) == typeid( base ) )
    {
        return mxINT16_CLASS;
    }
    if ( typeid( uint16 ) == typeid( base ) )
    {
        return mxUINT16_CLASS;
    }
    if ( typeid( int32 ) == typeid( base ) )
    {
        return mxINT32_CLASS;
    }
    if ( typeid( uint32 ) == typeid( base ) )
    {
        return mxUINT32_CLASS;
    }
    if ( typeid( int64 ) == typeid( base ) )
    {
        return mxINT64_CLASS;
    }
    if ( typeid( uint64 ) == typeid( base ) )
    {
        return mxUINT64_CLASS;
    }
    if ( typeid( float32 ) == typeid( base ) )
    {
        return mxSINGLE_CLASS;
    }
    if ( typeid( float64 ) == typeid( base ) )
    {
        return mxDOUBLE_CLASS;
    }
    mexErrMsgIdAndTxt( "helpers:getTypeId", "Unknown type id." );
    return mxUNKNOWN_CLASS;
}

#define EMPTY_ARRAY mxCreateDoubleMatrix( 0, 0, mxREAL )

template< class base >
class class_handle
{
public:
    class_handle( base* ptr )
        : ptr_m( ptr )
        , name_m( typeid( base ).name() )
        , signature_m( CLASS_HANDLE_SIGNATURE )
    {
    }

    ~class_handle()
    {
        signature_m = 0;
        delete ptr_m;
    }

    bool isValid()
    {
        return ( ( signature_m == CLASS_HANDLE_SIGNATURE ) &&
                 !strcmp( name_m.c_str(), typeid( base ).name() ) );
    }

    base* ptr()
    {
        return ptr_m;
    }

private:
    uint32 signature_m;
    std::string name_m;
    base* ptr_m;
};

template< class base >
mxArray* ptr2mat( base* ptr )
{
    mexLock();
    mxArray* out = mxCreateNumericMatrix( 1, 1, mxUINT64_CLASS, mxREAL );
    *( (uint64*)mxGetData( out ) ) = reinterpret_cast< uint64 >( new class_handle< base >( ptr ) );
    return out;
}

template< class base >
class_handle< base >* mat2hptr( const mxArray* in )
{
    if ( mxGetNumberOfElements( in ) != 1 || mxGetClassID( in ) != mxUINT64_CLASS || mxIsComplex( in ) )
    {
        mexErrMsgIdAndTxt( "helper:mat2hptr", "Input must be a real uint64 scalar." );
    }
    class_handle< base >* ptr = reinterpret_cast< class_handle< base >* >( *( (uint64*)mxGetData( in ) ) );
    if ( !ptr->isValid() )
    {
        mexErrMsgIdAndTxt( "helper:mat2hptr", "Handle not valid." );
    }
    return ptr;
}

template< class base >
mxArray* vec2mat( const std::vector< base >& in )
{
    if ( in.empty() )
    {
        return EMPTY_ARRAY;
    }
    mwSize dims[] = { (mwSize)in.size() };
    mxArray* out = mxCreateNumericArray( 1, dims, getTypeId< base >(), mxREAL );
    base* pdata = (base*)mxGetData( out );
    for ( size_t u = 0; u < in.size(); u++ )
    {
        *pdata++ = in[ u ];
    }
    return out;
}

template< class base >
mxArray* base2mat( const base& in )
{
    mwSize dims[] = { (mwSize)1 };
    mxArray* out = mxCreateNumericArray( 1, dims, getTypeId< base >(), mxREAL );
    base* pdata = (base*)mxGetData( out );
    *pdata = in;
    return out;
}

template< class base >
base mat2base( const mxArray* in )
{
    if ( mxGetClassID( in ) != getTypeId< base >() )
    {
        mexErrMsgIdAndTxt( "helper:mat2base", "Invalid type." );
    }
    return *(base*)mxGetData( in );
}

template< class base >
void destroy( const mxArray* in )
{
    delete mat2hptr< base >( in );
    mexUnlock();
}

template< class base >
base* mat2ptr( const mxArray* in )
{
    return mat2hptr< base >( in )->ptr();
}

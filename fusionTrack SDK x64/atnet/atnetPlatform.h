// ===========================================================================

/*!
    This file is part of the ATRACSYS fusiontrack library.
    Copyright (C) 2003-2015 by Atracsys LLC. All rights reserved.

  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
  WRITTEN PERMISSION OF ATRACSYS.

  \file     atnetPlatform.h
  \brief    Automatically detect compiler and platform.

*/
// ===========================================================================

#pragma once

/**
 * \addtogroup atnet
 * \{
 */

// Define compiler

#if defined( _MSC_VER )
/// Compiler is Microsoft Visual C++
#define ATR_MSVC 1
/// Microsoft Visual C++ version
#define ATR_MSVC_VER _MSC_VER
#endif

#if defined( __GNUC__ )
/// Compiler is GNU GCC
#define ATR_GCC 1
/// GNU GCC Version
#define ATR_GCC_VER __GNUC__
#endif

#if defined( __ICC ) || defined( __INTEL_COMPILER )
/// Compiler is Intel CC
#define ATR_ICC 1
#endif

#if defined( __BORLANDC__ ) || defined( __BCPLUSPLUS__ )
/// Compiler is Borland C++ Builder
#define ATR_BORLAND 1
#endif

#if defined( __MINGW32__ )
/// Compiler is Mingw
#define ATR_MINGWIN 1
#endif

#if defined( __CYGWIN32__ )
/// Compiler is Cygwin
#define ATR_CYGWIN 1
#endif

#if defined( _CVI_ )
/// Compiler is Labwindows CVI
#define ATR_CVI 1
#endif

// ----------------------
// Software target macros
// ----------------------

#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )
/// Windows 32 bits architecture
#define ATR_WIN32
#endif

#if defined( WIN64 ) || defined( _WIN64 ) || defined( __WIN64__ )
/// Windows 32 bits architecture
#define ATR_WIN64
#endif

#if defined( ATR_WIN32 ) || defined( ATR_WIN64 )
/// Target is windows
#define ATR_WIN
#else
/// Target is not windows
#define ATR_LIN
#endif

#if defined( _M_IX86 ) || defined( __i386__ )
/// X86 compatible processor
#define ATR_X86
#endif

#if defined( _M_X64 ) || defined( _M_AMD64 ) || defined( __x86_64 )
/// 64bits processor
#define ATR_X64
#endif

// C function export.

#if defined( ATR_WIN32 )
/// External prefix for DLL exportation
#define ATR_EXPORT extern "C" __declspec( dllexport )
/// Internal prefix for DLL exportation
#define ATR_EXPORT_INT extern "C"
#elif defined( ATR_CVI )
/// CVI is a C compiler. Use the C declaration
#define ATR_EXPORT __declspec( dllexport )
/// CVI is a C compiler
#define ATR_EXPORT_INT
#else
/// External prefix for DLL exportation
#define ATR_EXPORT extern "C"
/// Internal prefix for DLL exportation
#define ATR_EXPORT_INT extern "C"
#endif

// C/C++ struct management (+ packing)
#ifdef __cplusplus
#ifndef STRUCT_BEGIN
#define STRUCT_BEGIN( x ) struct x
#endif  // STRUCT_BEGIN
#ifndef STRUCT_END
#define STRUCT_END( x )
#endif  // STRUCT_END

#if defined( ATR_MSVC ) || defined( ATR_ICC_WIN )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) __pragma( pack( push, 1 ) ) struct x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) \
    ;                         \
    __pragma( pack( pop ) )
#endif  // PACK1_STRUCT_END
#elif defined( ATR_GCC ) || defined( ATR_ICC_LIN )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) _Pragma( "pack(push, 1)" ) struct x
#endif
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) \
    ;                         \
    _Pragma( "pack(pop)" )
#endif
#elif defined( __arm__ )
#define PACK1_STRUCT_BEGIN( x ) struct x
#define PACK1_STRUCT_END( x ) __attribute__( ( packed ) )
#else
#error "No way to guess which is the packing syntax of this compiler."
#endif
#else
#define STRUCT_BEGIN( x ) struct _##x
#define STRUCT_END( x ) \
    ;                   \
    typedef struct _##x x

#if defined( ATR_MSVC ) || defined( ATR_ICC_WIN )
#define PACK1_STRUCT_BEGIN( x ) __pragma( "push, 1)" ) struct _##x
#define PACK1_STRUCT_END( x ) \
    ;                         \
    __pragma( "pack(pop)" ) typedef struct _##x x
#elif defined( ATR_GCC ) || defined( ATR_ICC_LIN )
#define PACK1_STRUCT_BEGIN( x ) _Pragma( "pack(push, 1)" ) struct _##x
#define PACK1_STRUCT_END( x ) \
    ;                         \
    _Pragma( "pack(pop)" )
typedef _##x x
#elif define( __arm__ )
#define PACK1_STRUCT_BEGIN( x ) struct x
#define PACK1_STRUCT_END( x )    \
    __attribute__( ( packed ) ); \
    typedef _##x x
#else
#error "No way to guess which is the packing syntax of this compiler."
#endif
#endif

#ifdef __cplusplus
#ifndef TYPED_ENUM
/// Enumerator with basic type specification
#define TYPED_ENUM( _type, _name ) enum class _name : _type
#endif  // TYPED_ENUM
#else
#ifndef TYPED_ENUM
/// Enumerator with basic type specification
#define TYPED_ENUM( _type, _name ) \
    typedef _type _name;           \
    enum
#endif  // TYPED_ENUM
#endif

/**
 * \}
 */
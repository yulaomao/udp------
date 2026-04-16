// ===========================================================================

/*!
 *   This file is part of the ATRACSYS fusiontrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
 *  WRITTEN PERMISSION OF ATRACSYS.
 *
 *  \file     ftkPlatform.h
 *  \brief    Automatically detect compiler and platform.
 *
 */
// ===========================================================================

#ifndef ftk_ftkPlatform_h
#define ftk_ftkPlatform_h

// Define compiler

#if defined( _MSC_VER )
/// Compiler is Microsoft Visual C++
#ifndef ATR_MSVC
#define ATR_MSVC 1
#endif  // ATR_MSVC
        /// Microsoft Visual C++ version
#ifndef ATR_MSVC_VER
#define ATR_MSVC_VER _MSC_VER
#endif  // ATR_MSVC_VER
#endif

#if defined( __GNUC__ )
/// Compiler is GNU GCC
#ifndef ATR_GCC
#define ATR_GCC 1
#endif  // ATR_GCC
        /// GNU GCC Version
#ifndef ATR_GCC_VER
#define ATR_GCC_VER __GNUC__
#endif  // ATR_GCC_VER
#endif

#if defined( __clang__ )
/// Compiler is clang
#ifndef ATR_CLANG
#define ATR_CLANG 1
#endif  // ATR_GCC
/// clang Version
#ifndef ATR_CLANG_VER
#define ATR_CLANG_VER __clang_version__
#endif  // ATR_GCC_VER
#endif

#if defined( __ICC ) || defined( __INTEL_COMPILER )
/// Compiler is Intel CC
#ifndef ATR_ICC
#define ATR_ICC 1
#endif  // ATR_ICC
#endif

#if defined( __BORLANDC__ ) || defined( __BCPLUSPLUS__ )
/// Compiler is Borland C++ Builder
#ifndef ATR_BORLAND
#define ATR_BORLAND 1
#endif ATR_BORLAND
#endif

#if defined( __MINGW32__ )
/// Compiler is Mingw
#ifndef ATR_MINGWIN
#define ATR_MINGWIN 1
#endif  // ATR_MINGWIN
#endif

#if defined( __CYGWIN32__ )
/// Compiler is Cygwin
#ifndef ATR_CYGWIN
#define ATR_CYGWIN 1
#endif  // ATR_CYGWIN
#endif

#if defined( _CVI_ )
/// Compiler is Labwindows CVI
#ifndef ATR_CVI
#define ATR_CVI 1
#endif ATR_CVI
#endif

// ----------------------
// Software target macros
// ----------------------

#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )
/// Windows 32 bits architecture
#ifndef ATR_WIN32
#define ATR_WIN32 1
#endif  // ATR_WIN32
#endif

#if defined( WIN64 ) || defined( _WIN64 ) || defined( __WIN64__ )
/// Windows 64 bits architecture
#ifndef ATR_WIN64
#define ATR_WIN64 1
#endif  // ATR_WIN64
#endif

#if defined( ATR_WIN32 ) || defined( ATR_WIN64 )
/// Target is windows
#ifndef ATR_WIN
#define ATR_WIN
#endif  // ATR_WIN
#ifndef _CDECL_
#define _CDECL_ __cdecl
#endif  // _CDECL_
#elif defined( __APPLE__ ) && defined( __MACH__ )
/// Target is OSX
#ifndef ATR_OSX
#define ATR_OSX
#endif  // ATR_OSX
#ifndef _CDECL_
#define _CDECL_
#endif  // _CDECL_
#elif defined( __linux__ )
/// Target is GNU/linux
#ifndef ATR_LIN
#define ATR_LIN
#endif  // ATR_LIN
#ifndef _CDECL_
#define _CDECL_
#endif  // _CDECL_
#else
#error "Unknown operating system"
#endif

#if defined( _M_IX86 ) || defined( __i386__ )
/// X86 compatible processor
#ifndef ATR_X86
#define ATR_X86
#endif  // ATR_X86
#endif

#if defined( _M_X64 ) || defined( _M_AMD64 ) || defined( __x86_64 )
/// 64bits processor
#ifndef ATR_X64
#define ATR_X64
#endif  // ATR_X64
#endif

// C function export.

#ifdef __cplusplus
#if defined( ATR_WIN32 )
/// External prefix for DLL exportation
#ifndef ATR_EXPORT
#define ATR_EXPORT extern "C" __declspec( dllexport )
#endif  // ATR_EXPORT
        /// Internal prefix for DLL exportation
#ifndef ATR_EXPORT_INT
#define ATR_EXPORT_INT extern "C"
#endif  // ATR_EXPORT_INT
#else
/// External prefix for DLL exportation
#ifndef ATR_EXPORT
#define ATR_EXPORT extern "C"
#endif  // ATR_EXPORT
/// Internal prefix for DLL exportation
#ifndef ATR_EXPORT_INT
#define ATR_EXPORT_INT extern "C"
#endif
#endif  // ATR_EXPORT_INT
#else
#if defined( ATR_CVI )
/// CVI is a C compiler. Use the C declaration
#ifndef ATR_EXPORT
#define ATR_EXPORT __declspec( dllexport )
#endif  // ATR_EXPORT
/// CVI is a C compiler
#ifndef ATR_EXPORT_INT
#define ATR_EXPORT_INT
#endif  // ATR_EXPORT_INT
#else
#ifndef ATR_EXPORT
#define ATR_EXPORT
#endif  // ATR_EXPORT
#ifndef ATR_EXPORT_INT
#define ATR_EXPORT_INT
#endif  // ATR_EXPORT_INT
#endif
#endif

// Class export
#if defined( ATR_WIN )
/// Exporting class
#define ATR_CLASS_EXPORT __declspec( dllexport )
#else
#define ATR_CLASS_EXPORT
#endif

// C/C++ struct management (+ packing)
#ifdef __cplusplus
#ifndef STRUCT_BEGIN
///  Portable (C/C++) struct declaration begining
#define STRUCT_BEGIN( x ) struct x
#endif  // STRUCT_BEGIN
#ifndef STRUCT_END
///  Portable (C/C++) struct declaration ending
#define STRUCT_END( x )
#endif  // STRUCT_END

#if defined( ATR_MSVC ) || defined( ATR_ICC_WIN )
#ifndef PACK1_STRUCT_BEGIN
///  Portable (C/C++) packed struct declaration begining
#define PACK1_STRUCT_BEGIN( x ) __pragma( pack( push, 1 ) ) struct x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
///  Portable (C/C++) packed struct declaration ending
#define PACK1_STRUCT_END( x ) \
    ;                         \
    __pragma( pack( pop ) )
#endif  // PACK1_STRUCT_END
#ifndef PACK1_STRUCT_BEGIN_DEPRECATED
///  Portable (C/C++) packed struct deprecated declaration begining
#if _MSC_VER >= 1912
#define PACK1_STRUCT_BEGIN_DEPRECATED( x ) __declspec( deprecated ) __pragma( pack( push, 1 ) ) struct x
#else
#define PACK1_STRUCT_BEGIN_DEPRECATED( x ) \
    __pragma( message( "Deprecated structure" ) ) __pragma( pack( push, 1 ) ) struct x
#endif
#endif  // PACK1_STRUCT_BEGIN_DEPRECATED
#ifndef PACK1_STRUCT_END_DEPRECATED
///  Portable (C/C++) packed struct deprecated declaration ending
#define PACK1_STRUCT_END_DEPRECATED( x ) \
    ;                                    \
    __pragma( pack( pop ) )
#endif  // PACK1_STRUCT_END_DEPRECATED
#elif defined( ATR_GCC ) || defined( ATR_ICC_LIN ) || defined( ATR_CLANG )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) _Pragma( "pack(push, 1)" ) struct x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) \
    ;                         \
    _Pragma( "pack(pop)" )
#endif  // PACK1_STRUCT_END
#ifndef PACK1_STRUCT_BEGIN_DEPRECATED
#define PACK1_STRUCT_BEGIN_DEPRECATED( x ) _Pragma( "pack(push, 1)" ) struct x
#endif  // PACK1_STRUCT_BEGIN_DEPRECATED
#ifndef PACK1_STRUCT_END_DEPRECATED
#define PACK1_STRUCT_END_DEPRECATED( x ) \
    __attribute__( ( deprecated ) );     \
    _Pragma( "pack(pop)" )
#endif  // PACK1_STRUCT_END_DEPRECATED
#elif defined( __arm__ )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) struct x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) __attribute__( ( packed ) )
#endif  // PACK1_STRUCT_END
#ifndef PACK1_STRUCT_BEGIN_DEPRECATED
#define PACK1_STRUCT_BEGIN_DEPRECATED( x ) struct x
#endif  // PACK1_STRUCT_BEGIN_DEPRECATED
#ifndef PACK1_STRUCT_END_DEPRECATED
#define PACK1_STRUCT_END_DEPRECATED( x ) __attribute__( ( packed ) ) __attribute__( ( deprecated ) )
#endif  // PACK1_STRUCT_END_DEPRECATED
#else
#error "No way to guess which is the packing syntax of this compiler."
#endif
#else  // __cplusplus

#define STRUCT_BEGIN( x ) struct _##x
#define STRUCT_END( x ) \
    ;                   \
    typedef struct _##x x

#if defined( ATR_MSVC ) || defined( ATR_ICC_WIN )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) __pragma( "push, 1)" ) struct _##x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) \
    ;                         \
    __pragma( "pack(pop)" ) typedef struct _##x x
#endif  // PACK1_STRUCT_END
#elif defined( ATR_GCC ) || defined( ATR_ICC_LIN )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) _Pragma( "pack(push, 1)" ) struct _##x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x ) \
    ;                         \
    _Pragma( "pack(pop)" ) typedef struct _##x x
#endif  // PACK1_STRUCT_END
#ifndef PACK1_STRUCT_BEGIN_DEPRECATED
#define PACK1_STRUCT_BEGIN_DEPRECATED( x ) _Pragma( "pack(push, 1)" ) struct x
#endif  // PACK1_STRUCT_BEGIN_DEPRECATED
#ifndef PACK1_STRUCT_END_DEPRECATED
#define PACK1_STRUCT_END_DEPRECATED( x ) \
    __attribute__( ( deprecated ) );     \
    _Pragma( "pack(pop)" )
#endif  // PACK1_STRUCT_END_DEPRECATED
#elif defined( __arm__ )
#ifndef PACK1_STRUCT_BEGIN
#define PACK1_STRUCT_BEGIN( x ) struct x
#endif  // PACK1_STRUCT_BEGIN
#ifndef PACK1_STRUCT_END
#define PACK1_STRUCT_END( x )    \
    __attribute__( ( packed ) ); \
    typedef struct _##x x
#endif  // PACK1_STRUCT_END
#else
#error "No way to guess which is the packing syntax of this compiler."
#endif
#endif  // __cplusplus

#ifdef __cplusplus
/// Enumerator with basic type specification
#define TYPED_ENUM( _type, _name ) enum class _name : _type
#else  // __cplusplus
#ifndef TYPED_ENUM
#define TYPED_ENUM( _type, _name ) \
    typedef _type _name;           \
    enum
#endif  // TYPED_ENUM
#endif  // __cplusplus

#endif

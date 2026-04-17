// ===========================================================================

/*!
    This file is part of the ATRACSYS fusionTrack library.
    Copyright (C) 2003-2015 by Atracsys LLC. All rights reserved.

  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
  WRITTEN PERMISSION OF ATRACSYS.

  \file     atnetInterface.h
  \brief    Generic interface for hadware devices

*/
// ===========================================================================

#pragma once

#include "atnetErrors.h"
#include "atnetTypes.h"

#include <stddef.h>

/**
 * \addtogroup atnet
 * \{
 */

struct atnetImp;
typedef atnetImp* atnetLib;
typedef uint32 atnetIp;
typedef uint32 atnetVersion;

/** \brief Boot modes.
 */
TYPED_ENUM( uint8, atnetMode )
// clang-format off
{
    /** \brief Factory firmware (i.e. bootloader) mode.
     */
    FactoryFirmwareMode = 0u,
    /** \brief Application firmware mode.
     */
    ApplicationFirmwareMode = 1u,
    /** \brief Default mode, which corresponds to factory mode.
     */
    DefaultFactoryMode = 0xffu
};
// clang-format on

/** \brief Description of an atnet command.
 */
STRUCT_BEGIN( atnetCommandInfo )
{
    /** \brief Name of the command.
     */
    const char* Name;
    /** \brief Brief description of the command.
     */
    const char* Description;
    /** \brief Help for the command.
     */
    const char* Help;
    /** \brief Input arguments for the command.
     *
     * Syntax is \c "[name0:type0][name1:type1]...".
     */
    const char* InputArguments;
    /** \brief Output arguments for the command.
     *
     * Syntax is \c "[name0:type0][name1:type1]...".
     */
    const char* OutputArguments;
}
STRUCT_END( atnetCommandInfo );

// -----------------

/** \brief Library initialising function.
 *
 * This function initialises the library handle, which is needed for all other function calls. The parameter
 * can be the path to a configuration file passed to the ftkInitExt function.
 *
 * \param[in] configuration path to a JSON SDK configuration file, or \c nullptr.
 * \param[in] bufferSize size of the \c errorBuffer output parameter.
 * \param[in,out] errorBuffer allocated buffer in which the error messages (if any) will be copied.
 *
 * \return a valid library handle or \c nullptr if an error occurred.
 */
ATR_EXPORT atnetLib atnetInit( const char* configuration, const size_t bufferSize, char* errorBuffer );

/** \brief Library closing function.
 *
 * This function closes the library handle. Any subsequenc call to other library function will result in
 * atnetError::ErrInvPointer status.
 *
 * \param[in,out] lib pointer on a library handle, which will be closed and set to \c nullptr.
 *
 * \retval atnetError::Ok if the library closing could be successrully performed,
 * \retval atnetError::ErrInvPointer if lib is nullptr.
 */
ATR_EXPORT atnetError atnetClose( atnetLib* lib );

/** \brief Function to get the atnet library version.
 *
 * This function allows to get the atnet library version string.
 *
 * \param[in] bufferSize size of the \c buffer container.
 * \param[in,out] buffer allocated memory in which the version string will be written.
 *
 * \retval atnetError::Ok if the retrieval of the version could be successfully performed,
 * \retval atnetError::ErrInvPointer if \c bufferSize is \c 0u or \c buffer is \c nullptr,
 * \retval atnetError::ErrBufferTooSmall if the allocated buffer is too small.
 */
ATR_EXPORT atnetError atnetGetVersion( const size_t bufferSize, char* buffer );

/** \brief Function allowing to access the last error message.
 *
 * This function tries to copy the last error message in the provided buffer.
 *
 * \retval atnetError::Ok if the retrieval of the last error message could be successfully performed,
 * \retval atnetError::ErrInvPointer if \c lib or \c buffer is \c nullptr or if \c bufferSize is \c 0u,
 * \retval atnetError::ErrBufferTooSmall if the allocated buffer is too small.
 */
ATR_EXPORT atnetError atnetGetError( atnetLib lib, const size_t bufferSize, char* buffer );

/** \brief Definition of the callback function for the atnetListCommands function.
 *
 * \param[in] info information about the enumerated command.
 * \param[in,out] user user data.
 */
typedef void commandEnumerateCallback( const atnetCommandInfo* info, void* user );

/** \brief Function listing all accessible commands.
 *
 * This function walks through the list of available commands. For each of them, the \c func callback is
 * called.
 *
 * \param[in] lib initialised library handle.
 * \param[in] func callback function, called for each available command.
 * \param[in,out] user pointer on user-data, simply forwarded to the \c func function.
 *
 * \retval atnetError::Ok if the commands could be successfully enumerated,
 * \retval atnetError::ErrInvPointer if \c lib is \c nullptr.
 */
ATR_EXPORT atnetError atnetListCommands( atnetLib lib, commandEnumerateCallback func, void* user );

/** \brief Definition of the callback function for the atnetExecuteCommand function.
 *
 * \param[in] message output message of the command.
 * \param[in,out] user user data.
 */
typedef void processOutputCallback( const char* message, void* user );

/** \brief Definition of the callback function for the atnetExecuteCommand function.
 *
 * This callback signature is used if the use of the atnetExecuteCommand function want to be notified of the
 * progress of some lenghty commands.
 *
 * The first call to this callback will have as parameters the message, \c -1 as the old percentage and \c 0
 * as the new one. The \e last call (not depending whether the counter reached 100% or not) will have \c 0 as
 * old value and \c -1 as the new one.
 *
 * \param[in] message message of the command (for command using this feature).
 * \param[in] oldPercentage old value of the progress (in percent) of the command (for command using this
 * feature).
 * \param[in] newPercentage new value of the progress (in percent) of the command (for command using this
 * feature).
 * \param[in,out] user pointer on user data.
 */
typedef void processProgressOutputCallback( const char* message,
                                            int oldPercentage,
                                            int newPercentage,
                                            void* user );

/** \brief Function trying to execute the wanted function.
 *
 * This function tries to extract the wanted function from the \c args arguments:
 *  - the function name is extracted and checked to exist;
 *  - the arguments given to the function are extracted;
 *  - the arguments are checked to be compatible with the wanted function;
 *  - the function is executed.
 *
 * \param[in] lib initialised library handle.
 * \param[in] args arguments string provided to the function.
 * \param[in] func callback function, called for each output of the command.
 * \param[in,out] userOutput pointer on user-data, simply forwarded to the \c func function.
 * \param[in] progress callback function, called for each iteration of some complex commands.
 * \param[in,out] userProgress pointer on user-data, simply forwarded to the \c progress function.
 *
 * \retval atnetError::Ok if the command could be successfully executed,
 * \retval atnetError::ErrInvPointer if \c lib or \c func is \c nullptr,
 * \retval atnetError::ErrEmptyCommand if no commands are provided,
 * \retval atnetError::ErrUnknownCommand if the provided command does not exist,
 * \retval atnetError::ErrArgumentQuote if the provided command is ill-formed because of mismatching number
 * of single or double quotes for instance,
 * \retval atnetError::ErrInputArgument if any of the input arguments was given a n invalid value or if a
 * mandatory argument was not provided,
 * \retval atnetError::ErrCannotProcessCommand if the internal data could not be written before trying to
 * send the command to the fusionTrack,
 * \retval atnetError::ErrRunningCommand if the command could not be sent to the fusionTrack (some pointer
 * invalid, could not determine if several calls are needed, etc. or if the command execution on the
 * fusionTrack returned an error,
 * \retval atnetError::ErrCannotExtractOutput if the extraction of the command output could not be performed,
 * \retval atnetError::ErrInternal if an allocation error occurred,
 * \retval atnetError::WarAlreadyProcessingCommand if a command is already being executed.
 */
ATR_EXPORT atnetError atnetExecuteCommand( atnetLib lib,
                                           const char* args,
                                           processOutputCallback func,
                                           void* userOutput,
                                           processProgressOutputCallback progress,
                                           void* userProgress );

/**
 * \}
 */

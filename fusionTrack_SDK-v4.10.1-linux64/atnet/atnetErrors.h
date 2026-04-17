// ===========================================================================

/*!
    This file is part of the ATRACSYS fusiontrack library.
    Copyright (C) 2003-2015 by Atracsys LLC. All rights reserved.

  THIS FILE CANNOT BE SHARED, MODIFIED OR REDISTRIBUTED WITHOUT THE
  WRITTEN PERMISSION OF ATRACSYS.

  \file     atnetErrors.h
  \brief    Device error codes

*/
// ===========================================================================

#pragma once

#include "atnetTypes.h"

/**
 * \addtogroup atnet
 * \{
 */

/** \brief Status codes for the atnet library.
 */
TYPED_ENUM( int32, atnetError )
// clang-format off
{
    /** \brief A command is already being processed.
     */
    WarAlreadyProcessingCommand = -10,
    /** \brief No errors.
     */
    Ok = 0,
    /** \brief Provided pointer is \c nullptr.
     */
    ErrInvPointer = 1,
    /** \brief Provided serial number does not belong to the list of enumerated devices.
     */
    ErrInvSerial = 2,
    /** \brief The provided command is empty.
     */
    ErrEmptyCommand = 4,
    /** \brief The provided command does not exist.
     */
    ErrUnknownCommand = 5,
    /** \brief Internal error, should not happen.
     */
    ErrInternal = 6,
    /** \brief A provided argument was ill-formed (bad quoting).
     *
     * An error in the quotation marks was detected. For instance a missing \c " or a missing \c ' was
     * detected.
     */
    ErrArgumentQuote = 10,
    /** \brief Error when handling the input arguments.
     *
     * Either a mandatory argument was found missing or its value does not correspond to the argument type.
     */
    ErrInputArgument = 11,
    /** \brief The command could not be prepared.
     *
     * This is for instance issued if an error occurred when preparing the UDP packet.
     */
    ErrCannotProcessCommand = 20,
    /** \brief The command could not be run.
     *
     * This error can be caused by the packet not being sent, or the answer not received or an error was
     * reported by the fusionTrack.
     */
    ErrRunningCommand = 21,
    /** \brief The command could be run but the output could not be read.
     *
     * This error occurs when the command is successfully sent to the fusionTrack, successfully processed by
     * it, but output arguemnts couldn't be extracted from the received packet.
     */
    ErrCannotExtractOutput = 22,
    /** \brief The provided buffer is too small.
     */
    ErrBufferTooSmall = 30
};
// clang-format on

/**
 * \}
 */

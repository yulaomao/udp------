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
    /** \brief The provided command does not exist.
     */
    ErrUnknownCommand = 5,
    /** \brief Internal error, should not happen.
     */
    ErrInternal = 6,
    /** \brief The command could not be processed.
     *
     * This is for instance issued if the provided arguments are not corresponding to the expected ones, or
     * if an error occurred when preparing the UDP packet.
     */
    ErrCannotProcessCommand = 10,
    /** \brief The command could not be run.
     *
     * This error can be caused by the packet not being sent, or the answer not received or an error was
     * reported by the fusionTrack.
     */
    ErrNoRunCommand = 11,
    /** \brief The provided buffer is too small.
     */
    ErrBufferTooSmall = 20
};
// clang-format on

/**
 * \}
 */

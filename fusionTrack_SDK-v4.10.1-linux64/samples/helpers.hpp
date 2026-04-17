// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *   \file helpers.hpp
 *   \brief Helping functions used by sample applications
 *
 */
// ============================================================================

#pragma once

#include <ftkInterface.h>

#include <iostream>
#include <string>

/** \addtogroup Platform dependent functions
 * \{
 */

/** \brief is the software having its own console?
 *
 * This function is used to know if the program was launch from an explorer.
 * It needs to be called in main() before printing to stdout
 * See http://support.microsoft.com/kb/99115
 *
 * \retval true if program is in its own console (cursor at 0,0),
 * \retval false if it was launched from an existing console.
 */
bool isLaunchedFromExplorer();

/** \brief Function waiting for a keyboard hit.
 *
 * This function allows to detect a hit on (almost) any key. Known non-detected
 * keys are:
 *  - shift;
 *  - Caps lock;
 *  - Ctrl;
 *  - Alt;
 *  - Alt Gr.
 * The function blocks and only returns when a `valid' key is stroke.
 */
void waitForKeyboardHit();

/** \brief Function pausing the current execution process / thread.
 *
 * This function stops the current execution thread / process for at least
 * the given amount of time.
 *
 * \param[in] ms amount of time to wait, in milliseconds.
 */
void sleep( long ms );

/**
 * \}
 */

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

/** \brief Function displaying error messages on the standard error output.
*
* This function displays an error message and exits the current process. The
* value returned by the program is 1. On windows, the user is asked to press
* a key to exit.
*
* \param[in] message error message to print.
* \param[in] dontWaitForKeyboard, setting to true to avoid being prompted before exiting program
*/
void error( const char* message, bool dontWaitForKeyboard = false );

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

/** \brief Helper struct holding a device serial number and its type.
 */
struct DeviceData
{
    uint64 SerialNumber;
    ftkDeviceType Type;
};

/** \brief Callback function for devices.
*
* This function assigns the serial number to the \c user argument, so that the
* device serial number is retrieved.
*
* \param[in] sn serial number of the discovered device.
* \param[out] user pointer on a DeviceData instance where the information will
* be copied.
*/
void deviceEnumerator( uint64 sn, void* user, ftkDeviceType type );

/** \brief Callback function for fusionTrack devices.
*
* This function assigns the serial number to the \c user argument, so that the
* device serial number is retrieved. If the enumerated device is a simulator,
* the device is considered as not detected.
*
* \param[in] sn serial number of the discovered device.
* \param[out] user pointer on the location where the serial will be copied.
* \param[in] devType type of the device.
*/
void fusionTrackEnumerator( uint64 sn, void* user, ftkDeviceType devType );

/** \brief Function exiting the program after displaying the error.
*
* This function retrieves the last error and displays the corresponding
* string in the terminal.
*
* The typical usage is:
* \code
* ftkError err( ... );
* if ( err != FTK_OK )
* {
*     checkError( lib );
* }
* \endcode
*
* \param[in] lib library handle.
*/
void checkError( ftkLibrary lib, bool dontWaitForKeyboard = false, bool quit = true );

/** \brief Function enumerating the devices and keeping the last one.
*
* This function uses the ftkEnumerateDevices library function and the
* deviceEnumerator callback so that the last discovered device is kept.
*
* If no device is discovered, the execution is stopped by a call to error();
*
* \param[in] lib initialised library handle.
* \param[in] allowSimulator setting to \c false requires to discover only real
* \param[in] quiet setting to \c true to disactivate printouts
* \param[in] dontWaitForKeyboard, setting to true to avoid being prompted before exiting program
*
* devices (i.e. the simulator device is not retrieved).
*
* \return the serial number of the discovered device.
*/
DeviceData retrieveLastDevice( ftkLibrary lib,
                               bool allowSimulator = true,
                               bool quiet = false,
                               bool dontWaitForKeyboard = false );

void optionEnumerator( uint64 sn, void* user, ftkOptionsInfo* oi );


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

/** \brief Helper class reading the ftkError string.
 *
 * This class provides an API to interpret the error string gotten from
 * ftkGetLastErrorString.
 *
 * \code
 * char tmp[ 1024u ];
 * if ( ftkGetLastErrorString( lib, 1024u, tmp ) == FTK_OK )
 * {
 *     ErrorReader reader;
 *     if ( ! reader.parse( tmp ) )
 *     {
 *         cerr << "Cannot interpret received error:" << endl << tmp << endl;
 *     }
 *     else
 *     {
 *         if ( reader.hasError( FTK_ERR_INTERNAL ) )
 *         {
 *             cout << "Internal error" << endl;
 *         }
 *         // ...
 *     }
 * }
 * \endcode
 */
class ErrorReader
{
public:
    /** \brief Default constructor.
     */
    ErrorReader();

    /** \brief Destructor, does nothing fancy.
     */
    ~ErrorReader();

    /** \brief Parsing method.
     *
     * This method parses the error string. It is not a XML parser, as the
     * error syntax is very simple. It extracts the errors, warnings and other
     * messages from the provided string.
     *
     * Syntax errors are reported on std::cerr.
     *
     * \param[in] str string to be parsed.
     *
     * \retval true if the parsing could be done successfully,
     * \retval false if an error occurred,
     */
    bool parseErrorString( const std::string& str );

    void display( std::ostream& os = std::cout ) const;

    /** \brief Getter for a given error.
     *
     * This method checks whether is the given error was flagged.
     *
     * \param[in] err error code to be checked.
     *
     * \retval true if the given error was flagged in the XML string,
     * \retval false if \c err is a warning, if no errors were flagged or if
     * \c err is not found in the flagged errors.
     */
    bool hasError( ftkError err ) const;

    /** \brief Getter for a given warning.
     *
     * This method checks whether is the given warning was flagged.
     *
     * \param[in] err warning code to be checked.
     *
     * \retval true if the given warning was flagged in the XML string,
     * \retval false if \c war is an error, if no warning were flagged or if
     * \c war is not found in the flagged errors.
     */
    bool hasWarning( ftkError war ) const;

    /** \brief Getter for OK status.
     *
     * This method checks if no errors or warnings are flagged.
     *
     * \retval true if no errors and no warnings are flagged,
     * \retval false if at least one error or one warning was flagged.
     */
    bool isOk() const;

    /** \brief Getter for any error status.
     *
     * This methos checks if the error string describes an error.
     *
     * \retval true if any error was flagged,
     * \retval false if no errors were flagged.
     */
    bool isError() const;

    /** \brief Getter for any warning status.
     *
     * This methos checks if the error string describes an warning.
     *
     * \retval true if any warning was flagged,
     * \retval false if no warnings were flagged.
     */
    bool isWarning() const;

private:
    std::string _ErrorString;
    std::string _WarningString;
    std::string _StackMessage;
};



// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

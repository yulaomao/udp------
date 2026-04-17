/** \file TrackingSystemAbstract.hpp
 * \brief Definition of many helper functions and structures for the C++ API.
 *
 * This file gathers the the (non frame-related) data structures and functions of the C++ API.
 */
#pragma once

#include "AdditionalDataStructures.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * \addtogroup adApi SDK C++ API
 * \{
 */
namespace atracsys
{
    class TrackingSystem;
    class TrackingSystemAbstract;
    class TrackingSystemPrivate;

    /** \brief Shorthand for the type used for the timestamp.
     */
    using timestamp_t =
      std::chrono::time_point< std::chrono::system_clock, std::chrono::duration< int64_t > >;

    /** \brief Status codes.
     *
     * Those status codes are used as returned type by the various function and
     * class methods of the C++ API.
     */
    enum class Status : uint32_t
    {
        /** \brief No errors or warnings.
         */
        Ok = 0,

        /** \brief The underlying library handle was not / could not be
         * initialised properly.
         */
        LibNotInitialised,

        /** \brief No detected devices.
         */
        NoDevices,

        /** \brief Too many connected devices for a `short-call' method, i.e.
         * without serial number argument.
         */
        SeveralDevices,

        /** \brief The given serial number is not among the enumerated devices.
         */
        UnknownDevice,

        /** \brief The given option does not exist.
         */
        InvalidOption,

        /** \brief The input container size does not match the number of
         * detected devices.
         */
        UnmatchingSizes,

        /** \brief Allocation of a low-level object failed or attempt to access
         * a low-level unallocated object.
         */
        AllocationIssue,

        /** \brief A warning was issued by the SDK.
         *
         * More detailed information can be retreived from the internal status,
         * using TrackingSystemAbstract::getLastError / TrackingSystemAbstract::streamLastError
         * method.
         */
        SdkWarning,

        /** \brief An error was issued by the SDK.
         *
         * More detailed information can be retreived from the internal status,
         * using TrackingSystemAbstract::getLastError / TrackingSystemAbstract::streamLastError
         * method.
         */
        SdkError,

        /** \brief The given input file does not exist.
         */
        InvalidFile,

        /** \brief The given input file could not be parsed.
         *
         * This is only valid for JSON or INI files.
         */
        ParsingError,

        /** \brief Internal handling of ::ftkBuffer error.
         *
         * This error indicates the data stored in ::ftkBuffer has not the
         * expected size.
         */
        BufferSize
    };

    /** \brief Log levels.
     *
     * This enum is used to determine the verbosity of the logs.
     */
    enum class LogLevel : uint32_t
    {
        /** \brief All logs shown.
         */
        All,

        /** \brief Verbose output.
         */
        Verbose,

        /** \brief Debug output.
         */
        Debug,

        /** \brief Normal output.
         */
        Info,

        /** \brief Warning output.
         */
        Warning,

        /** \brief Error output.
         */
        Error,

        /** \brief No logs shown.
         */
        None
    };

    /** \brief Function waiting for a user input on (almost) any key.
     *
     * This function polls the keyboard each 200 ms and blocks until a key is
     * hit. The Ctrl, Alt, Shift keys are not detected, the windows key
     * on windows in intercept by the OS. The pressed character is \e consumed
     * by the function, i.e. it won't be displayed.
     */
    void waitForKeyboardHit();

    /** \brief Error reporting function.
     *
     * This function prints the sent message to \c std::cerr and aborts the
     * program.
     *
     * \param[in] message message to print.
     */
    void reportError( const std::string& message );

    /** \brief Function asking the user to hit a key.
     *
     * This function prints a message (which default is \c "Press the 'ANY'
     * key to what" and wait for a keyboard hit.
     *
     * \param[in] what last word printed in the message.
     */
    void continueOnUserInput( const std::string& what = "exit" );

    /** \brief Function loading a geometry from a file.
     *
     * This function tries to load a ftkGeometry object from a full file path.
     *
     * \deprecated Please use TrackingSystemAbstract::loadGeometry instead.
     *
     * \param[in] fileName name of the file to load.
     * \param[out] geom loaded geometry.
     * \param[out] out stream which will be populated by potential error
     * messages.
     *
     * \retval Status::Ok if the loading could be performed successfully,
     * \retval Status::InvalidFile if the file could not be opened (i.e. not
     * existing file or wrong file path),
     * \retval Status::ParsingError if the file could not be parsed, i.e. the
     * file is not valid INI, or if a mandatory information is not present, or
     * if a numeric value could be read from the file content.
     */
#ifdef ATR_MSVC
    __declspec( deprecated )
#endif
      Status loadGeometry( const std::string& fileName, ftkGeometry& geom, std::ostream& out = std::cerr )
#if defined( ATR_GCC ) || defined( ATR_CLANG )
        __attribute__( ( deprecated ) )
#endif
        ;

    /** \brief Description of an option.
     *
     * This is actually a mimic of the ftkOptionsInfo structure, but \c
     * std::string members are used to allow a long-term storage.
     */
    class DeviceOption
    {
    public:
        /** \brief Default implementation of default constructor.
         */
        DeviceOption() = default;

        /** \brief Standard constuctor with full initialisation of members.
         *
         * \param[in] id ID of the option.
         * \param[in] component category of the option.
         * \param[in] status status (read/write, etc.) of the option.
         * \param[in] type type (integer, floating point, binary )of the
         * option.
         * \param[in] name option name.
         * \param[in] desc option description.
         * \param[in] unit option unit (if applicable).
         */
        DeviceOption( uint32_t id,
                      ftkComponent component,
                      ftkOptionStatus status,
                      ftkOptionType type,
                      const std::string& name,
                      const std::string& desc,
                      const std::string& unit = "" );

        /** \brief Default implementation of copy-constructor.
         *
         * \param[in] other instance to duplicate.
         */
        DeviceOption( const DeviceOption& other ) = default;

        /** \brief Default implementation of move-constructor.
         *
         * \param[in] other instance to move.
         */
        DeviceOption( DeviceOption&& other ) = default;

        /** \brief Default implementation of destructor.
         */
        virtual ~DeviceOption() = default;

        /** \brief Assignment operator default implementation.
         *
         * \param[in] other instance to replicate.
         *
         * \retval *this as a reference.
         */
        DeviceOption& operator=( const DeviceOption& other ) = default;

        /** \brief Move-assignment operator default implementation.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        DeviceOption& operator=( DeviceOption&& other ) = default;

        /** \brief Comparison operator, check instance equality.
         *
         * This method allows to check if two instances are equal. All fields
         * must be the same for the method to return \c true.
         *
         * \param[in] other instance to compare with the current one.
         *
         * \retval true if all fields are the same,
         * \retval false if any of the member is different.
         */
        bool operator==( const DeviceOption& other ) const;

        /** \brief Getter for #_Id.
         *
         * \retval _Id as value.
         */
        uint32_t id() const;

        /** \brief Getter for #_Component.
         *
         * \retval _Component as value.
         */
        ftkComponent component() const;

        /** \brief Getter for #_Status.
         *
         * \retval _Status as value.
         */
        ftkOptionStatus status() const;

        /** \brief Getter for #_Type.
         *
         * \retval _Type as value.
         */
        ftkOptionType type() const;

        /** \brief Getter for #_Name.
         *
         * \retval _Name as value.
         */
        const std::string& name() const;

        /** \brief Getter for #_Description.
         *
         * \retval _Description as value.
         */
        const std::string& description() const;

        /** \brief Getter for #_Unit.
         *
         * \retval _Unit as value.
         */
        const std::string& unit() const;

    protected:
        friend class TrackingSystem;
        friend class TrackingSystemAbstract;
        friend class TrackingSystemPrivate;

        /** \brief Unique id of the option.
         */
        uint32_t _Id;

        /** \brief Driver component linked to the option.
         */
        ftkComponent _Component;

        /** \brief Option accessibility.
         */
        ftkOptionStatus _Status;

        /** \brief Type of the option.
         */
        ftkOptionType _Type;

        /** \brief Name of the option.
         */
        std::string _Name;

        /** \brief Detailed description of the option.
         */
        std::string _Description;

        /** \brief Unit of the option (if relevant).
         */
        std::string _Unit;
    };

    /** \brief Class holding the information about a device.
     *
     * This class contains the device type and serial number. The options
     * available on for this devices are also contained.
     *
     * A device is defined by its type and serial number. As different devices
     * can have different options, the available options are also contained in
     * this class.
     */
    class DeviceInfo
    {
    public:
        /** \brief Default implementation of default constructor.
         */
        DeviceInfo() = default;

        /** \brief Standard constructor.
         *
         * \param[in] sn serial number of the detected device.
         * \param[in] type type of the detected device.
         */
        DeviceInfo( uint64_t sn, ftkDeviceType type );

        /** \brief Default implementation of copy-constructor.
         *
         * \param[in] other instance to duplicate.
         */
        DeviceInfo( const DeviceInfo& other ) = default;

        /** \brief Default implementation of move-constructor.
         *
         * \param[in] other instance to move.
         */
        DeviceInfo( DeviceInfo&& other ) = default;

        /** \brief Default destructor.
         */
        virtual ~DeviceInfo() = default;

        /** \brief Assignment operator default implementation.
         *
         * \param[in] other instance to replicate.
         *
         * \retval *this as a reference.
         */
        DeviceInfo& operator=( const DeviceInfo& other ) = default;

        /** \brief Move-assignment operator default implementation.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        DeviceInfo& operator=( DeviceInfo&& other ) = default;

        /** \brief Comparison operator.
         *
         * This operator is needed for the python binding. It checks the
         * serial numbers and device type are the same.
         *
         * \retval true if both DeviceInfo::_SerialNumber and DeviceInfo::_Type
         * are the same,
         * \retval false if not.
         */
        bool operator==( const DeviceInfo& other ) const;

        /** \brief Getter for #_SerialNumber.
         *
         * \retval _SerialNumber as value.
         */
        uint64_t serialNumber() const;

        /** \brief Getter for #_Type.
         *
         * \retval _Type as value.
         */
        ftkDeviceType type() const;

        /** \brief Getter for #_Options.
         *
         * \retval _Options as a const reference.
         */
        const std::vector< DeviceOption >& options() const;

    protected:
        friend class TrackingSystem;
        friend class TrackingSystemAbstract;
        friend class TrackingSystemPrivate;

        /** \brief Device serial number.
         */
        uint64_t _SerialNumber;

        /** \brief Device type.
         */
        ftkDeviceType _Type;

        /** \brief Available options.
         */
        std::vector< DeviceOption > _Options;
    };

    /** \brief Abstract description of the tracking system.
     *
     * This is the base class for the tracking system handling class. It
     * provides the core functionalities and defines the interface.
     *
     * Unless specified, all \c public methods of this class are thread-safe,
     * and no \c protected methods are thread-safe.
     */
    class TrackingSystemAbstract
    {
    public:
        /** \brief Default constructor, allows to discriminate a simulator.
         *
         * The constructor initialises the instance and allows an optional
         * rejection of detected simulators.
         *
         * \param[in] timeout default timeout value (in ms) when calling ::ftkGetLastFrame.
         * \param[in] allowSimulator set to \e false to prevent to enumerate a simulator.
         */
        TrackingSystemAbstract( uint32_t timeout = 100u, bool allowSimulator = true );

        /** \brief Copy constructor, disabled as the class is non-copyable.
         */
        TrackingSystemAbstract( const TrackingSystemAbstract& ) = delete;

        /** \brief Default implementation of the move constructor.
         *
         * This constructor allows to move an existing instance.
         *
         * \param[in] other instance to move.
         */
        TrackingSystemAbstract( TrackingSystemAbstract&& other ) = default;

        /** \brief Destructor, cleans the allocated memory.
         *
         * The destructor is responsible to close the library.
         */
        virtual ~TrackingSystemAbstract();

        /** \brief Assignment operator, disabled as the class is non-copyable.
         */
        TrackingSystemAbstract& operator=( const TrackingSystemAbstract& ) = delete;

        /** \brief Default implementation of the move assignment operator.
         *
         * This operator allows to move an existing instance.
         *
         * \param[in] other instance to move.
         *
         * \return a reference on the current instance.
         */
        TrackingSystemAbstract& operator=( TrackingSystemAbstract&& other ) = default;

        /** \brief Method initialising the library.
         *
         * This method initialises the underlying library instance calling ::ftkInitExt. Calling any other
         * methods before a successful call to this one will result in an error Status::LibNotInitialised.
         *
         * It also initialises the std::regex responsible to extract the error, warning and other message
         * from the ::ftkGetLastErrorString function.
         *
         * \param[in] fileName name of the JSON file provided to ::ftkInitExt.
         * \param[out] out stream used to report the potential errors.
         *
         * \retval Status::Ok if the initialisation could be successfully performed,
         * \retval Status::LibNotInitialised in case of an error when calling ::ftkInitExt,
         * \retval Status::ParsingError if none of the tested regex dialect was able to catch multiline
         * strings.
         */
        virtual Status initialise( const std::string& fileName = "", std::ostream& out = std::cerr );

        /** \brief Getter for the SDK version.
         *
         * This function allows to get the version of the SDK. It internally calls ::ftkVersion.
         *
         * \param[out] version the \e full version string (i.e. version and build timestamp).
         *
         * \retval true if the version could vbe successfully read,
         * \retval false if the library handle was not initialised properly.
         */
        bool sdkVersion( std::string& version ) const;

        /** \brief Getter for #_DefaultTimeout.
         *
         * \retval _DefaultTimeout as value.
         */
        uint32_t defaultTimeout() const;

        /** \brief Setter for #_DefaultTimeout.
         *
         * \param[in] value new value for #_DefaultTimeout.
         */
        void setDefaultTimeout( uint32_t value );

        /** \brief Getter for #_AllowSimulator.
         *
         * \retval _AllowSimulator as value.
         */
        bool allowSimulator() const;

        /** \brief Setter for #_AllowSimulator.
         *
         * \param[in] value new value for #_AllowSimulator.
         */
        void setAllowSimulator( bool value );

        /** \brief Getter for #_Level.
         *
         * \retval _Level as value.
         */
        LogLevel level() const;

        /** \brief Setter for #_Level.
         *
         * \param[in] value new value for #_Level.
         */
        void setLevel( LogLevel value );

        /** \brief Method loading a ftkRigidBody instance from the given file name.
         *
         * This method tries to open the file from the given file name and loads it in a ::ftkBuffer. The
         * ::ftkLoadRigidBodyFromFile function is then called to read the file.
         *
         * \param[in] fileName name of the file to read.
         * \param[out] geom loaded geometry.
         *
         * \retval Status::Ok if the ftkRigidBody instance could be successfully loaded,
         * \retval Status::LibNotInitialised is the library handle was not correctly initialised,
         * \retval Status::InvalidFile if the file cannot be opened or read, or if the size exceeds the
         * size of the data contained in #ftkBuffer,
         * \retval Status::AllocationIssue if the temporary #ftkBuffer instance cannot be allocated,
         * \retval Status::SdkWarning if the call to #ftkLoadRigidBodyFromFile returned a warning,
         * \retval Status::SdkError if the call to #ftkLoadRigidBodyFromFile returned an error.
         */
        Status loadGeometry( const std::string& fileName, ftkRigidBody& geom );

        /** \brief Method saving a ftkRigidBody instance to the given file name.
         *
         * This method tries to open the file from the given file name and writes the geometry in a
         * ::ftkBuffer. The ::ftkSaveRigidBodyToFile function is then called to write the file.
         *
         * \param[in] fileName name of the file to write.
         * \param[in] geom geometry to save.
         *
         * \retval Status::Ok if the ftkRigidBody instance could be successfully loaded,
         * \retval Status::LibNotInitialised is the library handle was not correctly initialised,
         * \retval Status::InvalidFile if the file cannot be opened or read, or if the size exceeds the
         * size of the data contained in #ftkBuffer,
         * \retval Status::AllocationIssue if the temporary ::ftkBuffer instance cannot be allocated,
         * \retval Status::SdkWarning if the call to ::ftkLoadRigidBodyFromFile returned a warning,
         * \retval Status::SdkError if the call to ::ftkLoadRigidBodyFromFile returned an error.
         */
        Status saveGeometry( const std::string& fileName, const ftkRigidBody& geom );

        /** \brief Method converting a geometry file.
         *
         * This method tries to load the geometry from the given input file, then calls the
         * ::ftkGeometryFileConversion and finally writes the converted content in the output file.
         *
         * \param[in] inputFileName name of the file to read.
         * \param[in] outputFileName name of the file to write.
         *
         * \retval Status::Ok if the file could be successfully converted,
         * \retval Status::LibNotInitialised is the library handle was not correctly initialised,
         * \retval Status::InvalidFile if the input file cannot be opened or read, or if the out file cannot
         * be opened for writing,
         * \retval Status::AllocationIssue if any of the temporary ::ftkBuffer instances cannot be allocated,
         * \retval Status::SdkWarning if the call to ::ftkGeometryFileConversion returned a warning,
         * \retval Status::SdkError if the call to ::ftkGeometryFileConversion returned an error.
         */
        Status convertGeometryFile( const std::string& inputFileName, const std::string& outputFileName );

        /** \brief Method creating a frame for \e the detected device.
         *
         * This method creates a ::ftkFrameQuery instance by calling ::ftkCreateFrame followed by a call to
         * ::ftkSetFrameOptions. The created instance is kept in a bookkeeping so that:
         *  - the frame is internally used when calling #getLastFrame;
         *  - the frames are automatically deleted when the TrackingSystem instance is destroyed.
         *
         * A double call to this function will \e not create two instances, but it allows to change the
         * settings (e.g. increasing the number of retrieved 3D fiducials or disabling the picture
         * retrieval).
         *
         * \param[in] pixels \c true if the pixels must be retrieved.
         * \param[in] eventCount maximum number of events retrieved in a frame.
         * \param[in] rawDataCount maximum number of raw data retrieved in a frame.
         * \param[in] fiducialsCount maximum number of fiducials retrieved in a frame.
         * \param[in] markerCount maximum number of markers retrieved in a frame.
         *
         * \retval Status::Ok if the frame could be properly allocated,
         * \retval Status::NoDevices if no devices were detected,
         * \retval Status::SeveralDevices if more than 1 device were detected,
         * \retval Status::AllocationIssue if the call to ::ftkCreateFrame failed,
         * \retval Status::SdkWarning if the call to ::ftkSetFrameOptions returned a warning,
         * \retval Status::SdkError if the call to ::ftkSetFrameOptions returned an error.
         */
        Status createFrame( bool pixels = false,
                            uint32_t eventCount = 0u,
                            uint32_t rawDataCount = 0u,
                            uint32_t fiducialsCount = 0u,
                            uint32_t markerCount = 0u );

        /** \brief Method creating a frame per detected devices.
         *
         * This method creates a ::ftkFrameQuery instance for each detected devices by calling #createFrame
         * once per detected device. If a failure occurs for only one of the detected devices, \e all frame
         * instances are destroyed.
         *
         * A double call to this function will \e not create two instances per connected device, but it
         * allows to change the settings (e.g. increasing the number of retrieved 3D fiducials or disabling
         * the picture retrieving).
         *
         * \param[in] pixels \c true if the pixels must be retrieved.
         * \param[in] eventCount maximum number of events retrieved in a frame.
         * \param[in] rawDataCount maximum number of raw data retrieved in a frame.
         * \param[in] fiducialCount maximum number of fiducials retrieved in a frame.
         * \param[in] markerCount maximum number of markers retrieved in a frame.
         *
         * \retval Status::Ok if the frame could be properly allocated,
         * \retval Status::NoDevices if no devices were detected,
         * \retval Status::AllocationIssue if the call to ::ftkCreateFrame failed,
         * \retval Status::SdkWarning if the call to ::ftkSetFrameOptions returned a warning,
         * \retval Status::SdkError if the call to ::ftkSetFrameOptions returned an error.
         */
        Status createFrames( bool pixels = false,
                             uint32_t eventCount = 0u,
                             uint32_t rawDataCount = 0u,
                             uint32_t fiducialCount = 0u,
                             uint32_t markerCount = 0u );

        /** \brief Error getting method.
         *
         * This method calls ::ftkGetLastErrorString to get the last SDK error detailed information. The
         * output is parsed using regular expressions.
         *
         * \param[out] messages container for the gotten messages, keys are \c "errors", \c "warnings" or
         * \c "stack".
         *
         * \retval Status::Ok if retrieving and parsing the error message could be successfully performed,
         * \retval Status::SdkError if the call to ::ftkGetLastErrorString function failed,
         * \retval Status::ParsingError if the parsing of the string returned by ::ftkGetLastErrorString
         * failed.
         */
        Status getLastError( std::map< std::string, std::string >& messages ) const;

        /** \brief Error streaming method.
         *
         * This method calls TrackingSystemAbstract::getLastError to get the error, warning and general
         * messages, which are then streamed in the argument stream, only if something is reported (i.e.
         * \c "No error" won't be written).
         *
         * \param[out] out stream in which the messages will be written.
         *
         * \retval Status::Ok if retrieving and parsing the error message could be successfully performed,
         * \retval Status::SdkError if the call to ::ftkGetLastErrorString function failed,
         * \retval Status::ParsingError if the parsing of the string returned by ::ftkGetLastErrorString
         * failed.
         */
        virtual Status streamLastError( std::ostream& out = std::cerr ) const;

        /** \brief Frame getting method for a specific device.
         *
         * This method gets the last frame for the device with the given serial number. It internally calls
         * ::ftkGetLastFrame with the given parameters.
         *
         * A call to createFrame() or createFrames() must have been successfully performed, otherwise the
         * Status::AllocationIssue status is returned. The actual call to the C interface is delegated to the
         * #_getLastFrame method.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] timeout timeout (in ms) for the call to ::ftkGetLastFrame.
         * \param[out] frame instance in which the retrieved data will be stored.
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame),
         * \retval Status::SdkWarning if the call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrameForDevice( uint64_t serialNbr, uint32_t timeout, FrameData& frame ) const;

        /** \overload Status getLastFrameForDevice(uint64_t, FrameData&) const
         *
         * This method uses the instance default timeout when calling #getLastFrame.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[out] frame instance in which the retrieved data will be stored.
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if the call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrameForDevice( uint64_t serialNbr, FrameData& frame ) const;

        /** \brief Frame getting method for the connected device.
         *
         * This method checks only one device was enumerated and uses its serial number when calling
         * #_getLastFrame.
         *
         * \param[in] timeout timeout (in ms) for the call to ::ftkGetLastFrame.
         * \param[out] frame instance in which the retrieved data will be stored.
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if the call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrame( uint32_t timeout, FrameData& frame ) const;

        /** \overload Status getLastFrame(FrameData&) const
         *
         * This method checks only one device was enumerated and uses its serial number and #_DefaultTimeout
         * when calling #_getLastFrame.
         *
         * \param[in,out] frame pointer on an a frame where the data will be written.
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if the call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrame( FrameData& frame ) const;

        /** \brief Method getting one frame per device.
         *
         * This method loops over the detected devices and gets one frame for each of them.
         *
         * A call to createFrames() must have been successfully performed, otherwise the
         * Status::UnmatchingSizes status is returned. The #_getLastFrame method will be called in a loop.
         *
         * \param[in] timeout timeout (in ms) for the call to ::ftkGetLastFrame.
         * \param[out] frames container of the instances in which the retrieved data will be stored (the
         * container will automatically be resized if needed).
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrames( uint32_t timeout, std::vector< FrameData >& frames ) const;

        /** \brief Method getting one frame per device.
         *
         * \overload Status getLastFrames(std::vector< FrameData >&) const
         *
         * This method uses the instance default timeout to call
         * #getLastFrames(uint32_t,std::vector<FrameData>&) const.
         *
         * \param[out] frames container of the instances in which the retrieved data will be stored (the
         * container will automatically be resized if needed).
         *
         * \retval Status::Ok if the frame could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status getLastFrames( std::vector< FrameData >& frames ) const;

        /** \brief Method getting the strobe information from the latest retrieved frame.
         *
         * This method gets the strobe information for the latest gotten frame for the wanted device.
         *
         * A call to createFrames() must have been successfully performed, otherwise the
         * Status::UnmatchingSizes status is returned. The #_extractFrameInfo method will be called in a
         * loop.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[out] info strobe information.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( uint64_t serialNbr, ftkStrobeInfo& info ) const;
        /** \overload Status extractFrameInfo( ftkStrobeInfo& ) const
         *
         * This method allows to get the strobe information for \e the connected device.
         *
         * \param[out] info strobe information.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one device were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( ftkStrobeInfo& info ) const;
        /** \overload Status extractFrameInfo( std::vector<ftkStrobeInfo>& ) const
         *
         * This method allows to get the strobe information for \e all connected devices.
         *
         * \param[out] info strobe information for each connected devices.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one device were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( std::vector< ftkStrobeInfo >& info ) const;
        /** \brief Method getting the calibration information from the latest retrieved frame.
         *
         * This method gets the calibration information for the latest gotten frame for the wanted device.
         *
         * A call to createFrames() must have been successfully performed, otherwise the
         * Status::UnmatchingSizes status is returned. The #_extractFrameInfo method will be called in a
         * loop.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[out] info calibration parameters.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( uint64_t serialNbr, ftkStereoParameters& info ) const;
        /** \overload Status extractFrameInfo( ftkStereoParameters& ) const
         *
         * This method allows to get the calibration information for \e the connected device.
         *
         * \param[out] info calibration parameters.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one device were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( ftkStereoParameters& info ) const;
        /** \overload Status extractFrameInfo( std::vector<ftkStereoParameters>& ) const
         *
         * This method allows to get the calibration information for \e all connected devices.
         *
         * \param[out] info calibration parameters for each connected devices.
         *
         * \retval Status::Ok if the information could be successfully retrieved,
         * \retval Status::NoDevices if no devices were enumerated,
         * \retval Status::SeveralDevices if more than one device were enumerated,
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::AllocationIssue if no frame was allocated (i.e. no successful call to
         * #createFrame).
         * \retval Status::SdkWarning if a call to ::ftkGetLastFrame issued a warning,
         * \retval Status::SdkError if a call to ::ftkGetLastFrame issued an error.
         */
        Status extractFrameInfo( std::vector< ftkStereoParameters >& info ) const;

        /** \brief Device enumerating method.
         *
         * This method enumerates the connected devices. When a device is detected, its options are
         * automatically retrieved.
         *
         * \retval Status::Ok if the enumeration could be performed successfully,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::SdkWarning if the call to ::ftkEnumerateDevices issued a warning,
         * \retval Status::SdkError if the call to ::ftkEnumerateDevices issued an error.
         */
        virtual Status enumerateDevices() = 0;

        /** \brief Getter for the number of enumerated devices.
         *
         * This method allows to access the number of enumerated (i.e. detected devices). It returns the size
         * of the implementation-specific container.
         *
         * \return the number of detected devices.
         */
        virtual size_t numberOfEnumeratedDevices() const = 0;

        /** \brief Getter for TrackingSystemAbstract::_DetectedDevices.
         *
         * This method allows to access TrackingSystemAbstract::_DetectedDevices.
         *
         * \param[out] infos a copy of the  TrackingSystemAbstract::_DetectedDevices container.
         *
         * \retval Status::Ok if the getting could be performed successfully,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected).
         */
        virtual Status getEnumeratedDevices( std::vector< DeviceInfo >& infos ) const = 0;

        /** \brief Getter for the options related to the given device.
         *
         * This method allows to access the options for the device with the
         * given serial number.
         *
         * \param[in] serialNbr serial number of the device for which the options must be retrieved.
         * \param[out] opts options container.
         *
         * \retval Status::Ok if the options could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device.
         */
        virtual Status getEnumeratedOptions( uint64_t serialNbr,
                                             std::vector< DeviceOption >& opts ) const = 0;

        /** \overload Status getEnumeratedOptions(std::vector< DeviceOption >&) const
         *
         * This method allows to access the options for \e the connected device. It first checks there is
         * only one detected device, which serial number is then used in the call to #getEnumeratedOptions.
         *
         * \param[out] opts options container.
         *
         * \retval Status::Ok if the options could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device.
         */
        virtual Status getEnumeratedOptions( std::vector< DeviceOption >& opts ) const = 0;

        /** \brief Integer option getter.
         *
         * This method allows to read an integer option from its integer unique ID, for the wanted device.
         * The ::ftkGetInt32 function is called with the needed parameters.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint64_t serialNbr, uint32_t optId, ftkOptionGetter what, int32& value ) const;

        /** \overload Status getIntOption(uint64_t, uint32_t, int32&) const
         *
         * This method allows to read the current value of an integer option from its integer unique ID, for
         * the wanted device. The #getIntOption(uint64, uint32, ftkOptionGetter, int32&) const method is
         * called with ftkOptionGetter::FTK_VALUE as parameter.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint64_t serialNbr, uint32_t optId, int32& value ) const;

        /** \overload Status getIntOption(uint32_t optId, ftkOptionGetter what, int32& value) const
         *
         * This method allows to read an integer option from its integer unique ID, for \e the connected
         * device. It checks only one device was enumerated and used its serial number to call
         * #getIntOption(uint64, uint32, ftkOptionGetter, int32&) const.
         *
         * \param[in] optId option unique ID.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint32_t optId, ftkOptionGetter what, int32& value ) const;

        /** \overload Status getIntOption(uint32_t optId, int32& value) const
         *
         * This method allows to read the current value of an integer option from its integer unique ID, for
         * \e the connected device. It checks only one device was enumerated and uses its serial number
         * and ftkOptionGetter::FTK_VALUE to call
         * #getIntOption( uint64, uint32, ftkOptionGetter, int32& ) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint32_t optId, int32& value ) const;

        /** \overload Status getIntOption(uint64_t, const std::string&, ftkOptionGetter, int32&) const
         *
         * This method allows to read an integer option from its name, for the wanted device. The option
         * integer unique ID is retrieved from the given name, and then the ::ftkGetInt32 function is called.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint64_t serialNbr,
                             const std::string& optId,
                             ftkOptionGetter what,
                             int32& value ) const;

        /** \overload Status getIntOption(uint64_t, const std::string&, int32&) const
         *
         * This method allows to read the current value of an integer option from its name, for the wanted
         * device. It uses ftkOptionGetter::FTK_VALUE as argument of
         * #getIntOption(uint64, const std::string&, ftkOptionGetter, int32&) const.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( uint64_t serialNbr, const std::string& optId, int32& value ) const;

        /** \overload Status getIntOption(const std::string&, ftkOptionGetter, int32&) const
         *
         * This method allows to read an integer option from its name, for \c the connected device. It checks
         * only one device was enumerated and uses its serial number to call
         * #getIntOption(uint64, const std::string&, ftkOptionGetter, int32&) const.
         *
         * \param[in] optId option name.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( const std::string& optId, ftkOptionGetter what, int32& value ) const;

        /** \overload Status getIntOption(const std::string&, int32&) const
         *
         * This method allows to read the current value of an integer option from its name, for \e the
         * connected device. It checks only one device was enumerated and uses its serial number and
         * ftkOptionGetter::FTK_VALUE to call
         * #getIntOption(uint64, const std::string&, ftkOptionGetter, int32&) const.
         *
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 issued an error.
         */
        Status getIntOption( const std::string& optId, int32& value ) const;

        /** \brief Integer option setter.
         *
         * This method allows to write an integer option from its integer unique ID, for the wanted device.
         * The ::ftkSetInt32 function is called with the needed parameters.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetInt32 issued an error.
         */
        Status setIntOption( uint64_t serialNbr, uint32_t optId, int32 value ) const;

        /** \overload Status setIntOption(uint32_t, int32) const
         *
         * This method allows to write an integer option from its integer unique ID, for \e the connected
         * device. It checks only one device was enumerated and uses its serial number to call
         * #setIntOption(uint64, uint32, int32) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetInt32 issued an error.
         */
        Status setIntOption( uint32_t optId, int32 value ) const;

        /** \overload Status setIntOption(uint64_t, const std::string&, int32) const
         *
         * This method allows to write an integer option from its name, for the wanted device. The option
         * unique ID is retrieved from the option name, then the ::ftkSetInt32 function is called.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetInt32 issued an error.
         */
        Status setIntOption( uint64_t serialNbr, const std::string& optId, int32 value ) const;

        /** \overload Status setIntOption(const std::string&, int32) const
         *
         * This method allows to write an integer option from its name, for \e the connected device. It
         * checks only one device was enumerated and used the serial number to call
         * #setIntOption(uint64, const std::string&, int32) const.
         *
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetInt32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetInt32 issued an error.
         */
        Status setIntOption( const std::string& optId, int32 value ) const;

        /** \brief Float option getter.
         *
         * This method allows to read a float option from its integer unique ID, for the wanted device. The
         * ::ftkGetFloat32 option is called with the needed parameters.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetFloat32 issued an error.
         */
        Status getFloatOption( uint64_t serialNbr,
                               uint32_t optId,
                               ftkOptionGetter what,
                               float32& value ) const;

        /** \overload Status getFloatOption(uint32_t, ftkOptionGetter, float32&) const
         *
         * This method allows to read a float option from its integer unique ID, for \e the connected device.
         * It checks only one device was enumerated and used its serial number to call
         * #getFloatOption(uint64, uint32, ftkOptionGetter, float32&) const.
         *
         * \param[in] optId option unique ID.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( uint32_t optId, ftkOptionGetter what, float32& value ) const;

        /** \overload Status getFloatOption(uint64_t, uint32_t, float32&) const
         *
         * This method allows to read the current value of a float option from its integer unique ID, for the
         * wanted device. The ftkOptionGetter::FTK_VALUE to call
         * #getFloatOption( uint64, uint32, ftkOptionGetter, float32& ) const.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( uint64_t serialNbr, uint32_t optId, float32& value ) const;

        /** \overload Status getFloatOption( uint32_t optId, float32& value ) const
         *
         * This method allows to read a float option from its integer unique ID, for \e the connected device.
         * It checks only one device was enumerated and use its serial number and ftkOptionGetter::FTK_VALUE
         * to call #getFloatOption(uint64, uint32, ftkOptionGetter, float32&) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( uint32_t optId, float32& value ) const;

        /** \overload Status getFloatOption(uint64_t, const std::string&, ftkOptionGetter, float32&) const
         *
         * This method allows to read a float option from its name, for the wanted device. The option unique
         * ID is retrieved from the name and ::ftkGetFloat32 is called with the needed arguments.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( uint64_t serialNbr,
                               const std::string& optId,
                               ftkOptionGetter what,
                               float32& value ) const;

        /** \overload Status getFloatOption(const std::string&, ftkOptionGetter, float32&) const
         *
         * This method allows to read a float option from its name, for \e the connected device. It checks
         * only one device was enumerated and uses its serial number to call
         * #getFloatOption(uint64, const std::string&, ftkOptionGetter, float32&) const.
         *
         * \param[in] optId option name.
         * \param[in] what selector for min / max / default / current value.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( const std::string& optId, ftkOptionGetter what, float32& value ) const;

        /** \overload Status getFloatOption(uint64_t, const std::string&, float32&) const
         *
         * This method allows to read the current value of a float option from its name, for the wanted
         * device. It uses ftkOptionGetter::FTK_VALUE to call
         * #getFloatOption( uint64, const std::string&, ftkOptionGetter, float32& ) const.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( uint64_t serialNbr, const std::string& optId, float32& value ) const;

        /** \overload Status getFloatOption(const std::string&, float32&) const
         *
         * This method allows to read a float option from its name, for \e the connected device. It checks
         * only one device was enumerated and uses its serial number and ftkOptionGetter::FTK_VALUE to call
         * #getFloatOption( uint64, const std::string&, ftkOptionGetter, float32& ) const.
         *
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGeFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkGeFloat32 issued an error.
         */
        Status getFloatOption( const std::string& optId, float32& value ) const;

        /** \brief Float option setter.
         *
         * This method allows to write a float option from its integer unique ID, for the wanted device. It
         * calls the ::ftkSetFloat32 function with the needed parameters.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetFloat32 issued an error.
         */
        Status setFloatOption( uint64_t serialNbr, uint32_t optId, float32 value ) const;

        /** \overload Status setFloatOption(uint32_t, float32) const
         *
         * This method allows to write a float option from its integer unique ID, for \e the connected
         * device. It checks only one device was enumerated and uses its serial number to call
         * #setFloatOption( uint64, uint32, float32) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetFloat32 issued an error.
         */
        Status setFloatOption( uint32_t optId, float32 value ) const;

        /** \overload Status setFloatOption(uint64_t, const std::string&, float32) const
         *
         * This method allows to write a float option from its name, for the wanted device. It retrieved the
         * option unique ID from the option name and calls the ::ftkSetFloat32 with the needed arguments.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetFloat32 issued an error.
         */
        Status setFloatOption( uint64_t serialNbr, const std::string& optId, float32 value ) const;

        /** \overload Status setFloatOption(const std::string&, float32) const
         *
         * This method allows to write a float option from its name, for \e the connected device. It checks
         * only one device was enumerated and uses its serial number to call
         * #setFloatOption( uint64, const std::string& , float32) const
         *
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetFloat32 issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetFloat32 issued an error.
         */
        Status setFloatOption( const std::string& optId, float32 value ) const;

        /** \brief Data option getter.
         *
         * This method allows to read a data option from its integer unique ID, for the wanted device. It
         * calls ::ftkGetData with the needed parameters.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint64_t serialNbr, uint32_t optId, ftkBuffer& value ) const;

        /** \overload Status getDataOption(uint64_t, uint32_t, std::string&) const
         *
         * This method allows to read a data option from its integer unique ID, for the wanted device. It
         * calls the #getDataOption( uint64, uint32, ftkBuffer& ) const method and converts the ftkBuffer
         * into a \c std::string.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option as a string.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint64_t serialNbr, uint32_t optId, std::string& value ) const;

        /** \overload Status getDataOption(uint32_t, ftkBuffer&) const
         *
         * This method allows to read a data option from its integer unique ID, for \e the connected device.
         * It checks only one device was enumerated and uses its serial number to call
         * #getDataOption( uint64_t serialNbr, uint32_t optId, ftkBuffer& value ) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint32_t optId, ftkBuffer& value ) const;

        /** \overload Status getDataOption(uint32_t, std::string) const
         *
         * This method allows to read a data option from its integer unique ID, for \e the connected device.
         * The #getDataOption( uint64, uint32, std::string&) const method is called and the ftkBuffer is
         * converted into a \c std::string.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option as a string.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint32_t optId, std::string& value ) const;

        /** \overload Status getDataOption(uint64_t, const std::string&, ftkBuffer&) const
         *
         * This method allows to read a data option from its name, for the wanted device. It retrieved the
         * option unique ID from the option name and calls the ::ftkGetData function with the needed
         * arguments.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint64_t serialNbr, const std::string& optId, ftkBuffer& value ) const;

        /** \overload Status getDataOption(uint64_t, const std::string&, std::string&) const
         *
         * This method allows to read a data option from its name, for the wanted device. The
         * #getDataOption( uint64_t serialNbr, const std::string& optId, ftkBuffer& value ) const method is
         * called and the ftkBuffer is converted into a \c std::string.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[out] value value of the option as a string.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( uint64_t serialNbr, const std::string& optId, std::string& value ) const;

        /** \overload Status getDataOption(const std::string&, ftkBuffer&) const
         *
         * This method allows to read a data option from its name, for \e the connected device. It checks
         * only one device was enumerated and uses its serial number to call
         * #getDataOption( uint64, const std::string&, ftkBuffer& ) const.
         *
         * \param[in] optId option name.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( const std::string& optId, ftkBuffer& value ) const;

        /** \overload Status getDataOption(const std::string&, std::string&) const
         *
         * This method allows to read a data option from its name, for \e the connected device. It calls the
         * #getDataOption( const std::string&, ftkBuffer& ) const method and converts the ftkBuffer into a
         * \c std::string.
         *
         * \param[in] optId option name.
         * \param[out] value value of the option as a string.
         *
         * \retval Status::Ok if the option value could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were enumerated,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkGetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetData issued an error.
         */
        Status getDataOption( const std::string& optId, std::string& value ) const;

        /** \brief Data option setter.
         *
         * This method allows to write a data option from its integer unique ID, for the wanted device. The
         * ::ftkSetData function is called with the needed arguments.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[in] value new value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint64_t serialNbr, uint32_t optId, const ftkBuffer& value ) const;

        /** \overload Status setDataOption(uint64_t, uint32_t, const std::string&) const
         *
         * This method allows to write a data option from its integer unique ID, for the wanted device. The
         * \c std::string is converted into ftkBuffer and the
         * #setDataOption( uint64, uint32, const ftkBuffer& ) const method is called.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[out] value value of the option as a string.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint64_t serialNbr, uint32_t optId, const std::string& value ) const;

        /** \overload Status setDataOption(uint32_t, const ftkBuffer&) const
         *
         * This method allows to write a data option from its integer unique ID, for \e the connected device.
         * It checks only one device was enumerated and uses its serial number to call
         * #setDataOption( uint64, uint32, const std::string& ) const.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint32_t optId, const ftkBuffer& value ) const;

        /** \overload Status setDataOption(uint32_t, const std::string&) const
         *
         * This method allows to write a data option from its integer unique ID, for \e the connected device.
         * The \c std::string is converted into ftkBuffer and the
         * #setDataOption( uint32, const ftkBuffer& ) const method is called.
         *
         * \param[in] optId option unique ID.
         * \param[out] value value of the option as string.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint32_t optId, const std::string& value ) const;

        /** \overload Status setDataOption(uint64_t, const std::string&, const ftkBuffer&) const
         *
         * This method allows to write a data option from its name, for the wanted device. The option unique
         * ID is retrieved from the name and the ::ftkSetData function is called with the needed arguments.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[in] value new value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint64_t serialNbr, const std::string& optId, const ftkBuffer& value ) const;

        /** \overload Status setDataOption(uint64_t, const std::string&, const std::string&) const
         *
         * This method allows to write a data option from its name, for the wanted device. The \c std::string
         * is converted into ftkBuffer and the
         * TrackingSystem::setDataOption(uint64,const std::string&,const ftkBuffer&) const method is called.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[in] value new value of the option as string.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( uint64_t serialNbr, const std::string& optId, const std::string& value ) const;

        /** \overload Status setDataOption( const std::string&, const ftkBuffer& ) const
         *
         * This method allows to write a data option from its name, for \e the connected device. It checks
         * only one device was enumerated and uses its serial number to call the
         * TrackingSystem::setDataOption(uint64,const std::string&,const ftkBuffer&) const method.
         *
         * \param[in] optId option name.
         * \param[in] value new value of the option.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( const std::string& optId, const ftkBuffer& value ) const;

        /** \overload Status setDataOption(const std::string&, const std::string&) const
         *
         * This method allows to write a data option from its name, for \e the connected device. The
         * \c std::string is converted into ftkBuffer and the
         * #setDataOption( const std::string&, const ftkBuffer& ) const method is called.
         *
         * \param[in] optId option name.
         * \param[in] value new value of the option as string.
         *
         * \retval Status::Ok if the option value could be successfully set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::InvalidOption if the option does not exist,
         * \retval Status::SdkWarning if the call to ::ftkSetData issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetData issued an error.
         */
        Status setDataOption( const std::string& optId, const std::string& value ) const;

        /** \brief Geometry setter.
         *
         * This method sets the wanted geometry for the wanted device. It internally calls ::ftkSetGeometry.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geom geometry to register.
         *
         * \retval Status::Ok if the geometry could be successfully set in the SDK,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkSetGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetGeometry issued an error.
         */
        Status setGeometry( uint64_t serialNbr, const ftkGeometry& geom );

        /** \overload Status setGeometry(uint64_t, const ftkRigidBody&)
         *
         * This method sets the wanted geometry for the wanted device. It internally calls ::ftkSetRigidBody.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geom geometry to register.
         *
         * \retval Status::Ok if the geometry could be successfully set in the SDK,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkSetRigidBody issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetRigidBody issued an error.
         */
        Status setGeometry( uint64_t serialNbr, const ftkRigidBody& geom );

        /** \overload Status setGeometry(uint64_t serialNbr, const std::string&)
         *
         * This method sets the wanted geometry for the wanted device. It internally calls the
         * atracsys::loadGeometry function and then the #setGeometry(uint64, const ftkGeometry&) method.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geomFileName path of the geometry file to load.
         *
         * \retval Status::Ok if the option value could be successfully set in the SDK,
         * \retval Status::InvalidFile if the file could not be opened (i.e. the file does not exist or
         * the path is wrong),
         * \retval Status::ParsingError if the file syntax is wrong or if some needed information is missing,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkSetGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetGeometry issued an error.
         */
        Status setGeometry( uint64_t serialNbr, const std::string& geomFileName );

        /** \overload Status setGeometry(const ftkGeometry&)
         *
         * This method sets the wanted geometry for \e all connected devices. It loops over the detected
         * devices and calls the #setGeometry( uint64, const ftkGeometry& ) method for each of them.
         *
         * \param[in] geom geometry to register.
         *
         * \retval Status::Ok if the option value could be successfully set in the SDK,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkSetGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetGeometry issued an error.
         */
        Status setGeometry( const ftkGeometry& geom );

        /** \overload Status setGeometry(const ftkRigidBody&)
         *
         * This method sets the wanted geometry for \e all connected devices. It loops over the detected
         * devices and calls the #setGeometry(uint64, const ftkRigidBody&) method for each of them.
         *
         * \param[in] geom geometry to register.
         *
         * \retval Status::Ok if the option value could be successfully set in the SDK,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkSetRigidBody issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetRigidBody issued an error.
         */
        Status setGeometry( const ftkRigidBody& geom );

        /** \overload Status setGeometry(const std::string&)
         *
         * This method sets the wanted geometry for \e all connected devices. It internally calls the
         * atracsys::loadGeometry and then loops over the detected devices and calls the
         * #setGeometry(uint64, const ftkGeometry&) method for each of them.
         *
         * \param[in] geomFileName path of the geometry file to load.
         *
         * \retval Status::Ok if the option value could be successfully set in the SDK,
         * \retval Status::InvalidFile if the file could not be opened (i.e. the file does not exist or the
         * path is wrong),
         * \retval Status::ParsingError if the file syntax is wrong or if some needed information is missing,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkSetRigidBody issued a warning,
         * \retval Status::SdkError if the call to ::ftkSetRigidBody issued an error.
         */
        Status setGeometry( const std::string& geomFileName );

        /** \brief Method setting a geometry from a geometry file path \e or file contents.
         *
         * This method allows to call ::ftkRegisterRigidBody for the given device. It accepts either the
         * file path or the file contents as argument. The file is loaded \e and the loaded geometry is
         * registered in the SDK, which will be able to reconstruct it.
         *
         * Calling this method causes the reconstructed corresponding marker to be contained in
         * FrameData::_SixtyFourFiducialsMarkers, whatever number of fiducials.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geomFileName path of the geometry file to be loaded \e or the file contents.
         *
         * \retval Status::Ok if the rigid body could be successfully loaded and set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkRegisterRigidBody issued a warning,
         * \retval Status::SdkError if the call to ::ftkRegisterRigidBody issued an error.
         */
        Status registerRigidBody( uint64_t serialNbr, const std::string& geomFileName );

        /** \overload Status registerRigidBody(const std::string&)
         *
         * This method sets the wanted geometry for \e all connected devices. It internally calls the
         * ::ftkRegisterRigidBody function for all devices.
         *
         * \retval Status::Ok if the rigid body could be successfully loaded and set,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkRegisterRigidBody issued a warning,
         * \retval Status::SdkError if the call to ::ftkRegisterRigidBody issued an error.
         */
        Status registerRigidBody( const std::string& geomFileName );

        /** \brief Geometry clearing method.
         *
         * This method clears the wanted geometry for the wanted device. It internally calls
         * ::ftkClearGeometry
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geomId geometry ID to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( uint64_t serialNbr, uint32_t geomId );

        /** \overload Status unsetGeometry(uint64_t, const ftkGeometry&)
         *
         * This method clears the wanted geometry for the wanted device. It internally calls
         * #unsetGeometry(uint64, uint32_t) using \c geom.geometryId.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geom geometry to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( uint64_t serialNbr, const ftkGeometry& geom );

        /** \overload Status unsetGeometry(uint64_t, const ftkRigidBody&)
         *
         * This method clears the wanted geometry for the wanted device. It internally calls
         * #unsetGeometry(uint64, uint32_t) using \c geom.geometryId.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geom geometry to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( uint64_t serialNbr, const ftkRigidBody& geom );

        /** \overload Status unsetGeometry(uint64_t, const std::string&)
         *
         * This method clears the wanted geometry for the wanted device. It internally calls the
         * atracsys::loadGeometry function and then the #unsetGeometry( uint64_t serialNbr, uint32_t geomId )
         * method with the loaded geometry ID.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] geomFileName path of the geometry file to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::InvalidFile if the file could not be opened (i.e. the file does not exist or the
         * path is wrong),
         * \retval Status::ParsingError if the file syntax is wrong or if some needed information is missing,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( uint64_t serialNbr, const std::string& geomFileName );

        /** \overload Status unsetGeometry(uint32_t)
         *
         * This method clears the wanted geometry for \e all connected devices. It internally loops over the
         * detected devices and calls #unsetGeometry(uint64_t, uint32_t) for each of them.
         *
         * \param[in] geomId geometry ID to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( uint32_t geomId );

        /** \overload Status unsetGeometry(const ftkGeometry& )
         *
         * This method clears the wanted geometry for \e all connected devices. It internally calls the
         * #unsetGeometry(uint32_t) method.
         *
         * \param[in] geom geometry to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( const ftkGeometry& geom );

        /** \overload Status unsetGeometry(const ftkRigidBody&)
         *
         * This method clears the wanted geometry for \e all connected devices. It internally calls the
         * #unsetGeometry( uint32_t ) method.
         *
         * \param[in] geom geometry to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( const ftkRigidBody& geom );

        /** \overload Status unsetGeometry(const std::string&)
         *
         * This method clears the wanted geometry for \e all connected devices. It internally calls the
         * atracsys::loadGeometry function, then calls the #unsetGeometry(uint32_t) method.
         *
         * \param[in] geomFileName path of the geometry file to clear.
         *
         * \retval Status::Ok if the geometry could be successfully cleared,
         * \retval Status::InvalidFile if the file could not be opened (i.e. the file does not exist or the
         * path is wrong),
         * \retval Status::ParsingError if the file syntax is wrong or if some needed information is missing,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SdkWarning if the call to ::ftkClearGeometry issued a warning,
         * \retval Status::SdkError if the call to ::ftkClearGeometry issued an error.
         */
        Status unsetGeometry( const std::string& geomFileName );

        /** \brief Triangulation method.
         *
         * This method triangulates a 3D point for the wanted device. It internally calls ::ftkTriangulate.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] leftPixel left camera blob position.
         * \param[in] rightPixel rightt camera blob position.
         * \param[out] outPoint pointer on the triangulated point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkTriangulate issued a warning,
         * \retval Status::SdkError if the call to ::ftkTriangulate issued an error.
         */
        Status triangulate( uint64_t serialNbr,
                            const std::array< float, 2u >& leftPixel,
                            const std::array< float, 2u >& rightPixel,
                            ftk3DPoint& outPoint ) const;

        /** \overload Status triangulate(uint64_t, const std::array<float, 2u>&, const std::array<float, 2u>&,
         * std::array< float, 3u >&) const
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] leftPixel left camera blob position.
         * \param[in] rightPixel rightt camera blob position.
         * \param[out] outPoint triangulated point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkTriangulate issued a warning,
         * \retval Status::SdkError if the call to ::ftkTriangulate issued an error.
         */
        Status triangulate( uint64_t serialNbr,
                            const std::array< float, 2u >& leftPixel,
                            const std::array< float, 2u >& rightPixel,
                            std::array< float, 3u >& outPoint ) const;

        /** \overload Status triangulate(const std::array<float, 2u>&, const std::array<float, 2u>&,
         * ftk3DPoint&) const
         *
         * This function triangulates the given points for \e the detected device.
         *
         * \param[in] leftPixel left camera blob position.
         * \param[in] rightPixel rightt camera blob position.
         * \param[out] outPoint pointer on the triangulated point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkTriangulate issued a warning,
         * \retval Status::SdkError if the call to ::ftkTriangulate issued an error.
         */
        Status triangulate( const std::array< float, 2u >& leftPixel,
                            const std::array< float, 2u >& rightPixel,
                            ftk3DPoint& outPoint ) const;

        /** \overload Status triangulate(const std::array<float, 2u>&, const std::array<float, 2u>&,
         * std::array<float, 3u>&) const
         *
         * This function triangulates the given points for \e the detected device.
         *
         * \param[in] leftPixel left camera blob position.
         * \param[in] rightPixel rightt camera blob position.
         * \param[out] outPoint pointer on the triangulated point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkTriangulate issued a warning,
         * \retval Status::SdkError if the call to ::ftkTriangulate issued an error.
         */
        Status triangulate( const std::array< float, 2u >& leftPixel,
                            const std::array< float, 2u >& rightPixel,
                            std::array< float, 3u >& outPoint ) const;

        /** \brief Reprojection method.
         *
         * This method projects a 3D point on the two cameras. It internally calls ::ftkReprojectPoint.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] inPoint 3D point.
         * \param[out] outLeftData left projected point.
         * \param[out] outRightData right projected point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkReprojectPoint issued a warning,
         * \retval Status::SdkError if the call to ::ftkReprojectPoint issued an error.
         */
        Status reproject( uint64_t serialNbr,
                          const ftk3DPoint& inPoint,
                          std::array< float, 2u >& outLeftData,
                          std::array< float, 2u >& outRightData ) const;

        /** \overload Status reproject(uint64, const
         * std::array<float,3u>&,std::array<float,2u>&,std::array<float,2u>&) const
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] inPoint 3D point.
         * \param[out] outLeftData left projected point.
         * \param[out] outRightData right projected point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         * enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkReprojectPoint issued a warning,
         * \retval Status::SdkError if the call to ::ftkReprojectPoint issued an error.
         */
        Status reproject( uint64_t serialNbr,
                          const std::array< float, 3u >& inPoint,
                          std::array< float, 2u >& outLeftData,
                          std::array< float, 2u >& outRightData ) const;

        /** \overload Status reproject(const ftk3DPoint&, std::array<float, 2u>&, std::array<float, 2u>&)
         * const
         *
         * This method projects a 3D point on the two cameras for \e the detected device.
         *
         * \param[in] inPoint 3D point.
         * \param[out] outLeftData left projected point.
         * \param[out] outRightData right projected point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkReprojectPoint issued a warning,
         * \retval Status::SdkError if the call to ::ftkReprojectPoint issued an error.
         */
        Status reproject( const ftk3DPoint& inPoint,
                          std::array< float, 2u >& outLeftData,
                          std::array< float, 2u >& outRightData ) const;

        /** \overload Status reproject(const std::array<float,3u>&, std::array<float,2u>&,
         * std::array<float,2u>&) const
         *
         * This method projects a 3D point on the two cameras for \e the detected device.
         *
         * \param[in] inPoint 3D point.
         * \param[out] outLeftData left projected point.
         * \param[out] outRightData right projected point.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkReprojectPoint issued a warning,
         * \retval Status::SdkError if the call to ::ftkReprojectPoint issued an error.
         */
        Status reproject( const std::array< float, 3u >& inPoint,
                          std::array< float, 2u >& outLeftData,
                          std::array< float, 2u >& outRightData ) const;

        /** \brief Method reading the available accelerometer(s).
         *
         * This method allows to access the accelerometer(s) of the wanted device.
         *
         * \param[in] serialNbr
         * \param[out] data acceleration components for each readable sensor.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         *  enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkGetAccelerometerData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetAccelerometerData issued an error.
         */
        Status getAccelerometerData( uint64_t serialNbr, std::vector< std::array< float, 3u > >& data ) const;
        /** \overload getAccelerometerData( std::vector<std::array<float, 3u> >&) const
         *
         * This method allows to access the accelerometer(s) of \e the connected device.
         *
         * \param[out] data acceleration components for each readable sensor.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkGetAccelerometerData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetAccelerometerData issued an error.
         */
        Status getAccelerometerData( std::vector< std::array< float, 3u > >& data ) const;
        /** \overload getAccelerometerData( std::vector<std::vector<std::array<float, 3u>> >&) const
         *
         * This method allows to access the accelerometer(s) of \e all connected device.
         *
         * \param[out] data acceleration components for each readable sensor, for each device.
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::SdkWarning if the call to ::ftkGetAccelerometerData issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetAccelerometerData issued an error.
         */
        Status getAccelerometerData( std::vector< std::vector< std::array< float, 3u > > >& data ) const;

        /** \brief Method reading the device real time clock.
         *
         * This method allows to access the RTC of the wanted device.
         *
         * \param[in] serialNbr
         * \param[out] epoch Unix timestamp (i.e. seconds since 1970-01-01 00:00:00).
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not correspond to a known (i.e.
         *  enumerated) device,
         * \retval Status::SdkWarning if the call to ::ftkGetRealTimeClock issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetRealTimeClock issued an error.
         */
        Status getRealTimeClock( uint64_t serialNbr, timestamp_t& epoch ) const;
        /** \overload getRealTimeClock(timestamp_t&) const
         *
         * This method allows to access the RTC of \e the connected device.
         *
         * \param[out] epoch Unix timestamp (i.e. seconds since 1970-01-01 00:00:00).
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::SeveralDevices if more than one device have been detected,
         * \retval Status::SdkWarning if the call to ::ftkGetRealTimeClock issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetRealTimeClock issued an error.
         */
        Status getRealTimeClock( timestamp_t& epoch ) const;
        /** \overload Status getRealTimeClock( std::vector< timestamp_t >& ) const
         *
         * This method allows to access the RTC of \e all connected device.
         *
         * \param[out] epoch Unix timestamp (i.e. seconds since 1970-01-01 00:00:00).
         *
         * \retval Status::OK if the value retrieval could be done correctly,
         * \retval Status::LibNotInitialised if the underlying library handle was not initialised (i.e.
         * #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either #enumerateDevices was not
         * called or no devices were detected),
         * \retval Status::UnmatchingSizes if the frames input container size does not match the
         * #_numberOfEnumeratedDevices number,
         * \retval Status::SdkWarning if the call to ::ftkGetRealTimeClock issued a warning,
         * \retval Status::SdkError if the call to ::ftkGetRealTimeClock issued an error.
         */
        Status getRealTimeClock( std::vector< timestamp_t >& epoch ) const;

    protected:
        /** \brief String used to identify the error content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the start of the error part.
         */
        static const std::string _ErrorOpenTag;
        /** \brief String used to identify the error content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the end of the error part.
         */
        static const std::string _ErrorCloseTag;
        /** \brief String used to identify the warning content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the start of the warning part.
         */
        static const std::string _WarningOpenTag;
        /** \brief String used to identify the warning content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the end of the warning part.
         */
        static const std::string _WarningCloseTag;
        /** \brief String used to identify the message content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the start of the message part.
         */
        static const std::string _MessageOpenTag;
        /** \brief String used to identify the message content of the string obtained from
         * ftkGetLastErrorString.
         *
         * This string is the marker for the end of the message part.
         */
        static const std::string _MessageCloseTag;

        /** \brief Getter for a mutex protecting the device container.
         *
         * This method allows to get a mutex instance which protects the access
         * to the device container.
         *
         * \return a reference on the appropriate mutex.
         */
        virtual std::mutex& _enumeratedDevicesProtector() const = 0;

        /** \brief Getter for the number of enumerated devices.
         *
         * This method allows to get the number of enumerated devices.
         *
         * \return the size of a device container.
         */
        virtual size_t _numberOfEnumeratedDevices() const = 0;

        /** \brief Helper function which populates a global option container.
         *
         * This method is meant to populate an implementation-specific
         * container with the global options (i.e. options which don't need a
         * serial number).
         *
         * \retval Status::Ok if the global options could e successfully retrieved,
         * \retval Status::SdkWarning if the call to the SDK function issued a warning,
         * \retval Status::SdkError if the call to the SDK function triggered an error.
         */
        virtual Status _populateGlobalOptions() = 0;

        /** \brief Helper function checking whether the device exist.
         *
         * This function checks whether the given serial number is a valid
         * device.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[out] dev the device information instance.
         * \param[out] idx index of the device in the TrackingSystem::_DetectedDevices
         * container.
         *
         * \retval Status::Ok if the device could be successfully retreived,
         * \retval Status::LibNotInitialised if the underlying library handle
         * was not initialised (i.e. #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not
         * correspond to a known (i.e. enumerated) device.
         */
        virtual Status _getDevice( uint64_t serialNbr, DeviceInfo& dev, size_t& idx ) const = 0;

        /** \brief Generic option getter.
         *
         * This method is a generic getter for the DeviceOption instance
         * corresponding to a given option unique ID.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option unique ID.
         * \param[in] optType type of the wanted option.
         * \param[out] opt description of the wanted option.
         *
         * \retval Status::Ok if the wanted option could be successfully
         * retrieved,
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not
         * correspond to a known (i.e. enumerated) device,
         * \retval Status::InvalidOption if the option could not be found.
         */
        virtual Status _getOptionDesc( uint64_t serialNbr,
                                       uint32_t optId,
                                       ftkOptionType optType,
                                       DeviceOption& opt ) const = 0;

        /** \overload _Status _getOptionDesc(uint64, const std::string&, ftkOptionType, DeviceOption&) const
         *
         * This method is a generic getter for the DeviceOption instance
         * corresponding to a given option name.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] optId option name.
         * \param[in] optType type of the wanted option.
         * \param[out] opt description of the wanted option.
         *
         * \retval Status::Ok if the wanted option could be successfully
         * retrieved,
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not
         * correspond to a known (i.e. enumerated) device,
         * \retval Status::InvalidOption if the option could not be found.
         */
        virtual Status _getOptionDesc( uint64_t serialNbr,
                                       const std::string& optId,
                                       ftkOptionType optType,
                                       DeviceOption& opt ) const = 0;

        /** \brief Getter for the \f$i\f$-th device serial number.
         *
         * This method allows to get the serial number of device \f$i\f$.
         *
         * \param[in] i index of the device in the implementation-specific
         * container.
         *
         * \return the serial number of the selected device,
         * \retval 0uLL if the \c i index is invalid.
         */
        virtual uint64_t _deviceSerialNumber( size_t i = 0u ) const = 0;

        /** \brief Method getting the latest frame for the wanted device.
         *
         * This method calls ::ftkGetLastFrame for the specified device. The
         * ::ftkFrameQuery instance dedicated to the desired device will
         * automatically be used. It is first checked the given serial number
         * is valid.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in] timeout timeout in \f$\si{\milli\second}\f$ to allow
         * frame getting.
         * \param[out] frame instance of FrameData containing the retreived
         * frame.
         *
         * \retval Status::Ok if everything went fine,
         * \retval Status::LibNotInitialised is the library instance was not
         * properly initialised,
         * \retval Status::NoDevices is no devices are connected,
         * \retval Status::UnknownDevice is the given serial number is invalid,
         * \retval Status::SdkWarning if a warning was issued by the SDK,
         * \retval Status::SdkError if an error was issued by the SDK.
         */
        Status _getLastFrame( uint64_t serialNbr, uint32_t timeout, FrameData& frame ) const;

        /** \brief Method extracting the information from the latest frame for the wanted device.
         *
         * This method calls #ftkExtractFrameInfo for the specified device. The ::ftkFrameQuery
         * instance dedicated to the desired device will automatically be used. It is first checked the given
         * serial number is valid.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[in,out] info instance of ftkFrameInfoData, used to set what is the retrieved information
         * and get it back.
         *
         * \retval Status::Ok if everything went fine,
         * \retval Status::LibNotInitialised is the library instance was not
         * properly initialised,
         * \retval Status::NoDevices is no devices are connected,
         * \retval Status::UnknownDevice is the given serial number is invalid,
         * \retval Status::SdkWarning if a warning was issued by the SDK,
         * \retval Status::SdkError if an error was issued by the SDK.
         */
        Status _extractFrameInfo( uint64_t serialNbr, ftkFrameInfoData& info ) const;

        /** \brief Method performing the int option reading.
         *
         * This method performs the call to ::ftkGetInt32 and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[in] what which option value must be retrieved (default,
         * current, minimum or maximum).
         * \param[out] value wanted value.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkGetInt32 returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkGetInt32 returned an
         * error.
         */
        Status _getIntOption( uint64_t serialNbr, uint32_t optId, ftkOptionGetter what, int32& value ) const;

        /** \brief Method performing the int option writing.
         *
         * This method performs the call to ::ftkSetInt32 and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[in] value new value for the option.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkSetInt32 returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkSetInt32 returned an
         * error.
         */
        Status _setIntOption( uint64_t serialNbr, uint32_t optId, int32 value ) const;

        /** \brief Method performing the floating point option reading.
         *
         * This method performs the call to ::ftkGetFloat32 and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[in] what which option value must be retrieved (default,
         * current, minimum or maximum).
         * \param[out] value wanted value.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkGetFloat32 returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkGetFloat32 returned an
         * error.
         */
        Status _getFloatOption( uint64_t serialNbr,
                                uint32_t optId,
                                ftkOptionGetter what,
                                float& value ) const;

        /** \brief Method performing the floating point option writing.
         *
         * This method performs the call to ::ftkSetFloat32 and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[in] value new value for the option.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkSetFloat32 returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkSetFloat32 returned an
         * error.
         */
        Status _setFloatOption( uint64_t serialNbr, uint32_t optId, float value ) const;

        /** \brief Method performing the binary option reading.
         *
         * This method performs the call to ::ftkGetData and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[out] value wanted value.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkGetData returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkGetData returned an
         * error.
         */
        Status _getDataOption( uint64_t serialNbr, uint32_t optId, ftkBuffer& value ) const;

        /** \brief Method performing the binary option writing.
         *
         * This method performs the call to ::ftkSetData and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] optId unique ID of the wanted option.
         * \param[in] value new value for the option.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkSetData returned a
         * warning,
         * \retval Status::SdkError if the call to ::ftkSetData returned an
         * error.
         */
        Status _setDataOption( uint64_t serialNbr, uint32_t optId, const ftkBuffer& value ) const;

        /** \brief Method performing the triangulation.
         *
         * This method performs the call to ::ftkTriangulate and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] point3dLeftPixel left raw data, \f$ z \f$ coordinate set
         * to zero.
         * \param[in] point3dRightPixel right raw data, \f$ z \f$ coordinate
         * set to zero.
         * \param[out] outPoint triangulated point.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkTriangulate returned
         * a warning,
         * \retval Status::SdkError if the call to ::ftkTriangulate returned an
         * error.
         */
        Status _triangulate( uint64_t serialNbr,
                             const ftk3DPoint& point3dLeftPixel,
                             const ftk3DPoint& point3dRightPixel,
                             ftk3DPoint& outPoint ) const;

        /** \brief Method performing the reprojection.
         *
         * This method performs the call to ::ftkReprojectPoint and handles the
         * various returned error codes.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[in] inPoint point to reproject.
         * \param[out] leftPoint left projection, \f$ z \f$ coordinate set
         * to zero.
         * \param[out] rightPoint right projection, \f$ z \f$ coordinate set
         * to zero.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkReprojectPoint
         * returned a warning,
         * \retval Status::SdkError if the call to ::ftkReprojectPoint returned
         * an error.
         */
        Status _reproject( uint64_t serialNbr,
                           const ftk3DPoint& inPoint,
                           ftk3DPoint& leftPoint,
                           ftk3DPoint& rightPoint ) const;

        /** \brief Method reading the accelerometer(s).
         *
         * This method performs the call to ::ftkGetAccelerometerData and handles the various returned
         * statuses.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[out] data acceleration components for each readable sensor.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to ::ftkGetAccelerometerData returned a warning,
         * \retval Status::SdkError if the call to ::ftkGetAccelerometerData returned an error.
         */
        Status _getAccelerometerData( uint64_t serialNbr,
                                      std::vector< std::array< float, 3u > >& data ) const;

        /** \brief Method reading the real time clock.
         *
         * This method performs the call to #ftkGetRealTimeClock and handles the various returned
         * statuses.
         *
         * \param[in] serialNbr serial number of the desired device.
         * \param[out] epoch time point object, which value is the time since Unix EPOCH.
         *
         * \retval Status::Ok if the value could be retrieved successfully,
         * \retval Status::SdkWarning if the call to #ftkGetRealTimeClock returned a warning,
         * \retval Status::SdkError if the call to #ftkGetRealTimeClock returned an error.
         */
        Status _getRealTimeClock( uint64_t serialNbr, timestamp_t& epoch ) const;

        /** \brief Default timeout for getting a frame.
         */
        uint32_t _DefaultTimeout;

        /** \brief Mutex preventing multithreaded access to #_DefaultTimeout.
         */
        mutable std::mutex _DefaultTimeoutProtector;

        /** \brief Toggles on / off the rejection of simulator.
         *
         * If set to \c false, a simulator won't be enumerated as a device.
         * This is used in cases where the simulator cannot emulate a device,
         * for instance when dealing with wireless markers.
         */
        bool _AllowSimulator;

        /** \brief Internal log reporting level.
         */
        LogLevel _Level;

        /** \brief Mutex preventing multithreaded access to #_Level.
         */
        mutable std::mutex _LevelProtector;

        /** \brief Library handle.
         */
        ftkLibrary _Library;

        /** \brief Mutex preventing multithreaded access #_Level.
         */
        mutable std::mutex _LibraryProtector;

        /** \brief Container of created frames, for automatic deletion.
         */
        std::deque< ftkFrameQuery* > _AllocatedFrames;

        /** \brief Mutex preventing multithreaded access to the allocated
         * frames.
         */
        mutable std::mutex _AllocatedFramesProtector;

        /** \brief Muteces to protect the streaming of errors.
         *
         * This member is lazy-initialised, i.e. a new key-value pair is
         * inserted when needed.
         */
        mutable std::unordered_map< std::ostream*, std::unique_ptr< std::mutex > > _StreamLocks;

        /** \brief Mutex preventing multithreaded access to the stream-specific
         * muteces.
         */
        mutable std::mutex _StreamLocksProtector;
    };

    /** \brief Function reading a key from a given section of an INI file.
     *
     * This function extracts the data of a section/key pair out of the data
     * loaded from an INI file.
     *
     * \param[in] haystack parsed information, in the format (sectionName,
     * keyName, value).
     * \param[in] secName name of the section to read.
     * \param[in] keyName name of the key to read.
     * \param[out] value numeric extracted value.
     * \param[out] errors stream in which the potential error messages will be
     * written.
     *
     * \tparam T type of the read numeric value.
     *
     * \retval true if the reading could be done successfully,
     * \retval false if an error occurred, e.g. non-existing section/key
     * combination, value could not be translated from the string to the
     * numeric type. The \c errors stream gives additional information.
     */
    template< typename T >
    bool readValueFromIniFile(
      const std::vector< std::tuple< std::string, std::string, std::string > >& haystack,
      const std::string& secName,
      const std::string& keyName,
      T& value,
      std::ostream& errors );

    /** \brief Helper function transforming a string in a numeric value.
     *
     * This function provides a wrapper around the \c stoul, \c stof, etc
     * functions family.
     *
     * Currently, only the \c uint32_t and \c float specialisation are
     * implemented.
     *
     * \param[in] str value to translate.
     * \param[out] value numeric read value.
     *
     * \tparam T type of the numeric value to read.
     */
    template< typename T >
    void stringReader( const std::string& str, T& value );

    // --------------------------------------------------------------------- //
    //                                                                       //
    //                 Implementation of templated functions                 //
    //                                                                       //
    // --------------------------------------------------------------------- //

    template< typename T >

    bool readValueFromIniFile(
      const std::vector< std::tuple< std::string, std::string, std::string > >& haystack,
      const std::string& secName,
      const std::string& keyName,
      T& value,
      std::ostream& errors )
    {
        auto pos(
          find_if( haystack.cbegin(),
                   haystack.cend(),
                   [&secName, &keyName]( const std::tuple< std::string, std::string, std::string >& item ) {
                       return std::get< 0u >( item ) == secName && std::get< 1u >( item ) == keyName;
                   } ) );
        if ( pos == haystack.cend() )
        {
            errors << "Cannot find key " << keyName << " in section [" << secName << "]" << std::endl;
            return false;
        }
        try
        {
            stringReader( std::get< 2u >( *pos ), value );
        }
        catch ( std::exception& e )
        {
            errors << "Cannot read key " << keyName << " in section [" << secName << "]" << std::endl;
            errors << e.what() << std::endl;
            return false;
        }

        return true;
    }
}  // namespace atracsys

/**
 * \}
 */

/** \file TrackingSystem.hpp
 * \brief Definition of the atracsys::TrackingSystem class.
 *
 * This file is the file to include to enable the C++ API in your program.
 */
#pragma once

#include "AdditionalDataStructures.hpp"
#include "TrackingSystemAbstract.hpp"

#include <ftkInterface.h>
#include <ftkOptions.h>
#include <ftkTypes.h>

#include <array>
#include <deque>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

/**
 * \addtogroup adApi SDK C++ API
 * \brief This is a set of classes and functions providing a higher-lever API.
 *
 * The Atracsys SDK C++ API provides a high-level API, with C++ objects. It is
 * \e not meant to replace the standard C API, but is rather meant to provide a
 * more convenient API for C++ programmers.
 *
 * The data structures have been designed to hide any manual allocation and
 * deletion, so that the user can focus on using the tracking device, without
 * spending too much energy on how to use the SDK.
 *
 * The API was thought to allow simple usage for prototyping but also allow
 * full control.
 *
 * The connection to the device, geometry setting and getting a frame can be as
 * simple as that:
 * \code
 * int main()
 * {
 *     atracsys::TrackingSystem wrapper;
 *
 *     if ( wrapper.initialise() != atracsys::Status::Ok )
 *     {
 *         atracsys::reportError( "Cannot initialise wrapper" );
 *     }
 *
 *     if ( wrapper.enumerateDevices() != atracsys::Status::Ok )
 *     {
 *         wrapper.streamLastError( cerr );
 *     }
 *
 *     if ( wrapper.createFrame( false, 10u, 0u, 20u, 4u ) != atracsys::Status::Ok )
 *     {
 *         atracsys::reportError( "Cannot initialise frame" );
 *     }
 *
 *     if ( wrapper.setGeometry( "geometry074.ini" ) != atracsys::Status::Ok )
 *     {
 *         atracsys::reportError( "Cannot set geometry" );
 *     }
 *
 *     atracsys::FrameData frame;
 *
 *     if ( wrapper.getLastFrame( frame ) != atracsys::Status::Ok )
 *     {
 *         wrapper.streamLastError( cerr );
 *     }
 *     else
 *     {
 *         for ( const auto& marker : frame.markers() )
 *         {
 *             cout << "Marker " << marker.index() << " is geometry "
 *                  << marker.geometryId() << endl;
 *             cout << "\t(" << marker.position()[ 0u ] << ", "
 *                  << marker.position()[ 1u ] << ", "
 *                  << marker.position()[ 2u ] << ")" << endl;
 *         }
 *     }
 *     return 0;
 * }
 * \endcode
 * \{
 */

/** \brief Namespace holding the C++ API for the Atracsys Stereo SDK.
 *
 * This namespace contains helper classes, functions and the wrapping class.
 */
namespace atracsys
{
    /** \brief Wrapper class for the stereo SDK.
     *
     * This class provides a C++ interface for the Atracsys Stereo SDK.
     *
     * \warning This class is non-copyable.
     *
     * \warning Accessing global options is currently not possible.
     *
     * The class contains the library handle, the DeviceInfo instances
     * corresponding to the enumerated devices and the allocated
     * ::ftkFrameQuery instances. Each option getter / setter, geometry setter
     * / cleaner and frame getting method has several overloads providing the
     * convenient method for each situation.
     *
     * Unless specified, all \e public methods of this class are thread-safe.
     */
    class TrackingSystem : public TrackingSystemAbstract
    {
    public:
        /** \brief Default constructor, allows to discriminate a simulator.
         *
         * The constructor initialises the instance and allows an optional
         * rejection of detected simulators.
         *
         * \param[in] timeout default timeout value (in ms) when calling
         * ::ftkGetLastFrame.
         * \param[in] allowSimulator set to \e false to prevent to enumerate
         * a simulator.
         */
        TrackingSystem( uint32_t timeout = 100u, bool allowSimulator = true );

        /** \brief Copy constructor, disabled as the class is non-copyable.
         */
        TrackingSystem( const TrackingSystem& ) = delete;

        /** \brief Default implementation of the move constructor.
         *
         * \param[in] other instance to move.
         */
        TrackingSystem( TrackingSystem&& other ) = default;

        /** \brief Destructor, cleans the allocated memory.
         *
         * The destructor is responsible to close the library and releases the
         * created ftkFrameQuery instances.
         */
        virtual ~TrackingSystem() = default;

        /** \brief Assignment operator, disabled as the class is non-copyable.
         */
        TrackingSystem& operator=( const TrackingSystem& ) = delete;

        /** \brief Default implementation of the move assignment operator.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        TrackingSystem& operator=( TrackingSystem&& other ) = default;

        /** \brief Device enumerating method.
         *
         * This method enumerates the connected devices. When a device is
         * detected, its options are automatically retrieved.
         *
         * The ::ftkEnumerateDevices function is called with the
         * #_deviceEnumerator method as callback. The latter then calls
         * ::ftkEnumerateOptions with the #_optionEnumerator method as
         * callback.
         *
         * \retval Status::Ok if the enumeration could be performed
         * successfully,
         * \retval Status::LibNotInitialised if the underlying library handle
         * was not initialised (i.e. #initialise was not called),
         * \retval Status::NoDevices if the enumeration was correctly performed
         * but no devices were detected,
         * \retval Status::SdkWarning if the call to ::ftkEnumerateDevices issued
         * a warning,
         * \retval Status::SdkError if the call to ::ftkEnumerateDevices issued
         * an error.
         */
        Status enumerateDevices() override;

        /** \brief Getter for #_DetectedDevices.
         *
         * This method allows to access #_DetectedDevices.
         *
         * \param[out] infos a copy of the  #_DetectedDevices container.
         *
         * \retval Status::Ok if the getting could be performed successfully,
         * \retval Status::LibNotInitialised if the underlying library handle
         * was not initialised (i.e. #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected).
         */
        Status getEnumeratedDevices( std::vector< DeviceInfo >& infos ) const override;

        /** \brief Getter for the number of enumerated devices.
         *
         * This method allows to access the number of enumerated (i.e. detected
         * devices). It returns the size of the implementation-specific
         * container.
         *
         * \return the size of #_DetectedDevices.
         */
        size_t numberOfEnumeratedDevices() const override;

        /** \brief Getter for the options related to the given device.
         *
         * This method allows to access the options for the device with the
         * given serial number.
         *
         * \param[in] serialNbr serial number of the device for which the
         * options must be retrieved.
         * \param[out] opts options container.
         *
         * \retval Status::Ok if the options could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle
         * was not initialised (i.e. #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected),
         * \retval Status::UnknownDevice if the serial number does not
         * correspond to a known (i.e. enumerated) device.
         */
        Status getEnumeratedOptions( uint64_t serialNbr, std::vector< DeviceOption >& opts ) const override;

        /** \overload Status getEnumeratedOptions( std::vector< DeviceOption >& opts ) const
         *
         * This method allows to access the options for \e the connected
         * device. It first checks there is only one detected device, which
         * serial number is then used in the call to #getEnumeratedOptions.
         *
         * \param[out] opts options container.
         *
         * \retval Status::Ok if the options could be successfully retrieved,
         * \retval Status::LibNotInitialised if the underlying library handle
         * was not initialised (i.e. #initialise was not called),
         * \retval Status::NoDevices if no devices could be gotten (i.e either
         * #enumerateDevices was not called or no devices were detected),
         * \retval Status::SeveralDevices if more than one devices were
         * enumerated,
         * \retval Status::UnknownDevice if the serial number does not
         * correspond to a known (i.e. enumerated) device.
         */
        Status getEnumeratedOptions( std::vector< DeviceOption >& opts ) const override;

    protected:
        /** \brief Callback function for device enumeration.
         *
         * This function is called by ftkEnumerateDevices. The received serial
         * number is looked for in the #_DetectedDevices container, and added
         * if not already present. If the detected device is a new one, its
         * options are automatically retrieved.
         *
         * \param[in] sn serial number of the detected device.
         * \param[in,out] user pointer on the calling TrackingSystem instance.
         * \param[in] type type of the connected device.
         */
        static void _deviceEnumerator( uint64_t sn, void* user, ftkDeviceType type );

        /** \brief Callback function for options enumeration.
         *
         * This function is called by ftkEnumerateOptions. The received serial
         * number is looked for in the #_DetectedDevices container. The options
         * for the concerned device are then stored as DeviceOption instances.
         *
         *
         * \param[in] sn serial number of the detected device.
         * \param[in,out] user pointer on the calling TrackingSystem instance.
         * \param[in] oi pointer on the enumerated option.
         */
        static void _optionEnumerator( uint64_t sn, void* user, ftkOptionsInfo* oi );

        /** \brief Getter for a #_DetectedDeviceProtector.
         *
         * This method allows to get #_DetectedDeviceProtector.
         *
         * \retval #_DetectedDeviceProtector as a reference.
         */
        std::mutex& _enumeratedDevicesProtector() const override;

        /** \brief Getter for the number of enumerated devices.
         *
         * This method allows to get the number of enumerated devices.
         *
         * \retval _DetectedDevices.size() the size of a device container.
         */
        size_t _numberOfEnumeratedDevices() const override;

        /** \brief Method allowing to get the global options.
         *
         * This method creates a fake device with serial number \c 0uLL so that the global options can be
         * retrieved using the existing mechanism.
         *
         * \retval Status::Ok if the global options could e successfully retrieved,
         * \retval Status::SdkWarning if the call to the SDK function issued a warning,
         * \retval Status::SdkError if the call to the SDK function triggered an error.
         */
        Status _populateGlobalOptions() override;

        /** \brief Helper function checking whether the device exist.
         *
         * This function checks whether the given serial number is a valid
         * device.
         *
         * \param[in] serialNbr serial number of the wanted device.
         * \param[out] dev the device information instance.
         * \param[out] idx index of the device in the #_DetectedDevices
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
        Status _getDevice( uint64_t serialNbr, DeviceInfo& dev, size_t& idx ) const override;

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
        Status _getOptionDesc( uint64_t serialNbr,
                               uint32_t optId,
                               ftkOptionType optType,
                               DeviceOption& opt ) const override;

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
        Status _getOptionDesc( uint64_t serialNbr,
                               const std::string& optId,
                               ftkOptionType optType,
                               DeviceOption& opt ) const override;

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
        uint64_t _deviceSerialNumber( size_t i = 0u ) const override;

        /** \brief Container of detected devices.
         */
        std::deque< DeviceInfo > _DetectedDevices;

        /** \brief Fake device, only meant to hold the global options.
         */
        DeviceInfo _GlobalOptionContainer;

        /** \brief Mutex preventing multithreaded access to #_DetectedDevices.
         */
        mutable std::mutex _DetectedDeviceProtector;
    };
}  // namespace atracsys

/**
 * \}
 */

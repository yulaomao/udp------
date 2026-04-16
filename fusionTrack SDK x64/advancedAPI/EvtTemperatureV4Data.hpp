/** \file EvtTemperatureV4Data.hpp
 * \brief File defining all the temperature event (version 4) data structures of the C++ API.
 *
 * This file is a collection of all temperature event (version 4) data structures used in the C++ API.
 */
#pragma once

#include <ftkInterface.h>

#include <vector>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class wrapping a single temperature measurement.
     *
     * This class contains the data of \e one temperature reading. It is actually a wrapper around the
     * ::EvtTemperatureV4Payload structure.
     */
    class TempV4Item : protected EvtTemperatureV4Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         */
        TempV4Item() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        TempV4Item( const TempV4Item& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        TempV4Item( TempV4Item&& other ) = default;

        /** \brief Constructor promoting a ::EvtTemperatureV4Payload instance.
         *
         * This constructor allows to promote a ::EvtTemperatureV4Payload
         * instance.
         *
         * \param[in] other instance to promote.
         */
        TempV4Item( const EvtTemperatureV4Payload& other );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~TempV4Item() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        TempV4Item& operator=( const TempV4Item& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        TempV4Item& operator=( TempV4Item&& other ) = default;

        /** \brief Getter for EvtTemperatureV4Payload::SensorId.
         *
         * \retval EvtTemperatureV4Payload::SensorId.
         */
        uint32 sensorId() const;

        /** \brief Getter for EvtTemperatureV4Payload::SensorValue.
         *
         * \retval EvtTemperatureV4Payload::SensorValue.
         */
        float sensorValue() const;

        /** \brief Comparison operator.
         *
         * This method compares the sensor value and ID.
         *
         * \param[in] other instance to compare.
         *
         * \retval true if the the sensor value and ID match,
         * \retval false if not.
         */
        bool operator==( const TempV4Item& other ) const;
    };

    /** \brief Class providing access to the temperature (v4) event.
     *
     * This class contains all the information about the temperature sent in the FtkEventType::fetTempV4
     * event. The instance does now if it has been properly set (i.e. whether its member values make sense).
     */
    class EvtTemperatureV4Data
    {
    public:
        /** \brief Default constructor.
         *
         * This constructor is needed to have instances in STL containers. The resulting instance is invalid.
         */
        EvtTemperatureV4Data() = default;

        /** \brief Copy constructor, duplicates an instance.
         *
         * This constructor is needed for STL containers.
         *
         * \param[in] other instance to duplicate.
         */
        EvtTemperatureV4Data( const EvtTemperatureV4Data& other ) = default;

        /** \brief Move-constructor.
         *
         * \param[in] other instance to move to the current one.
         */
        EvtTemperatureV4Data( EvtTemperatureV4Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent of type
         * FtkEventType::fetTempV4.
         *
         * This constructor reads the whole payload of the ::ftkEvent and
         * populates the _Sensors container.
         *
         * \param[in] other payload data.
         * \param[in] payload
         */
        EvtTemperatureV4Data( const EvtTemperatureV4Payload& other, uint32 payload );

        /** \brief Destructor, virtual for a proper destruction sequence.
         */
        virtual ~EvtTemperatureV4Data() = default;

        /** \brief Assignment operator.
         *
         * This method allows to dump an instance into the current one.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtTemperatureV4Data& operator=( const EvtTemperatureV4Data& other ) = default;

        /** \brief Move-assignment operator.
         *
         * This method allows to move an instance into the current one.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtTemperatureV4Data& operator=( EvtTemperatureV4Data&& other ) = default;

        /** \brief Getter for #_Sensors.
         *
         * \retval _Sensors as a const reference.
         */
        const std::vector< TempV4Item >& sensors() const;

        /** \brief Getter for #_Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for #_InvalidInstance.
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtTemperatureV4Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtAccelerometerV1Data to be accessed is not
         * defined, either if the index is too big or there are no stored EvtAccelerometerV1Data instances.
         */
        static const EvtTemperatureV4Data _InvalidInstance;

        /** \brief Container for all temperature readings.
         */
        std::vector< TempV4Item > _Sensors{};

        /** Contains \c true if the instance contains valid information.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys

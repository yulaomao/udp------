/** \file EvtSyntheticTemperatureV1Data.hpp
 * \brief File defining all the synthetic temperature event data structures of the C++ API.
 *
 * This file is a collection of all syntheric temperature event data structures used in the C++ API.
 */
#pragma once

#include <ftkEvent.h>
#include <ftkTypes.h>

#include <vector>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class wrapping a single synthetic temperature indication.
     *
     * This class contains the data of \e one synthetic temperature measurement. It is actually a wrapper
     * around the ::EvtSyntheticTemperaturesV1Payload structure.
     */
    class SyntheticTemperatureV1Item : protected EvtSyntheticTemperaturesV1Payload
    {
    public:
        /** \brief Default implementation of the default constructor.
         */
        SyntheticTemperatureV1Item() = default;

        /** \brief Default implementation of the copy-constructor.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         */
        SyntheticTemperatureV1Item( const SyntheticTemperatureV1Item& other ) = default;

        /** \brief Default implementaion of the move-constructor.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         */
        SyntheticTemperatureV1Item( SyntheticTemperatureV1Item&& other ) = default;

        /** \brief Constructor promoting a ::EvtSyntheticTemperaturesV1Payload instance.
         *
         * This constructor allows to promote a ::EvtSyntheticTemperaturesV1Payload instance.
         *
         * \param[in] other instance to promote.
         */
        SyntheticTemperatureV1Item( const EvtSyntheticTemperaturesV1Payload& other );

        /** \brief Default implementation of the destructor.
         */
        ~SyntheticTemperatureV1Item() = default;

        /** \brief Default implementation of the assignment operator.
         *
         * This method allows to dump an instance in the current one.
         *
         * \param[in] other instance to copy in the current one.
         *
         * \retval *this as a reference.
         */
        SyntheticTemperatureV1Item& operator=( const SyntheticTemperatureV1Item& other ) = default;

        /** \brief Default implementation of the move-assignment operator.
         *
         * This method allows to move an instance in the current one.
         *
         * \param[in] other instance to move in the current one.
         *
         * \retval *this as a reference.
         */
        SyntheticTemperatureV1Item& operator=( SyntheticTemperatureV1Item&& other ) = default;

        /** \brief Comparison operator, needed to store instances in STL containers.
         *
         * This method compares the current and reference synthetic temperatures. As they are floats, a
         * tolerance is used.
         *
         * \param[in] other instance to compare to.
         *
         * \retval true if the two stored temperatures of both instances are `the same',
         * \retval false if at least one of the two temperatures is different.
         */
        bool operator==( const SyntheticTemperatureV1Item& other ) const;

        /** \brief Getter for the current value of the synthetic temperature.
         *
         * This method allows to access the current synthetic temperature value.
         *
         * \return the value of the current synthetic temperature.
         */
        float currentTemperature() const;

        /** \brief Getter for the reference value of the synthetic temperature.
         *
         * This method allows to access the reference synthetic temperature value, i.e. the value when the
         * device has reach thermal equilibrium in a room at 20 degree celcius.
         *
         * \return the value of the reference synthetic temperature.
         */
        float referenceTemperature() const;
    };

    /** \brief Class providing access to the synthetic temperature event.
     *
     * This class contains all the information about the synthetic temperatures sent in the
     * FtkEventType::fetSyntheticTemperatureV1 event. The instance does know if it has been properly set (i.e.
     * whether its member values make sense).
     */
    class EvtSyntheticTemperatureV1Data
    {
    public:
        /** \brief Default constructor, creates an invalid instance.
         */
        EvtSyntheticTemperatureV1Data() = default;

        /** \brief Default implementation of the copy-constructor.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         */
        EvtSyntheticTemperatureV1Data( const EvtSyntheticTemperatureV1Data& other ) = default;

        /** \brief Default implementaion of the move-constructor.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         */
        EvtSyntheticTemperatureV1Data( EvtSyntheticTemperatureV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type
         * FtkEventType::fetSyntheticTemperaturesV1.
         *
         * This constructor reads the whole ::EvtSyntheticTemperaturesV1Payload data and populates the
         * _Measurement container.
         *
         * \param[in] other reference on the \e first ::EvtSyntheticTemperaturesV1Payload instance.
         * \param[in] payload ::ftkEvent payload size.
         */
        EvtSyntheticTemperatureV1Data( const EvtSyntheticTemperaturesV1Payload& other, uint32 payload );

        /** \brief Default implementation of the destructor.
         */
        ~EvtSyntheticTemperatureV1Data() = default;

        /** \brief Default implementation of the affectation operator.
         *
         * This method allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtSyntheticTemperatureV1Data& operator=( const EvtSyntheticTemperatureV1Data& other ) = default;

        /** \brief Default implementaion of the move-affectation operator.
         *
         * This method allows to move an instance.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtSyntheticTemperatureV1Data& operator=( EvtSyntheticTemperatureV1Data&& other ) = default;

        /** \brief Getter for _Measurements.
         *
         * This method allows to access _Measurements.
         *
         * \retval _Measurements as a const reference.
         */
        const std::vector< SyntheticTemperatureV1Item > measurements() const;

        /** \brief Getter for _Valid.
         *
         * This method allows to access _Valid.
         *
         * \retval _Valid as value.
         */
        bool valid() const;

        /** \brief Getter for _InvalidInstance.
         *
         * This method allows to access _InvalidInstance
         *
         * \retval _InvalidInstance as a const reference.
         */
        static const EvtSyntheticTemperatureV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtSyntheticTemperatureV1Data to be accessed is not
         * defined, if there are no stored EvtSyntheticTemperatureV1Data instances.
         */
        static const EvtSyntheticTemperatureV1Data _InvalidInstance;

        /** \brief Container for the synthetic temperature measurements.
         */
        std::vector< SyntheticTemperatureV1Item > _Measurements{};

        /** \brief Contains \c true if the instance is valid.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys

/** \file EvtEioTaggingV1Data.hpp
 * \brief File defining the event EIO tagging (version 1) data structure for the C++ API.
 *
 * This file defines the C++ API equivalent of the ::EvtEioTaggingV1Payload structure.
 */
#pragma once

#include <ftkEvent.h>
#include <ftkTypes.h>

/**
 * \addtogroup adApi
 * \{
 */

namespace atracsys
{
    class FrameData;

    /** \brief Class providing access to the EIO tagging event.
     *
     * This class contains the data of \e a EIO tagging event. It is actually a wrapper around the
     * ::EvtEioTaggingV1Payload structure.
     */
    class EvtEioTaggingV1Data : protected EvtEioTaggingV1Payload
    {
    public:
        /** \brief Used tagging mode.
         */
        enum class TaggingMode : uint32_t
        {
            /** \brief No tagging on the port.
             */
            Disabled = 0u,
            /** \brief Single tagging on the port.
             */
            Single = 1u,
            /** \brief Both ports used in dual mode.
             */
            Dual = 2u
        };

        /** \brief Default implementation of the default constructor.
         *
         * This constructor builds an invalid instance.
         */
        EvtEioTaggingV1Data() = default;

        /** \brief Default implementation of the copy-constructor.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         */
        EvtEioTaggingV1Data( const EvtEioTaggingV1Data& other ) = default;

        /** \brief Default implementation of the move-constructor.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         */
        EvtEioTaggingV1Data( EvtEioTaggingV1Data&& other ) = default;

        /** \brief Constructor promoting a ::ftkEvent instance of type FtkEventType::fetEioTaggingV1.
         *
         * This constructor reads the whole ::EvtEioTaggingV1Payload data and populates the members.
         *
         * \param[in] other ::EvtEioTaggingV1Payload instance.
         */
        EvtEioTaggingV1Data( const EvtEioTaggingV1Payload& other );

        /**\brief Default implementation of the destructor.
         */
        virtual ~EvtEioTaggingV1Data() = default;

        /** \brief Default implementation of the affectation operator.
         *
         * This constructor allows to duplicate an instance.
         *
         * \param[in] other instance to copy.
         *
         * \retval *this as a reference.
         */
        EvtEioTaggingV1Data& operator=( const EvtEioTaggingV1Data& other ) = default;

        /** \brief Default implementation of the move-affectation operator.
         *
         * This constructor allows to move an instance.
         *
         * \param[in] other instance to move.
         *
         * \retval *this as a reference.
         */
        EvtEioTaggingV1Data& operator=( EvtEioTaggingV1Data&& other ) = default;

        /** \brief Getter for the tag ID for the given EIO port.
         *
         * Allows to access the last tag ID for the wanted EIO port.
         *
         * \param[in] portNumber EIO port number, valid values are \c 0u and \c 1u.
         * \param[out] value tag ID value.
         *
         * \retval true if the value could be retrieved,
         * \retval false if the \c portNumber is not valid, or if no tags are read for the given port.
         */
        bool tagId( const size_t portNumber, uint32_t& value ) const;

        /** \brief Getter for the tag mode for the given EIO port.
         *
         * Allows to access the tagging mode for the wanted EIO port.
         *
         * \param[in] portNumber EIO port number, valid values are \c 0u and \c 1u.
         * \param[out] value tagging mode value.
         *
         * \retval true if the value could be retrieved,
         * \retval false if the \c portNumber is not valid.
         */
        bool tagMode( const size_t portNumber, TaggingMode& value ) const;

        /** \brief Getter for the timestamp for the given EIO port.
         *
         * Allows to access the timestamp corresponding to the last tag ID for the wanted EIO port.
         *
         * \param[in] portNumber EIO port number, valid values are \c 0u and \c 1u.
         * \param[out] value timestamp.
         *
         * \retval true if the value could be retrieved,
         * \retval false if the \c portNumber is not valid, or if no tags are read for the given port.
         */
        bool timestamp( const size_t portNumber, uint64_t& value ) const;

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
        static const EvtEioTaggingV1Data& invalidInstance();

    protected:
        /** \brief Invalid instance.
         *
         * This instance is used as return value when the EvtEioTaggingV1Data to be accessed is not
         * defined.
         */
        static const EvtEioTaggingV1Data _InvalidInstance;

        /** \brief Contains \c true if the instance is valid.
         */
        bool _Valid{ false };
    };
}  // namespace atracsys

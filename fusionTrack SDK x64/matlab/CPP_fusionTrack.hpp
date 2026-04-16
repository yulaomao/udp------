#pragma once

#include <ftkInterface.h>

#include <string>
#include <vector>

/** \brief The FusionTrack class is a light wrapper on the C API.
 *
 * This class defines a lightweight interface with minimal functionality, used
 * in the compiled `mex function'.
 */
class FusionTrack
{
public:
    /** \brief Maximal number of retrieved 3D fiducial data.
     */
    static constexpr uint32 cstMax3DFiducials{ 256u };

    /** \brief Maximal number of retrieved marker data.
     */
    static constexpr uint32 cstMaxMarkers{ 16u };

    /** \brief Default implementation of the default constructor.
     */
    FusionTrack() = default;

    /** \brief Disabled copy-constructor, as the class is non-copyable.
     */
    FusionTrack( const FusionTrack& ) = delete;

    /** \brief Default implementation of the move-constructor.
     *
     * \param[in] other instance to move.
     */
    FusionTrack( FusionTrack&& other ) = default;

    /** \brief Destructor, releases the allocated memory.
     *
     * The destructor is responsible for closing the library handle.
     */
    virtual ~FusionTrack();

    /** \brief Disabled affectation operator, as the class is non-copyable.
     */
    FusionTrack& operator=( const FusionTrack& ) = delete;

    /** \brief Default implementation of the move-affectation operator.
     *
     * \param[in] other instance to move.
     *
     * \retval *this as a reference.
     */
    FusionTrack& operator=( FusionTrack&& other ) = default;

    /** \brief Initialising method.
     *
     * This method initialises the library handle, creates the frame and sets it
     * to retrieve markers and fiducial data.
     *
     * \param[in] configurationFile path to a valid JSON configuration file.
     *
     * \retval true if the library and frame could be initialised,
     * \retval false if either the library could not be initialised or if a
     * problem occurred when allocating the frame.
     */
    bool initialise( const std::string& configurationFile = "" );

    /** \brief Getter for the list of detected devices.
     *
     * This method calls ftkEnumerateDevices and populates a std::vector with
     * the serial numbers.
     *
     * \return a container with the serial numbers of the detected devices.
     */
    std::vector< uint64 > devices();

    /** \brief Getter for the registered geometries.
     *
     * This method allows to get the list of registered geometries for the given
     * device. It internall calls ftkEnumerateGeometries.
     *
     * \param[in] device serial number of the wanted device.
     *
     * \return a container with all the geometries registered for the given
     * device.
     */
    std::vector< ftkRigidBody > geometries( uint64 device );

    /** \brief Method clearing a geometry for the given device.
     *
     * This method calls ftkClearGeometry with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] id geometry ID to clear.
     */
    void clearGeometry( uint64 device, uint32 id );

    /** \brief Method registering a geometry for the given device.
     *
     * This method calls ftkSetGeometry with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] in geometry instance to register.
     */
    void setGeometry( uint64 device, const ftkRigidBody& in );

    /** \brief Method to get a geometry from file.
     *
     * This method calls ftkLoadRigidBodyFromFile after getting
     * content of geometry file to return a ftkRigidBody.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] fileName Path to the geometry file
     * \param[out] geometryOut ftkRigidBody corresponding to geometry file.
     */
    void loadGeometry( uint64 device, const std::string& fileName, ftkRigidBody& geometryOut );

    /** \brief Getter for the available options.
     *
     * This method calls ftkEnumerateOptions and populates a std::vector with
     * the retrieved options.
     *
     * \param[in] device serial number of the wanted device.
     *
     * \return a container with the options of the wanted devices.
     */
    std::vector< ftkOptionsInfo > options( uint64 device );

    /** \brief Getter for an \c int32 option.
     *
     * This method calls ftkGetInt32 with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     * \param[in] what determines wether the current value, minimal / maximal
     * / default value must be retrieved.
     *
     * \return the value of the option.
     */
    int32 getInt32( uint64 device, uint32 optId, ftkOptionGetter what );

    /** \brief Setter for an \c int32 option.
     *
     * This method calls ftkSetInt32 with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     * \param[in] val new value for the option.
     */
    void setInt32( uint64 device, uint32 optId, int32 val );

    /** \brief Getter for an \c float32 option.
     *
     * This method calls ftkGetFloat32 with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     * \param[in] what determines wether the current value, minimal / maximal
     * / default value must be retrieved.
     *
     * \return the value of the option.
     */
    float32 getFloat32( uint64 device, uint32 optId, ftkOptionGetter what );

    /** \brief Setter for an \c float32 option.
     *
     * This method calls ftkSetFloat32 with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     * \param[in] val new value for the option
     */
    void setFloat32( uint64 device, uint32 optId, float32 val );

    /** \brief Getter for an \c ftkBuffer option.
     *
     * This method calls ftkGetData with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     *
     * \return the value of the option.
     */
    std::string getData( uint64 device, uint32 optId );

    /** \brief Setter for an \c float32 option.
     *
     * This method calls ftkSetFloat32 with the given arguments.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] optId unique ID of the option.
     * \param[in] data new value for the option
     */
    void setData( uint64 device, uint32 optId, const std::string &data );

    /** \brief Frame getting method.
     *
     * This method calls ftkGetLastFrame for the given device.
     *
     * \param[in] device serial number of the wanted device.
     * \param[in] timeout timeout in milliseconds.
     */
    void getLastFrame( uint64 device, uint32 timeout = 100u );

    /** \brief Getter for #frame.
     *
     * This method allows to access #frame.
     *
     * \retval frame as a const pointer.
     */
    const ftkFrameQuery& getFrame() const;

protected:
    /** \brief Library handle.
     */
    ftkLibrary handle = nullptr;

    /** \brief Frame used to get data.
     */
    ftkFrameQuery* frame = nullptr;

    /** \brief Helper function which handles errors and warnings.
     *
     * This function calls ftkGetLastErrorString if the status is different
     * from fTkError::FTK_OK. For errors, this breaks the execution, whilst
     * warnings are simply printed.
     *
     * \param[in] err status code to check.
     */
    void check( ftkError err );

private:
    /** \brief Callback for the device enumeration.
     *
     * The callback populates the userdata std::vector with the retrieved serial
     * numbers.
     *
     * \param[in] sn serial number of the enumerated device.
     * \param[in] user pointer on a \c std::vector<uint64_t>.
     */
    static void devicesCallback( uint64 sn, void* user, ftkDeviceType );

    /** \brief Callback for the geometry enumeration.
     *
     * This callback populates the userdata std::vector with the retrieved
     * geometry instances.
     *
     * \param[in] sn serial number of the enumerated device.
     * \param[in] user pointer on a \c std::vector<ftkRigidBody>.
     * \param[in] in pointer on the enumerated geometry.
     */
    static void geometriesCallback( uint64 sn, void* user, ftkRigidBody* in );

    /** \brief Callback for the geometry enumeration.
     *
     * This callback populates the userdata std::vector with the retrieved
     * ftkOptionInfo instances.
     *
     * \param[in] sn serial number of the enumerated device.
     * \param[in] user pointer on a \c std::vector<ftkRigidBody>.
     * \param[in] in pointer on the enumerated option.
     */
    static void optionsCallback( uint64 sn, void* user, ftkOptionsInfo* in );
};

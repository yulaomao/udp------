#include "TrackingSystem.hpp"

#include <ftkPlatform.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
#include <thread>

using namespace std;

namespace atracsys
{
    // --------------------------------------------------------------------- //
    //                                                                       //
    //                     Tracking system definitions                       //
    //                                                                       //
    // --------------------------------------------------------------------- //

    TrackingSystem::TrackingSystem( uint32_t timeout, bool allowSimulator )
        : TrackingSystemAbstract( timeout, allowSimulator )
        , _DetectedDevices()
        , _DetectedDeviceProtector()
    {
    }

    Status TrackingSystem::enumerateDevices()
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _DetectedDeviceProtector, defer_lock );
        lock( lock1, lock2 );
        if ( _Library == nullptr )
        {
            return Status::LibNotInitialised;
        }

        ftkError err( ftkEnumerateDevices( _Library, _deviceEnumerator, this ) );

        if ( err > ftkError::FTK_OK )
        {
            streamLastError( cerr );
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return _DetectedDevices.empty() ? Status::NoDevices : Status::Ok;
    }

    Status TrackingSystem::getEnumeratedDevices( vector< DeviceInfo >& infos ) const
    {
        lock_guard< mutex > lock( _DetectedDeviceProtector );
        if ( _DetectedDevices.empty() )
        {
            return Status::NoDevices;
        }
        infos.assign( _DetectedDevices.cbegin(), _DetectedDevices.cend() );

        return Status::Ok;
    }

    size_t TrackingSystem::numberOfEnumeratedDevices() const
    {
        lock_guard< mutex > lock( _DetectedDeviceProtector );
        return _DetectedDevices.size();
    }

    Status TrackingSystem::getEnumeratedOptions( uint64_t serialNbr, vector< DeviceOption >& opts ) const
    {
        lock_guard< mutex > lock( _DetectedDeviceProtector );
        DeviceInfo dev;
        size_t idx;
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            opts = dev._Options;
        }

        return ret;
    }

    Status TrackingSystem::getEnumeratedOptions( vector< DeviceOption >& opts ) const
    {
        lock_guard< mutex > lock( _DetectedDeviceProtector );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( _DetectedDevices.empty() )
        {
            serialNbr = 0uLL;
        }
        else if ( _DetectedDevices.size() > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceInfo dev;
        size_t idx;
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            opts = dev._Options;
        }

        return ret;
    }

    void TrackingSystem::_deviceEnumerator( uint64_t sn, void* user, ftkDeviceType type )
    {
        TrackingSystem* instance( reinterpret_cast< TrackingSystem* >( user ) );
        if ( instance == nullptr )
        {
            return;
        }

        auto pos( find_if( instance->_DetectedDevices.cbegin(),
                           instance->_DetectedDevices.cend(),
                           [&sn]( const DeviceInfo& info ) { return info._SerialNumber == sn; } ) );

        if ( pos == instance->_DetectedDevices.cend() )
        {
            if ( ( instance->_AllowSimulator || type != ftkDeviceType::DEV_SIMULATOR ) )
            {
                instance->_DetectedDevices.emplace_back( sn, type );
                cout << "Detected a ";
                switch ( type )
                {
                case ftkDeviceType::DEV_FUSIONTRACK_500:
                    cout << "fTk500";
                    break;

                case ftkDeviceType::DEV_FUSIONTRACK_250:
                    cout << "fTk250";
                    break;

                case ftkDeviceType::DEV_SPRYTRACK_180:
                    cout << "sTk180";
                    break;

                case ftkDeviceType::DEV_SPRYTRACK_300:
                    cout << "sTk300";
                    break;

                default:
                    cout << "not supported device";
                }
                cout << " with serial number 0x" << setw( 16u ) << setfill( '0' ) << hex << sn << dec << endl;
                ftkEnumerateOptions( instance->_Library, sn, _optionEnumerator, user );
            }
            else if ( type == ftkDeviceType::DEV_SIMULATOR )
            {
                cerr << "This software is not compatible with the simulator" << endl;
            }
        }
        else
        {
            cout << "Detected ";
            switch ( type )
            {
            case ftkDeviceType::DEV_FUSIONTRACK_500:
                cout << "fTk500";
                break;

            case ftkDeviceType::DEV_FUSIONTRACK_250:
                cout << "fTk250";
                break;

            case ftkDeviceType::DEV_SPRYTRACK_180:
                cout << "sTk180";
                break;

            case ftkDeviceType::DEV_SPRYTRACK_300:
                cout << "sTk300";
                break;

            default:
                cout << "not supported device";
            }
            cout << " with serial number 0x" << setw( 16u ) << setfill( '0' ) << hex << sn << dec << " again"
                 << endl;
        }
    }

    void TrackingSystem::_optionEnumerator( uint64_t sn, void* user, ftkOptionsInfo* oi )
    {
        TrackingSystem* instance( reinterpret_cast< TrackingSystem* >( user ) );
        if ( instance == nullptr )
        {
            return;
        }

        auto pos( find_if( instance->_DetectedDevices.begin(),
                           instance->_DetectedDevices.end(),
                           [&sn]( const DeviceInfo& info ) { return info._SerialNumber == sn; } ) );

        if ( pos == instance->_DetectedDevices.end() )
        {
            return;
        }

        DeviceOption opt( oi->id, oi->component, oi->status, oi->type, oi->name, oi->description, oi->unit );

        pos->_Options.emplace_back( opt );
    }

    mutex& TrackingSystem::_enumeratedDevicesProtector() const
    {
        return _DetectedDeviceProtector;
    }

    size_t TrackingSystem::_numberOfEnumeratedDevices() const
    {
        return _DetectedDevices.size();
    }

    Status TrackingSystem::_populateGlobalOptions()
    {
        if ( find_if( _DetectedDevices.cbegin(), _DetectedDevices.cend(), []( const DeviceInfo& info ) {
                 return info._SerialNumber == 0uLL;
             } ) == _DetectedDevices.cend() )
        {
            _DetectedDevices.emplace_back( 0uLL, ftkDeviceType::DEV_UNKNOWN_DEVICE );
        }

        ftkError err( ftkEnumerateOptions( _Library, 0uLL, _optionEnumerator, this ) );
        if ( err != ftkError::FTK_OK && err != ftkError::FTK_WAR_OPT_GLOBAL_ONLY )
        {
            return ( err > ftkError::FTK_OK ) ? Status::SdkError : Status::SdkWarning;
        }

        _GlobalOptionContainer = std::move( _DetectedDevices.front() );
        _DetectedDevices.pop_back();

        return Status::Ok;
    }

    Status TrackingSystem::_getDevice( uint64_t serialNbr, DeviceInfo& dev, size_t& idx ) const
    {
        if ( _Library == nullptr )
        {
            return Status::LibNotInitialised;
        }
        if ( serialNbr == 0uLL )
        {
            dev = _GlobalOptionContainer;
            idx = numeric_limits< size_t >::max();
            return Status::Ok;
        }
        if ( _DetectedDevices.empty() )
        {
            return Status::NoDevices;
        }

        auto pos( find_if(
          _DetectedDevices.cbegin(), _DetectedDevices.cend(), [&serialNbr]( const DeviceInfo& info ) {
              return info._SerialNumber == serialNbr;
          } ) );

        if ( pos == _DetectedDevices.cend() )
        {
            return Status::UnknownDevice;
        }

        dev = *pos;
        idx = static_cast< size_t >( pos - _DetectedDevices.cbegin() );

        return Status::Ok;
    }

    Status TrackingSystem::_getOptionDesc( uint64_t serialNbr,
                                           uint32_t optId,
                                           ftkOptionType optType,
                                           DeviceOption& opt ) const
    {
        DeviceInfo dev;
        size_t idx;
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            auto theOpt( find_if(
              dev._Options.cbegin(), dev._Options.cend(), [&optId, &optType]( const DeviceOption& opt ) {
                  return opt.id() == optId && opt.type() == optType;
              } ) );

            if ( theOpt == dev._Options.cend() )
            {
                return Status::InvalidOption;
            }

            opt = *theOpt;
        }

        return ret;
    }

    Status TrackingSystem::_getOptionDesc( uint64_t serialNbr,
                                           const string& optId,
                                           ftkOptionType optType,
                                           DeviceOption& opt ) const
    {
        DeviceInfo dev;
        size_t idx;
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            auto theOpt( find_if(
              dev._Options.cbegin(), dev._Options.cend(), [&optId, &optType]( const DeviceOption& opt ) {
                  return opt.name() == optId && opt.type() == optType;
              } ) );

            if ( theOpt == dev._Options.cend() )
            {
                return Status::InvalidOption;
            }

            opt = *theOpt;
        }

        return ret;
    }

    uint64_t TrackingSystem::_deviceSerialNumber( size_t i ) const
    {
        if ( i >= _DetectedDevices.size() )
        {
            return 0uLL;
        }

        return _DetectedDevices.at( i ).serialNumber();
    }
}  // namespace atracsys

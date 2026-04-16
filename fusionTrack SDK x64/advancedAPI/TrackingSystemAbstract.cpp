#include "TrackingSystemAbstract.hpp"

#include <ftkInterface.h>

#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <thread>

#ifdef ATR_WIN
#include <conio.h>
#include <windows.h>
#elif defined( ATR_LIN ) || defined( ATR_OSX )
#include <cstdio>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

namespace atracsys
{
    // --------------------------------------------------------------------- //
    //                                                                       //
    //                      Internal utility functions                       //
    //                                                                       //
    // --------------------------------------------------------------------- //
    int detectKeyboardHit()
    {
#ifdef ATR_WIN
        if ( _kbhit() == 0 )
        {
            return 0;
        }
        else
        {
            char ch1( _getch() );
            return 1;
        }
#elif defined( ATR_LIN ) || defined( ATR_OSX )
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr( STDIN_FILENO, &oldt );
        newt = oldt;
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr( STDIN_FILENO, TCSANOW, &newt );
        oldf = fcntl( STDIN_FILENO, F_GETFL, 0 );
        fcntl( STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK );

        ch = getchar();

        tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
        fcntl( STDIN_FILENO, F_SETFL, oldf );

        if ( ch != EOF )
        {
            ungetc( ch, stdin );
            return 1;
        }

        return 0;

#else
#error "Not implemented"
#endif
    }

    /** \brief Helper class holding whether the software is launched from
     * explorer.
     *
     * This class simply defines a static method checking whether the sofware
     * was launched from a console or explorer (only valid on windows).
     *
     * The way it is done ensures the check is performed before any call to
     * cout / cerr / printf function, which would change the outcome.
     *
     * \warning This class is non-copyable.
     */
    class ConsoleDetector
    {
    public:
        /** \brief Default implementation for default constructor.
         */
        ConsoleDetector() = default;

        /** \brief Copy-constructor, deleted as the class is non-copyable.
         */
        ConsoleDetector( const ConsoleDetector& ) = delete;

        /** \brief Move-constructor, deleted as the class is non-copyable.
         */
        ConsoleDetector( ConsoleDetector&& ) = delete;

        /** \brief Destructor, default implementation.
         */
        ~ConsoleDetector() = default;

        /** \brief Assignment operator, deleted as the class is non-copyable.
         */
        ConsoleDetector& operator=( const ConsoleDetector& ) = delete;

        /** \brief Move-assignment operator, deleted as the class is
         * non-copyable.
         */
        ConsoleDetector& operator=( ConsoleDetector&& ) = delete;

        /** \brief Getter for ConsoleDetector::_FromExplorer.
         *
         * This method allows to access ConsoleDetector::_FromExplorer.
         *
         * \return the value of ConsoleDetector::_FromExplorer.
         */
        static bool fromExplorer();

    private:
        /** \brief Method detecting whether the sofware is launched from a
         * console or not.
         *
         * On Windows, This method detects if the running software was launched
         * from explorer or from a console. On Unices, the answer is always no.
         *
         * This method is called to initialise the
         * ConsoleDetector::_FromExplorer static member.
         *
         * \retval false if the software was launched from the console,
         * \retval true if not.
         */
        static bool _isLaunchedFromExplorer();

        /** \brief Contains \c true if the software was \e not launched from a
         * console.
         */
        static bool _FromExplorer;
    };

    bool ConsoleDetector::_FromExplorer( _isLaunchedFromExplorer() );

    bool ConsoleDetector::fromExplorer()
    {
        return _FromExplorer;
    }

    bool ConsoleDetector::_isLaunchedFromExplorer()
    {
#ifdef ATR_WIN
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if ( !GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &csbi ) )
        {
            cout << "GetConsoleScreenBufferInfo failed: " << GetLastError() << endl;
            return false;
        }

        // if cursor position is (0,0) then we were launched in a separate console
        return ( ( !csbi.dwCursorPosition.X ) && ( !csbi.dwCursorPosition.Y ) );
#elif defined( ATR_LIN )
        return false;
#endif
    }

    // --------------------------------------------------------------------- //
    //                                                                       //
    //                      Device option definitions                        //
    //                                                                       //
    // --------------------------------------------------------------------- //

    DeviceOption::DeviceOption( uint32_t id,
                                ftkComponent component,
                                ftkOptionStatus status,
                                ftkOptionType type,
                                const std::string& name,
                                const std::string& desc,
                                const std::string& unit )
        : _Id( id )
        , _Component( component )
        , _Status( status )
        , _Type( type )
        , _Name( name )
        , _Description( desc )
        , _Unit( unit )
    {
    }

    bool DeviceOption::operator==( const DeviceOption& other ) const
    {
        if ( _Id != other._Id )
        {
            return false;
        }
        else if ( _Component != other._Component )
        {
            return false;
        }
        else if ( _Status.read != other._Status.read )
        {
            return false;
        }
        else if ( _Status.write != other._Status.write )
        {
            return false;
        }
        else if ( _Status.accessProtected != other._Status.accessProtected )
        {
            return false;
        }
        else if ( _Status.accessPrivate != other._Status.accessPrivate )
        {
            return false;
        }
        else if ( _Name != other._Name )
        {
            return false;
        }
        else if ( _Description != other._Description )
        {
            return false;
        }
        else if ( _Unit != other._Unit )
        {
            return false;
        }

        return true;
    }

    uint32_t DeviceOption::id() const
    {
        return _Id;
    }

    ftkComponent DeviceOption::component() const
    {
        return _Component;
    }

    ftkOptionStatus DeviceOption::status() const
    {
        return _Status;
    }

    ftkOptionType DeviceOption::type() const
    {
        return _Type;
    }

    const string& DeviceOption::name() const
    {
        return _Name;
    }

    const string& DeviceOption::description() const
    {
        return _Description;
    }

    const string& DeviceOption::unit() const
    {
        return _Unit;
    }

    // --------------------------------------------------------------------- //
    //                                                                       //
    //                       Device info definitions                         //
    //                                                                       //
    // --------------------------------------------------------------------- //

    DeviceInfo::DeviceInfo( uint64_t sn, ftkDeviceType type )
        : _SerialNumber( sn )
        , _Type( type )
        , _Options()
    {
    }

    bool DeviceInfo::operator==( const DeviceInfo& other ) const
    {
        return _SerialNumber == other.serialNumber() && _Type == other.type();
    }

    uint64_t DeviceInfo::serialNumber() const
    {
        return _SerialNumber;
    }

    ftkDeviceType DeviceInfo::type() const
    {
        return _Type;
    }

    const vector< DeviceOption >& DeviceInfo::options() const
    {
        return _Options;
    }

    // --------------------------------------------------------------------- //
    //                                                                       //
    //                      Exported utility functions                       //
    //                                                                       //
    // --------------------------------------------------------------------- //

    void waitForKeyboardHit()
    {
        while ( detectKeyboardHit() == 0 )
        {
            this_thread::sleep_for( chrono::milliseconds( 200 ) );
        }
    }

    void reportError( const string& message )
    {
        cerr << message << endl;

        continueOnUserInput();

        exit( 1 );
    }

    void continueOnUserInput( const string& what )
    {
        if ( ConsoleDetector::fromExplorer() )
        {
            cout << "Press the \"ANY\" key to " << what << endl;
            waitForKeyboardHit();
        }
    }

    Status loadGeometry( const string& fileName, ftkGeometry& geom, ostream& out )
    {
        ifstream geomFile( fileName.c_str() );
        if ( !geomFile.is_open() )
        {
            out << "File '" << fileName << "' does not exist" << endl;
            return Status::InvalidFile;
        }

        string line;

        getline( geomFile, line );

        regex sectionMatcher( R"x(\[([^\]]+)\])x" );
        regex attributeMatcher( "(.+)=(.+)", regex::extended );
        regex commentMatcher( "^;.+", regex::extended );

        string currentSection( "" ), currentKey, currentValue;

        smatch sm;
        smatch::const_iterator smIt;

        vector< tuple< string, string, string > > parsedInfo;

        while ( !line.empty() && !geomFile.eof() )
        {
            // Linux handling of Windows \r\n let the \r in the line...
            if ( line.size() && line[ line.size() - 1 ] == '\r' )
            {
                line = line.substr( 0, line.size() - 1 );
            }
            if ( regex_match( line, sm, sectionMatcher ) )
            {
                currentSection = *( ++sm.begin() );
            }
            else if ( regex_match( line, sm, attributeMatcher ) )
            {
                smIt = sm.begin();
                currentKey = *( ++smIt );
                currentValue = *( ++smIt );
                parsedInfo.emplace_back( currentSection, currentKey, currentValue );
            }
            else if ( !regex_match( line, commentMatcher ) )
            {
                out << "Regex error processing line '" << line << "'" << endl;
                return Status::ParsingError;
            }
            getline( geomFile, line );
        }

        uint32_t tmp;
        geom.version = 0u;
        if ( !readValueFromIniFile( parsedInfo, "geometry", "count", tmp, out ) )
        {
            out << "File '" << fileName << "' does not contain /geometry/count" << endl;
            return Status::ParsingError;
        }
        geom.pointsCount = tmp;
        if ( !readValueFromIniFile( parsedInfo, "geometry", "id", geom.geometryId, out ) )
        {
            out << "File '" << fileName << "' does not contain /geometry/id" << endl;
            return Status::ParsingError;
        }

        stringstream convert;

        for ( uint32_t i( 0u ); i < geom.pointsCount; ++i )
        {
            convert << "fiducial" << i;
            if ( !readValueFromIniFile( parsedInfo, convert.str(), "x", geom.positions[ i ].x, out ) )
            {
                out << "File '" << fileName << "' does not contain /geometry/fiducial" << i << "/x" << endl;
                return Status::ParsingError;
            }
            if ( !readValueFromIniFile( parsedInfo, convert.str(), "y", geom.positions[ i ].y, out ) )
            {
                out << "File '" << fileName << "' does not contain /geometry/fiducial" << i << "/y" << endl;
                return Status::ParsingError;
            }
            if ( !readValueFromIniFile( parsedInfo, convert.str(), "z", geom.positions[ i ].z, out ) )
            {
                out << "File '" << fileName << "' does not contain /geometry/fiducial" << i << "/z" << endl;
                return Status::ParsingError;
            }
            convert.str( "" );
        }

        return Status::Ok;
    }

    const string TrackingSystemAbstract::_ErrorOpenTag( "<errors>" );
    const string TrackingSystemAbstract::_ErrorCloseTag( "</errors>" );
    const string TrackingSystemAbstract::_WarningOpenTag( "<warnings>" );
    const string TrackingSystemAbstract::_WarningCloseTag( "</warnings>" );
    const string TrackingSystemAbstract::_MessageOpenTag( "<messages>" );
    const string TrackingSystemAbstract::_MessageCloseTag( "</messages>" );

    TrackingSystemAbstract::TrackingSystemAbstract( uint32_t timeout, bool allowSimulator )
        : _DefaultTimeout( timeout )
        , _DefaultTimeoutProtector()
        , _AllowSimulator( allowSimulator )
        , _Level( LogLevel::Info )
        , _LevelProtector()
        , _Library( nullptr )
        , _LibraryProtector()
        , _AllocatedFrames()
        , _AllocatedFramesProtector()
        , _StreamLocks()
        , _StreamLocksProtector()
    {
    }

    TrackingSystemAbstract::~TrackingSystemAbstract()
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        lock( lock1, lock2 );

        ftkError err( ftkError::FTK_OK );

        for ( auto& ptr : _AllocatedFrames )
        {
            err = ftkDeleteFrame( ptr );
            if ( err != ftkError::FTK_OK )
            {
                streamLastError();
            }
            ptr = nullptr;
        }

        if ( _Library != nullptr )
        {
            if ( ftkClose( &_Library ) != ftkError::FTK_OK )
            {
                cerr << "Cannot close library" << endl;
            }
        }
    }

    Status TrackingSystemAbstract::initialise( const string& fileName, ostream& out )
    {
        lock_guard< mutex > lock( _LibraryProtector );

        const char* fileNameC( fileName.empty() ? nullptr : fileName.c_str() );
        ftkBuffer errs;
        _Library = ftkInitExt( fileNameC, &errs );

        if ( errs.size > 0u )
        {
            out << errs.data << endl;
        }

        if ( _Library == nullptr )
        {
            return Status::LibNotInitialised;
        }

        return _populateGlobalOptions();
    }

    bool TrackingSystemAbstract::sdkVersion( std::string& version ) const
    {
        lock_guard< mutex > lock( _LibraryProtector );
        if ( _Library == nullptr )
        {
            return false;
        }

        ftkBuffer buffer{};

        ftkVersion( &buffer );

        version = move( string( buffer.data, buffer.size ) );

        return true;
    }

    uint32_t TrackingSystemAbstract::defaultTimeout() const
    {
        lock_guard< mutex > lock( _DefaultTimeoutProtector );
        return _DefaultTimeout;
    }

    void TrackingSystemAbstract::setDefaultTimeout( uint32_t value )
    {
        lock_guard< mutex > lock( _DefaultTimeoutProtector );
        _DefaultTimeout = value;
    }

    bool TrackingSystemAbstract::allowSimulator() const
    {
        lock_guard< mutex > lock( _LibraryProtector );
        return _AllowSimulator;
    }

    void TrackingSystemAbstract::setAllowSimulator( bool value )
    {
        lock_guard< mutex > lock( _LibraryProtector );
        _AllowSimulator = value;
    }

    LogLevel TrackingSystemAbstract::level() const
    {
        lock_guard< mutex > lock( _LevelProtector );
        return _Level;
    }

    void TrackingSystemAbstract::setLevel( LogLevel value )
    {
        lock_guard< mutex > lock( _LevelProtector );
        _Level = value;
    }

    Status TrackingSystemAbstract::loadGeometry( const string& fileName, ftkRigidBody& geom )
    {
        lock_guard< mutex > lock( _LibraryProtector );
        if ( _Library == nullptr )
        {
            return Status::LibNotInitialised;
        }

        ifstream geomFile( fileName, ios::binary | ios::ate );
        if ( !geomFile.is_open() || geomFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not open file '" << fileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        size_t pos( geomFile.tellg() );

        if ( pos > sizeof( ftkBuffer::data ) )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "File '" << fileName << "' is too big (max size is " << sizeof( ftkBuffer::data )
                     << ")" << endl;
            }
            return Status::InvalidFile;
        }

        unique_ptr< ftkBuffer > buffer( new ( nothrow ) ftkBuffer() );
        if ( buffer == nullptr )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not allocate internal buffer" << endl;
            }
            return Status::AllocationIssue;
        }

        geomFile.seekg( 0u, ios::beg );
        geomFile.read( buffer->data, pos );
        buffer->size = static_cast< uint32_t >( pos );
        if ( geomFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Error reading file '" << fileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        ftkError err( ftkLoadRigidBodyFromFile( _Library, buffer.get(), &geom ) );

        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::saveGeometry( const std::string& fileName, const ftkRigidBody& geom )
    {
        lock_guard< mutex > lock( _LibraryProtector );
        if ( _Library == nullptr )
        {
            return Status::LibNotInitialised;
        }

        ofstream geomFile( fileName, ios::binary );
        if ( !geomFile.is_open() || geomFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not open file '" << fileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        unique_ptr< ftkBuffer > buffer( new ( nothrow ) ftkBuffer() );
        if ( buffer == nullptr )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not allocate internal buffer" << endl;
            }
            return Status::AllocationIssue;
        }

        ftkError err( ftkSaveRigidBodyToFile( _Library, &geom, buffer.get() ) );

        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        geomFile.write( buffer->data, buffer->size );

        if ( geomFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Error writing buffer in file" << endl;
            }
            return Status::InvalidFile;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::convertGeometryFile( const string& inputFileName,
                                                        const string& outputFileName )
    {
        ifstream inputFile( inputFileName, ios::binary | ios::ate );
        if ( !inputFile.is_open() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not open input file '" << inputFileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        streampos endPos( inputFile.tellg() );

        unique_ptr< ftkBuffer > inputBuffer( new ( nothrow ) ftkBuffer{} );
        if ( inputBuffer == nullptr )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not allocate reading buffer" << endl;
            }
            return Status::AllocationIssue;
        }

        inputFile.seekg( 0u, ios::beg );

        inputFile.read( inputBuffer->data, endPos );
        inputBuffer->size = static_cast< uint32_t >( endPos );

        if ( inputFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not read input file '" << inputFileName << "'" << endl;
            }
            return Status::InvalidFile;
        }
        inputBuffer->size = static_cast< uint32_t >( endPos );

        inputFile.close();

        unique_ptr< ftkBuffer > outputBuffer( new ( nothrow ) ftkBuffer{} );
        if ( outputBuffer == nullptr )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not allocate writing buffer" << endl;
            }

            return Status::AllocationIssue;
        }

        {
            unique_lock< mutex > libLock( _LibraryProtector );
            if ( _Library == nullptr )
            {
                return Status::LibNotInitialised;
            }
            ftkError status( ftkGeometryFileConversion( _Library, inputBuffer.get(), outputBuffer.get() ) );

            if ( status > ftkError::FTK_OK )
            {
                return Status::SdkError;
            }
            else if ( status < ftkError::FTK_OK )
            {
                return Status::SdkWarning;
            }
        }

        ofstream outputFile( outputFileName, ios::binary );
        if ( !outputFile.is_open() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not open output file '" << outputFileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        outputFile.write( outputBuffer->data, outputBuffer->size );

        if ( outputFile.fail() )
        {
            unique_lock< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Error )
            {
                cerr << "Could not write output file '" << outputFileName << "'" << endl;
            }
            return Status::InvalidFile;
        }

        outputFile.close();

        return Status::Ok;
    }

    Status TrackingSystemAbstract::createFrame(
      bool pixels, uint32_t eventCount, uint32_t rawDataCount, uint32_t fiducialsCount, uint32_t markerCount )
    {
        lock_guard< mutex > lock( _AllocatedFramesProtector );
        if ( numberOfEnumeratedDevices() == 0u )
        {
            return Status::NoDevices;
        }
        else if ( numberOfEnumeratedDevices() > 1u )
        {
            return Status::SeveralDevices;
        }
        else if ( _AllocatedFrames.size() == 1u && _AllocatedFrames.front() != nullptr )
        {
            ftkError err( ftkSetFrameOptions( pixels,
                                              eventCount,
                                              rawDataCount,
                                              rawDataCount,
                                              fiducialsCount,
                                              markerCount,
                                              _AllocatedFrames.front() ) );

            if ( err > ftkError::FTK_OK )
            {
                ftkDeleteFrame( _AllocatedFrames.front() );
                _AllocatedFrames.pop_front();
                return Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                ftkDeleteFrame( _AllocatedFrames.front() );
                _AllocatedFrames.pop_front();
                return Status::SdkWarning;
            }
        }
        else if ( _AllocatedFrames.size() == 1u && _AllocatedFrames.front() == nullptr )
        {
            _AllocatedFrames.front() = ftkCreateFrame();
            if ( _AllocatedFrames.front() != nullptr )
            {
                ftkError err( ftkSetFrameOptions( pixels,
                                                  eventCount,
                                                  rawDataCount,
                                                  rawDataCount,
                                                  fiducialsCount,
                                                  markerCount,
                                                  _AllocatedFrames.front() ) );

                if ( err > ftkError::FTK_OK )
                {
                    ftkDeleteFrame( _AllocatedFrames.front() );
                    _AllocatedFrames.pop_front();
                    return Status::SdkError;
                }
                else if ( err < ftkError::FTK_OK )
                {
                    ftkDeleteFrame( _AllocatedFrames.front() );
                    _AllocatedFrames.pop_front();
                    return Status::SdkWarning;
                }
            }
            else
            {
                return Status::AllocationIssue;
            }

            return Status::Ok;
        }

        ftkFrameQuery* result( ftkCreateFrame() );

        if ( result != nullptr )
        {
            ftkError err( ftkSetFrameOptions(
              pixels, eventCount, rawDataCount, rawDataCount, fiducialsCount, markerCount, result ) );

            if ( err > ftkError::FTK_OK )
            {
                ftkDeleteFrame( result );
                return Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                ftkDeleteFrame( result );
                return Status::SdkWarning;
            }

            _AllocatedFrames.push_back( result );
        }
        else
        {
            return Status::AllocationIssue;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::createFrames(
      bool pixels, uint32_t eventCount, uint32_t rawDataCount, uint32_t fiducialsCount, uint32_t markerCount )
    {
        unique_lock< mutex > lock1( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            return Status::NoDevices;
        }

        _AllocatedFrames.resize( nDevices, nullptr );

        Status result( Status::Ok );

        for ( auto& frame : _AllocatedFrames )
        {
            if ( frame == nullptr )
            {
                frame = ftkCreateFrame();
                if ( frame == nullptr )
                {
                    result = Status::AllocationIssue;
                    break;
                }
            }

            ftkError err( ftkSetFrameOptions(
              pixels, eventCount, rawDataCount, rawDataCount, fiducialsCount, markerCount, frame ) );

            if ( err > ftkError::FTK_OK )
            {
                result = Status::SdkError;
                break;
            }
            else if ( err < ftkError::FTK_OK )
            {
                result = Status::SdkWarning;
                break;
            }
        }

        if ( result != Status::Ok )
        {
            for ( auto& frame : _AllocatedFrames )
            {
                ftkDeleteFrame( frame );
            }
            _AllocatedFrames.clear();
        }

        return result;
    }

    Status TrackingSystemAbstract::getLastError( map< string, string >& messages ) const
    {
        lock_guard< mutex > lock( _LibraryProtector );
        messages.clear();
        ftkBuffer buffer;
        ftkError err( ftkGetLastErrorString( _Library, sizeof( buffer.data ), buffer.data ) );

        if ( err != ftkError::FTK_OK )
        {
            return Status::SdkError;
        }

        string message( buffer.data );

        string errorMsg( "" );

        size_t openLoc( message.find( _ErrorOpenTag ) ), closeLoc( message.find( _ErrorCloseTag ) );
        if ( openLoc == string::npos || closeLoc == string::npos )
        {
            return Status::ParsingError;
        }
        else if ( closeLoc > openLoc + _ErrorOpenTag.size() )
        {
            errorMsg =
              message.substr( openLoc + _ErrorOpenTag.size(), closeLoc - ( openLoc + _ErrorOpenTag.size() ) );
        }

        messages.emplace( "errors", errorMsg );

        string warnMsg( "" );
        openLoc = message.find( _WarningOpenTag );
        closeLoc = message.find( _WarningCloseTag );
        if ( openLoc == string::npos || closeLoc == string::npos )
        {
            return Status::ParsingError;
        }
        else if ( closeLoc > openLoc + _WarningOpenTag.size() )
        {
            warnMsg = message.substr( openLoc + _WarningOpenTag.size(),
                                      closeLoc - ( openLoc + _WarningOpenTag.size() ) );
        }

        messages.emplace( "warnings", warnMsg );

        string stack( "" );
        openLoc = message.find( _MessageOpenTag );
        closeLoc = message.find( _MessageCloseTag );
        if ( openLoc == string::npos || closeLoc == string::npos )
        {
            return Status::ParsingError;
        }
        else if ( closeLoc > openLoc + _MessageOpenTag.size() )
        {
            stack = message.substr( openLoc + _MessageOpenTag.size(),
                                    closeLoc - ( openLoc + _MessageOpenTag.size() ) );
        }

        messages.emplace( "stack", stack );

        return Status::Ok;
    }

    Status TrackingSystemAbstract::streamLastError( ostream& out ) const
    {
        {
            lock_guard< mutex > lockContainer( _StreamLocksProtector );
            if ( _StreamLocks.find( &out ) == _StreamLocks.cend() )
            {
                _StreamLocks.emplace( &out, move( unique_ptr< mutex >( new mutex() ) ) );
            }
        }
        lock_guard< mutex > lock( *_StreamLocks.at( &out ).get() );
        map< string, string > msg;
        Status retCode( getLastError( msg ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }
        if ( !msg.at( "errors" ).empty() && msg.at( "errors" ) != "No errors" )
        {
            out << "Errors:" << endl << msg.at( "errors" ) << endl;
        }
        if ( !msg.at( "warnings" ).empty() && msg.at( "warnings" ) != "No errors" )
        {
            out << "Warnings:" << endl << msg.at( "warnings" ) << endl;
        }
        if ( !msg.at( "stack" ).empty() )
        {
            out << "Messages:" << endl << msg.at( "stack" ) << endl;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::getLastFrameForDevice( uint64_t serialNbr,
                                                          uint32_t timeout,
                                                          FrameData& frame ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( idx > _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No allocated frame (did you forget to call createFrame?)" << endl;
            }
            return Status::AllocationIssue;
        }
        if ( ret == Status::Ok )
        {
            ret = _getLastFrame( serialNbr, timeout, frame );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getLastFrameForDevice( uint64_t serialNbr, FrameData& frame ) const
    {
        lock_guard< mutex > lock( _DefaultTimeoutProtector );
        return getLastFrameForDevice( serialNbr, _DefaultTimeout, frame );
    }

    Status TrackingSystemAbstract::getLastFrame( uint32_t timeout, FrameData& frame ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        return _getLastFrame( _deviceSerialNumber(), timeout, frame );
    }

    Status TrackingSystemAbstract::getLastFrame( FrameData& frame ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        unique_lock< mutex > lock4( _DefaultTimeoutProtector, defer_lock );
        lock( lock1, lock2, lock3, lock4 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        return _getLastFrame( _deviceSerialNumber(), _DefaultTimeout, frame );
    }

    Status TrackingSystemAbstract::getLastFrames( uint32_t timeout, std::vector< FrameData >& frames ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "No devices detected" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices != _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "Number of devices does not match the number of "
                     << "allocated frames" << endl;
            }
            return Status::UnmatchingSizes;
        }

        frames.resize( nDevices );

        Status retCode( Status::Ok );

        for ( size_t i( 0u ); i < nDevices; ++i )
        {
            retCode = _getLastFrame( _deviceSerialNumber( i ), timeout, frames.at( i ) );
            if ( retCode != Status::Ok )
            {
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::getLastFrames( std::vector< FrameData >& frames ) const
    {
        lock_guard< mutex > lock( _DefaultTimeoutProtector );
        return getLastFrames( _DefaultTimeout, frames );
    }

    Status TrackingSystemAbstract::extractFrameInfo( uint64_t serialNbr, ftkStrobeInfo& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok && idx > _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No allocated frame (did you forget to call createFrame?)" << endl;
            }
            return Status::AllocationIssue;
        }
        else if ( ret == Status::Ok )
        {
            ftkFrameInfoData data{};
            data.WantedInformation = ftkInformationType::StrobeEnabled;
            ret = _extractFrameInfo( serialNbr, data );
            if ( ret == Status::Ok )
            {
                info = move( data.StrobeInfo );
            }
        }
        else
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "Could not get device" << endl;
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::extractFrameInfo( ftkStrobeInfo& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        ftkFrameInfoData data{};
        data.WantedInformation = ftkInformationType::StrobeEnabled;
        Status ret( _extractFrameInfo( _deviceSerialNumber(), data ) );
        if ( ret == Status::Ok )
        {
            info = move( data.StrobeInfo );
        }

        return ret;
    }

    Status TrackingSystemAbstract::extractFrameInfo( uint64_t serialNbr, ftkStereoParameters& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok && idx > _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No allocated frame (did you forget to call createFrame?)" << endl;
            }
            return Status::AllocationIssue;
        }
        else if ( ret == Status::Ok )
        {
            ftkFrameInfoData data{};
            data.WantedInformation = ftkInformationType::CalibrationParameters;
            ret = _extractFrameInfo( serialNbr, data );
            if ( ret == Status::Ok )
            {
                info = move( data.Calibration );
            }
        }
        else
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "Could not get device" << endl;
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::extractFrameInfo( vector< ftkStrobeInfo >& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices != _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "Number of devices does not match the number of "
                     << "allocated frames" << endl;
            }
            return Status::UnmatchingSizes;
        }

        info.assign( nDevices, {} );

        Status retCode( Status::Ok );

        ftkFrameInfoData data{};
        data.WantedInformation = ftkInformationType::StrobeEnabled;

        for ( size_t i( 0u ); i < nDevices; ++i )
        {
            retCode = _extractFrameInfo( _deviceSerialNumber( i ), data );
            if ( retCode == Status::Ok )
            {
                info[ i ] = move( data.StrobeInfo );
            }
            else
            {
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::extractFrameInfo( ftkStereoParameters& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        ftkFrameInfoData data{};
        data.WantedInformation = ftkInformationType::CalibrationParameters;
        Status ret( _extractFrameInfo( _deviceSerialNumber(), data ) );
        if ( ret == Status::Ok )
        {
            info = move( data.Calibration );
        }

        return ret;
    }

    Status TrackingSystemAbstract::extractFrameInfo( vector< ftkStereoParameters >& info ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices != _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "Number of devices does not match the number of "
                     << "allocated frames" << endl;
            }
            return Status::UnmatchingSizes;
        }

        info.assign( nDevices, {} );

        Status retCode( Status::Ok );

        ftkFrameInfoData data{};
        data.WantedInformation = ftkInformationType::CalibrationParameters;

        for ( size_t i( 0u ); i < nDevices; ++i )
        {
            retCode = _extractFrameInfo( _deviceSerialNumber( i ), data );
            if ( retCode == Status::Ok )
            {
                info[ i ] = move( data.Calibration );
            }
            else
            {
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::getIntOption( uint64_t serialNbr,
                                                 uint32_t optId,
                                                 ftkOptionGetter what,
                                                 int32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getIntOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getIntOption( uint64_t serialNbr, uint32_t optId, int32& value ) const
    {
        return getIntOption( serialNbr, optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::getIntOption( uint32_t optId, ftkOptionGetter what, int32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getIntOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getIntOption( uint32_t optId, int32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getIntOption( serialNbr, opt.id(), ftkOptionGetter::FTK_VALUE, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getIntOption( uint64_t serialNbr,
                                                 const string& optId,
                                                 ftkOptionGetter what,
                                                 int32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getIntOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getIntOption( uint64_t serialNbr, const string& optId, int32& value ) const
    {
        return getIntOption( serialNbr, optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::getIntOption( const string& optId,
                                                 ftkOptionGetter what,
                                                 int32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getIntOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getIntOption( const string& optId, int32& value ) const
    {
        return getIntOption( optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::setIntOption( uint64_t serialNbr, uint32_t optId, int32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setIntOption( serialNbr, optId, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setIntOption( uint32_t optId, int32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setIntOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setIntOption( uint64_t serialNbr, const string& optId, int32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setIntOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setIntOption( const string& optId, int32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_INT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setIntOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getFloatOption( uint64_t serialNbr,
                                                   uint32_t optId,
                                                   ftkOptionGetter what,
                                                   float32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getFloatOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getFloatOption( uint32_t optId,
                                                   ftkOptionGetter what,
                                                   float32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getFloatOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getFloatOption( uint64_t serialNbr, uint32_t optId, float32& value ) const
    {
        return getFloatOption( serialNbr, optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::getFloatOption( uint32_t optId, float32& value ) const
    {
        return getFloatOption( optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::getFloatOption( uint64_t serialNbr,
                                                   const string& optId,
                                                   ftkOptionGetter what,
                                                   float32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getFloatOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getFloatOption( const string& optId,
                                                   ftkOptionGetter what,
                                                   float32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getFloatOption( serialNbr, opt.id(), what, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getFloatOption( uint64_t serialNbr,
                                                   const string& optId,
                                                   float32& value ) const
    {
        return getFloatOption( serialNbr, optId, ftkOptionGetter::FTK_VALUE, value );
    }

    Status TrackingSystemAbstract::getFloatOption( const string& optId, float32& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getFloatOption( serialNbr, opt.id(), ftkOptionGetter::FTK_VALUE, value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setFloatOption( uint64_t serialNbr, uint32_t optId, float32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setFloatOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setFloatOption( uint32_t optId, float32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setFloatOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setFloatOption( uint64_t serialNbr,
                                                   const string& optId,
                                                   float32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setFloatOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setFloatOption( const string& optId, float32 value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_FLOAT32, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setFloatOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint64_t serialNbr, uint32_t optId, ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint64_t serialNbr, uint32_t optId, string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            ret = _getDataOption( serialNbr, opt.id(), buffer );
            if ( ret == Status::Ok )
            {
                value = string( buffer.data, buffer.size );
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint32_t optId, ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint32_t optId, string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            ret = _getDataOption( serialNbr, opt.id(), buffer );
            if ( ret == Status::Ok )
            {
                value = string( buffer.data );
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint64_t serialNbr,
                                                  const string& optId,
                                                  ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( uint64_t serialNbr,
                                                  const string& optId,
                                                  string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            ret = _getDataOption( serialNbr, opt.id(), buffer );
            if ( ret == Status::Ok )
            {
                value = string( buffer.data, buffer.size );
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( const string& optId, ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _getDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getDataOption( const string& optId, string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            ret = _getDataOption( serialNbr, opt.id(), buffer );
            if ( ret == Status::Ok )
            {
                value = string( buffer.data, buffer.size );
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint64_t serialNbr,
                                                  uint32_t optId,
                                                  const ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint64_t serialNbr,
                                                  uint32_t optId,
                                                  const string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            buffer.reset();
            strncpy( buffer.data, value.c_str(), sizeof( buffer.data ) );
            buffer.size = uint32( value.length() );
            ret = _setDataOption( serialNbr, opt.id(), buffer );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint32_t optId, const ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint32_t optId, const string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            buffer.reset();
            strncpy( buffer.data, value.c_str(), sizeof( buffer.data ) );
            buffer.size = uint32( value.length() );
            ret = _setDataOption( serialNbr, opt.id(), buffer );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint64_t serialNbr,
                                                  const string& optId,
                                                  const ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( uint64_t serialNbr,
                                                  const string& optId,
                                                  const string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            buffer.reset();
            strncpy( buffer.data, value.c_str(), sizeof( buffer.data ) );
            buffer.size = uint32( value.length() );
            ret = _setDataOption( serialNbr, opt.id(), buffer );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( const string& optId, const ftkBuffer& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ret = _setDataOption( serialNbr, opt.id(), value );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setDataOption( const string& optId, const string& value ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        uint64_t serialNbr( numeric_limits< uint64_t >::max() );
        if ( nDevices == 0u )
        {
            serialNbr = 0uLL;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        else
        {
            serialNbr = _deviceSerialNumber();
        }

        DeviceOption opt;
        Status ret( _getOptionDesc( serialNbr, optId, ftkOptionType::FTK_DATA, opt ) );

        if ( ret == Status::Ok )
        {
            ftkBuffer buffer;
            buffer.reset();
            strncpy( buffer.data, value.c_str(), sizeof( buffer.data ) );
            buffer.size = uint32( value.length() );
            ret = _setDataOption( serialNbr, opt.id(), buffer );
        }

        return ret;
    }

    Status TrackingSystemAbstract::setGeometry( uint64_t serialNbr, const ftkGeometry& geom )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            ftkRigidBody newGeom;
            newGeom = geom;
            ftkError err( ftkSetRigidBody( _Library, dev._SerialNumber, &newGeom ) );
            if ( err > ftkError::FTK_OK )
            {
                return Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                return Status::SdkWarning;
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::setGeometry( uint64_t serialNbr, const ftkRigidBody& geom )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( ret == Status::Ok )
        {
            ftkError err(
              ftkSetRigidBody( _Library, dev._SerialNumber, const_cast< ftkRigidBody* >( &geom ) ) );
            if ( err > ftkError::FTK_OK )
            {
                return Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                return Status::SdkWarning;
            }
        }

        return ret;
    }

    Status TrackingSystemAbstract::setGeometry( uint64_t serialNbr, const string& geomFileName )
    {
        ftkRigidBody theGeom{};
        Status retCode( loadGeometry( geomFileName, theGeom ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }
        return setGeometry( serialNbr, theGeom );
    }

    Status TrackingSystemAbstract::setGeometry( const ftkGeometry& geom )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        Status retCode( Status::NoDevices );
        ftkError err( ftkError::FTK_OK );
        ftkRigidBody newGeom;
        newGeom = geom;
        for ( size_t idx( 0u ); idx < _numberOfEnumeratedDevices(); ++idx )
        {
            err = ftkSetRigidBody( _Library, _deviceSerialNumber( idx ), &newGeom );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
                break;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
                break;
            }
            retCode = Status::Ok;
        }

        return retCode;
    }

    Status TrackingSystemAbstract::setGeometry( const ftkRigidBody& geom )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        Status retCode( Status::NoDevices );
        ftkError err( ftkError::FTK_OK );
        for ( size_t idx( 0u ); idx < _numberOfEnumeratedDevices(); ++idx )
        {
            err =
              ftkSetRigidBody( _Library, _deviceSerialNumber( idx ), const_cast< ftkRigidBody* >( &geom ) );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
                break;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
                break;
            }
            retCode = Status::Ok;
        }

        return retCode;
    }

    Status TrackingSystemAbstract::setGeometry( const string& geomFileName )
    {
        ftkRigidBody theGeom{};
        Status retCode( loadGeometry( geomFileName, theGeom ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }
        return setGeometry( theGeom );
    }

    Status TrackingSystemAbstract::unsetGeometry( uint64_t serialNbr, uint32_t geomId )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }

        ftkError err( ftkClearRigidBody( _Library, dev._SerialNumber, geomId ) );
        if ( err > ftkError::FTK_OK )
        {
            retCode = Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            retCode = Status::SdkWarning;
        }

        return retCode;
    }

    Status TrackingSystemAbstract::unsetGeometry( uint64_t serialNbr, const ftkGeometry& geom )
    {
        return unsetGeometry( serialNbr, geom.geometryId );
    }

    Status TrackingSystemAbstract::unsetGeometry( uint64_t serialNbr, const ftkRigidBody& geom )
    {
        return unsetGeometry( serialNbr, geom.geometryId );
    }

    Status TrackingSystemAbstract::unsetGeometry( uint64_t serialNbr, const string& geomFileName )
    {
        ftkRigidBody theGeom{};
        Status retCode( loadGeometry( geomFileName, theGeom ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }
        return unsetGeometry( serialNbr, theGeom.geometryId );
    }

    Status TrackingSystemAbstract::unsetGeometry( uint32_t geomId )
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        Status retCode( Status::Ok );
        ftkError err( ftkError::FTK_OK );
        for ( size_t idx( 0u ); idx < _numberOfEnumeratedDevices(); ++idx )
        {
            err = ftkClearRigidBody( _Library, _deviceSerialNumber( idx ), geomId );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
                break;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::unsetGeometry( const ftkGeometry& geom )
    {
        return unsetGeometry( geom.geometryId );
    }

    Status TrackingSystemAbstract::unsetGeometry( const ftkRigidBody& geom )
    {
        return unsetGeometry( geom.geometryId );
    }

    Status TrackingSystemAbstract::unsetGeometry( const string& geomFileName )
    {
        ftkRigidBody theGeom{};
        Status retCode( loadGeometry( geomFileName, theGeom ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }
        return unsetGeometry( theGeom.geometryId );
    }

    Status TrackingSystemAbstract::triangulate( uint64_t serialNbr,
                                                const array< float, 2u >& leftPixel,
                                                const array< float, 2u >& rightPixel,
                                                ftk3DPoint& outPoint ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );
        if ( retCode != Status::Ok )
        {
            return retCode;
        }

        ftk3DPoint leftPoint = { leftPixel.at( 0u ), leftPixel.at( 1u ), 0.f },
                   rightPoint = { rightPixel.at( 0u ), rightPixel.at( 1u ), 0.f };

        return _triangulate( serialNbr, leftPoint, rightPoint, outPoint );
    }

    Status TrackingSystemAbstract::triangulate( uint64_t serialNbr,
                                                const array< float, 2u >& leftPixel,
                                                const array< float, 2u >& rightPixel,
                                                array< float, 3u >& outPoint ) const
    {
        ftk3DPoint point = { 0 };

        Status retCode( triangulate( serialNbr, leftPixel, rightPixel, point ) );

        if ( retCode == Status::Ok )
        {
            outPoint = { point.x, point.y, point.z };
        }

        return retCode;
    }

    Status TrackingSystemAbstract::triangulate( const array< float, 2u >& leftPixel,
                                                const array< float, 2u >& rightPixel,
                                                ftk3DPoint& outPoint ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        if ( numberOfEnumeratedDevices() == 0u )
        {
            return Status::NoDevices;
        }
        else if ( numberOfEnumeratedDevices() > 1u )
        {
            return Status::SeveralDevices;
        }
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        const uint64_t serialNbr( _deviceSerialNumber() );

        ftk3DPoint leftPoint = { leftPixel.at( 0u ), leftPixel.at( 1u ), 0.f },
                   rightPoint = { rightPixel.at( 0u ), rightPixel.at( 1u ), 0.f };

        return _triangulate( serialNbr, leftPoint, rightPoint, outPoint );
    }

    Status TrackingSystemAbstract::triangulate( const array< float, 2u >& leftPixel,
                                                const array< float, 2u >& rightPixel,
                                                array< float, 3u >& outPoint ) const
    {
        ftk3DPoint point = { 0 };

        Status retCode( triangulate( leftPixel, rightPixel, point ) );

        if ( retCode == Status::Ok )
        {
            outPoint = { point.x, point.y, point.z };
        }

        return retCode;
    }

    Status TrackingSystemAbstract::reproject( uint64_t serialNbr,
                                              const ftk3DPoint& inPoint,
                                              array< float, 2u >& outLeftData,
                                              array< float, 2u >& outRightData ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );
        outLeftData.fill( 0.0f );
        outRightData.fill( 0.0f );
        if ( retCode == Status::Ok )
        {
            ftk3DPoint leftPoint, rightPoint;
            retCode = _reproject( serialNbr, inPoint, leftPoint, rightPoint );
            if ( retCode == Status::Ok )
            {
                outLeftData.at( 0u ) = leftPoint.x;
                outLeftData.at( 1u ) = leftPoint.y;
                outRightData.at( 0u ) = rightPoint.x;
                outRightData.at( 1u ) = rightPoint.y;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::reproject( uint64_t serialNbr,
                                              const array< float, 3u >& inPoint,
                                              array< float, 2u >& outLeftData,
                                              array< float, 2u >& outRightData ) const
    {
        ftk3DPoint point = { inPoint.at( 0u ), inPoint.at( 1u ), inPoint.at( 2u ) };

        return reproject( serialNbr, point, outLeftData, outRightData );
    }

    Status TrackingSystemAbstract::reproject( const ftk3DPoint& inPoint,
                                              array< float, 2u >& outLeftData,
                                              array< float, 2u >& outRightData ) const
    {
        outLeftData.fill( 0.f );
        outRightData.fill( 0.f );
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2 );
        if ( numberOfEnumeratedDevices() == 0u )
        {
            return Status::NoDevices;
        }
        else if ( numberOfEnumeratedDevices() > 1u )
        {
            return Status::SeveralDevices;
        }
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            return Status::SeveralDevices;
        }
        const uint64_t serialNbr( _deviceSerialNumber() );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );
        outLeftData.fill( 0.0f );
        outRightData.fill( 0.0f );
        if ( retCode == Status::Ok )
        {
            ftk3DPoint leftPoint, rightPoint;
            retCode = _reproject( serialNbr, inPoint, leftPoint, rightPoint );
            if ( retCode == Status::Ok )
            {
                outLeftData.at( 0u ) = leftPoint.x;
                outLeftData.at( 1u ) = leftPoint.y;
                outRightData.at( 0u ) = rightPoint.x;
                outRightData.at( 1u ) = rightPoint.y;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::reproject( const array< float, 3u >& inPoint,
                                              array< float, 2u >& outLeftData,
                                              array< float, 2u >& outRightData ) const
    {
        ftk3DPoint point = { inPoint.at( 0u ), inPoint.at( 1u ), inPoint.at( 2u ) };

        return reproject( point, outLeftData, outRightData );
    }

    Status TrackingSystemAbstract::getAccelerometerData( uint64_t serialNbr,
                                                         vector< array< float, 3u > >& data ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( idx > _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No allocated frame (did you forget to call createFrame?)" << endl;
            }
            return Status::AllocationIssue;
        }
        if ( ret == Status::Ok )
        {
            ret = _getAccelerometerData( serialNbr, data );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getAccelerometerData( vector< array< float, 3u > >& data ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        unique_lock< mutex > lock4( _DefaultTimeoutProtector, defer_lock );
        lock( lock1, lock2, lock3, lock4 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        return _getAccelerometerData( _deviceSerialNumber(), data );
    }

    Status TrackingSystemAbstract::getAccelerometerData( vector< vector< array< float, 3u > > >& data ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "No devices detected" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices != _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "Number of devices does not match the number of "
                     << "allocated frames" << endl;
            }
            return Status::UnmatchingSizes;
        }

        data.resize( nDevices );

        Status retCode( Status::Ok );

        for ( size_t i( 0u ); i < nDevices; ++i )
        {
            retCode = _getAccelerometerData( _deviceSerialNumber( i ), data.at( i ) );
            if ( retCode != Status::Ok )
            {
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::getRealTimeClock( uint64_t serialNbr, timestamp_t& epoch ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status ret( _getDevice( serialNbr, dev, idx ) );
        if ( idx > _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No allocated frame (did you forget to call createFrame?)" << endl;
            }
            return Status::AllocationIssue;
        }
        if ( ret == Status::Ok )
        {
            ret = _getRealTimeClock( serialNbr, epoch );
        }

        return ret;
    }

    Status TrackingSystemAbstract::getRealTimeClock( timestamp_t& epoch ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        unique_lock< mutex > lock4( _DefaultTimeoutProtector, defer_lock );
        lock( lock1, lock2, lock3, lock4 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << "No detected devices" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices > 1u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level <= LogLevel::Warning )
            {
                cerr << nDevices << " detected devices" << endl;
            }
            return Status::SeveralDevices;
        }

        return _getRealTimeClock( _deviceSerialNumber(), epoch );
    }

    Status TrackingSystemAbstract::getRealTimeClock( vector< timestamp_t >& epoch ) const
    {
        unique_lock< mutex > lock1( _LibraryProtector, defer_lock );
        unique_lock< mutex > lock2( _AllocatedFramesProtector, defer_lock );
        unique_lock< mutex > lock3( _enumeratedDevicesProtector(), defer_lock );
        lock( lock1, lock2, lock3 );
        const size_t nDevices( _numberOfEnumeratedDevices() );
        if ( nDevices == 0u )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "No devices detected" << endl;
            }
            return Status::NoDevices;
        }
        else if ( nDevices != _AllocatedFrames.size() )
        {
            lock_guard< mutex > lock( _LevelProtector );
            if ( _Level < LogLevel::Info )
            {
                cerr << "Number of devices does not match the number of "
                     << "allocated frames" << endl;
            }
            return Status::UnmatchingSizes;
        }

        epoch.resize( nDevices );

        Status retCode( Status::Ok );

        for ( size_t i( 0u ); i < nDevices; ++i )
        {
            retCode = _getRealTimeClock( _deviceSerialNumber( i ), epoch.at( i ) );
            if ( retCode != Status::Ok )
            {
                break;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::_getLastFrame( uint64_t serialNbr,
                                                  uint32_t timeout,
                                                  FrameData& frame ) const
    {
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );

        if ( retCode == Status::Ok )
        {
            ftkError err( ftkGetLastFrame( _Library, serialNbr, _AllocatedFrames.at( idx ), timeout ) );
            if ( err == ftkError::FTK_WAR_NO_FRAME )
            {
                frame = nullptr;
                return Status::SdkWarning;
            }
            frame = _AllocatedFrames.at( idx );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::_extractFrameInfo( uint64_t serialNbr, ftkFrameInfoData& info ) const
    {
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );

        if ( retCode == Status::Ok )
        {
            ftkError err( ftkExtractFrameInfo( _AllocatedFrames.at( idx ), &info ) );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::_getIntOption( uint64_t serialNbr,
                                                  uint32_t optId,
                                                  ftkOptionGetter what,
                                                  int32& value ) const
    {
        ftkError err( ftkGetInt32( _Library, serialNbr, optId, &value, what ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_setIntOption( uint64_t serialNbr, uint32_t optId, int32 value ) const
    {
        ftkError err( ftkSetInt32( _Library, serialNbr, optId, value ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_getFloatOption( uint64_t serialNbr,
                                                    uint32_t optId,
                                                    ftkOptionGetter what,
                                                    float& value ) const
    {
        ftkError err( ftkGetFloat32( _Library, serialNbr, optId, &value, what ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_setFloatOption( uint64_t serialNbr, uint32_t optId, float value ) const
    {
        ftkError err( ftkSetFloat32( _Library, serialNbr, optId, value ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_getDataOption( uint64_t serialNbr,
                                                   uint32_t optId,
                                                   ftkBuffer& value ) const
    {
        ftkError err( ftkGetData( _Library, serialNbr, optId, &value ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_setDataOption( uint64_t serialNbr,
                                                   uint32_t optId,
                                                   const ftkBuffer& value ) const
    {
        ftkError err( ftkSetData( _Library, serialNbr, optId, const_cast< ftkBuffer* >( &value ) ) );
        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_triangulate( uint64_t serialNbr,
                                                 const ftk3DPoint& point3dLeftPixel,
                                                 const ftk3DPoint& point3dRightPixel,
                                                 ftk3DPoint& outPoint ) const
    {
        ftkError err(
          ftkTriangulate( _Library, serialNbr, &point3dLeftPixel, &point3dRightPixel, &outPoint ) );

        if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }
        else if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_reproject( uint64_t serialNbr,
                                               const ftk3DPoint& inPoint,
                                               ftk3DPoint& leftPoint,
                                               ftk3DPoint& rightPoint ) const
    {
        ftkError err( ftkReprojectPoint( _Library, serialNbr, &inPoint, &leftPoint, &rightPoint ) );

        if ( err > ftkError::FTK_OK )
        {
            return Status::SdkError;
        }
        else if ( err < ftkError::FTK_OK )
        {
            return Status::SdkWarning;
        }

        return Status::Ok;
    }

    Status TrackingSystemAbstract::_getAccelerometerData( uint64_t serialNbr,
                                                          vector< array< float, 3u > >& data ) const
    {
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );

        data.clear();

        if ( retCode == Status::Ok )
        {
            ftk3DPoint components{};
            array< float, 3u > tmp{};
            uint32_t iSensor( 0u );
            while ( true )
            {
                ftkError err( ftkGetAccelerometerData( _Library, serialNbr, iSensor, &components ) );
                if ( err == ftkError::FTK_ERR_INV_INDEX )
                {
                    break;
                }
                else if ( err > ftkError::FTK_OK )
                {
                    retCode = Status::SdkError;
                    break;
                }
                else if ( err < ftkError::FTK_OK )
                {
                    retCode = Status::SdkWarning;
                    break;
                }
                tmp = { components.x, components.y, components.z };
                data.emplace_back( tmp );
                ++iSensor;
            }
        }

        return retCode;
    }

    Status TrackingSystemAbstract::_getRealTimeClock( uint64_t serialNbr, timestamp_t& epoch ) const
    {
        DeviceInfo dev{};
        size_t idx{ string::npos };
        Status retCode( _getDevice( serialNbr, dev, idx ) );

        if ( retCode == Status::Ok )
        {
            uint64_t timestamp( 0ULL );
            ftkError err( ftkGetRealTimeClock( _Library, serialNbr, &timestamp ) );
            if ( err > ftkError::FTK_OK )
            {
                retCode = Status::SdkError;
            }
            else if ( err < ftkError::FTK_OK )
            {
                retCode = Status::SdkWarning;
            }
            else
            {
                epoch = chrono::time_point< chrono::system_clock, chrono::duration< uint64_t > >(
                  chrono::duration< uint64_t >( timestamp ) );
            }
        }

        return retCode;
    }

    // --------------------------------------------------------------------- //
    //                                                                       //
    //                   Template function specialisations                   //
    //                                                                       //
    // --------------------------------------------------------------------- //

    template<>
    void stringReader( const string& str, uint32_t& value )
    {
        value = stoul( str );
    }

    template<>
    void stringReader( const string& str, float& value )
    {
        value = stof( str );
    }
}  // namespace atracsys

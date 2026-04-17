classdef FusionTrack < handle
    % Wrapper of the FusionTrack library
    % See fusionTrack C API for more detailed info
    % Note that most of the parameters are typed
    %
    % What is currently not wrapped
    % - no access to low-level information (left/right images/raw data)
    %
    % Supported Matlab version:
    % - Matlab R2012b 64 bits
	
    properties (SetAccess = private, Hidden = true)
        % fusionTrack library handle.
        LibraryHandle;
    end
    
    properties (Constant)
        FTK_OPT_DRIVER_VER  = uint32(4); % Hardcoded option ID for the SDK version
        FTK_OPT_SAVE_ENV = uint32(155); % Hardcoded option ID for saving environment
        FTK_OPT_LOAD_ENV = uint32(156); % Hardcoded option ID for loading environment
        FTK_MIN_VAL = uint8(0); % Getting the minimal value of the option.
        FTK_MAX_VAL = uint8(1); % Getting the maximal value of the option.
        FTK_DEF_VAL = uint8(2); % Getting the default value of the option.
        FTK_VALUE   = uint8(3); % Getting the current value of the option.
        version = 1; % Current version of the wrapper
    end
    
    methods
        
        function this = FusionTrack(varargin)
            % Standard constructor.
            % This method creates a new C++ FusionTrack class and calls
            % FusionTrack::initialise.
            %
            %INPUT:
            %   - configFile (optional) path to a valid configuration JSON
            %   file.
            % 
            % e.g: s = FusionTrack
            this.LibraryHandle = sdkMatlabInterface('new', varargin{:});
        end
        
		function delete(this)
            % Destructor, must be called.
            % The destructor must be called before clearing the variable,
            % otherwise Matlab's memory will be corrupted.
            % e.g: s.delete; clear s;
            sdkMatlabInterface('delete', this.LibraryHandle);
        end
        
		function varargout = devices(this, varargin)
            % enumerate the devices (return a list of serial numbers).
            % This method is a wrapper for the FusionTrack::devices() C++
            % method
            % e.g: sn = s.devices
            [varargout{1:nargout}] = sdkMatlabInterface('devices', this.LibraryHandle, varargin{:});
        end

		function varargout = options(this, varargin)
            % enumerate the options (return a list of serial numbers).
            % This method is a wrapper for the FusionTrack::options() C++
            % method
            %
            %OUTPUT:
            %   - array list of detected serial numbers.
            %
            % e.g: sn = s.options
            [varargout{1:nargout}] = sdkMatlabInterface('options', this.LibraryHandle, varargin{:});
        end
        
		function varargout = getInt32(this, varargin)
            % get an int32 option
            % This method is a wrapper for the FusionTrack::getInt32() C++
            % method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %   - what determines wether the current value, minimal /
            %   maximal / default value must be retrieved.
            %
            %OUTPUT:
            %   - value of the option.
            %
            % e.g: s.getInt32(sn, uint32(96), s.FTK_VALUE)
            [varargout{1:nargout}] = sdkMatlabInterface('getint32', this.LibraryHandle, varargin{:});
        end
        
        % set an int32 option
        % e.g: s.setInt32(sn, s.FTK_OPT_PREDEF_GEOM, int32(1))
		function varargout = setInt32(this, varargin)
            % set an int32 option
            % This method is a wrapper for the FusionTrack::setInt32() C++
            % method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %   - val value of the option.
            %
            % e.g: s.setInt32(sn, s.FTK_OPT_PREDEF_GEOM, int32(1))
            [varargout{1:nargout}] = sdkMatlabInterface('setint32', this.LibraryHandle, varargin{:});
        end

		function varargout = getFloat32(this, varargin)
            % get a floatt32 option
            % This method is a wrapper for the FusionTrack::getFloatt32()
            % C++ method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %   - what determines wether the current value, minimal /
            %   maximal / default value must be retrieved.
            %
            %OUTPUT:
            %   - value of the option.
            %
            % e.g:  s.getFloat32(sn, uint32(1001), s.FTK_MIN_VAL)
            [varargout{1:nargout}] = sdkMatlabInterface('getfloat32', this.LibraryHandle, varargin{:});
        end
        
		function varargout = setFloat32(this, varargin)
            % set a float32 option
            % This method is a wrapper for the FusionTrack::setFloat32()
            % C++ method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %   - val value of the option.
            %
            % e.g: s.setInt32(sn, s.FTK_OPT_PREDEF_GEOM, int32(1))
            [varargout{1:nargout}] = sdkMatlabInterface('setfloat32', this.LibraryHandle, varargin{:});
        end


		function varargout = getData(this, varargin)
            % get a string option
            % This method is a wrapper for the FusionTrack::getData()
            % C++ method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %
            %OUTPUT:
            %   - value of the option.
            %
            % e.g:  s.getData(sn, s.FTK_OPT_DRIVER_VER)
            [varargout{1:nargout}] = sdkMatlabInterface('getdata', this.LibraryHandle, varargin{:});
        end

        function varargout = setData(this, varargin)
            % set a string option
            % This method is a wrapper for the FusionTrack::setData()
            % C++ method
            %
            %INPUT:
            %   - device serial number of the wanted device.
            %   - optId unique ID of the wanted option.
            %   - val value of the option.
            %
            % e.g: s.setData(sn, s.FTK_SAVE_ENV, "C:/Temp/environment.json")
            varargin{3} = convertStringsToChars(varargin{3});
            [varargout{1:nargout}] = sdkMatlabInterface('setdata', this.LibraryHandle, varargin{:});
        end

		function varargout = geometries(this, varargin)
            % enumerate the geometries (return a list structures).
            % This method is a wrapper for the FusionTrack::geometries
            % C++ method
            %
            %INPUT:
            %   - sn serial number of the wanted device.
            %
            %OUTPUT:
            %   - array list of registered geometries.
            %
            % e.g: geoms = s.geometries (sn);
            [varargout{1:nargout}] = sdkMatlabInterface('geometries', this.LibraryHandle, varargin{:});
        end

		function varargout = clearGeometry(this, varargin)
            % clears the geometry for the wanted device
            % This method is a wrapper for the FusionTrack::clearGeometry
            % C++ method
            %
            %INPUT:
            %   - sn serial number of the wanted device.
            %   - id geometry ID of the geometry to remove.
            %
            % e.g: s.clearGeometry (sn,uint32(1))
            [varargout{1:nargout}] = sdkMatlabInterface('cleargeometry', this.LibraryHandle, varargin{:});
        end
        
        % set a geometry
		function varargout = setGeometry(this, varargin)
            % sets the geometry for the wanted device
            % This method is a wrapper for the FusionTrack::setGeometry C++
            % method
            %
            %INPUT:
            %   - sn serial number of the wanted device.
            %   - geom geometry structure.
            %
            % geom = s.getGeometriesFromFile(sn, filename); s.setGeometry (sn, geom)
            geometries = varargin{2}; % Cell of struct array

            for i = 1:size(geometries)

                varargin(i + 1) = {geometries(i)} ; % to cells of geom struct

            end    

            [varargout{1:nargout}] = sdkMatlabInterface('setgeometry', this.LibraryHandle, varargin{:});
        end

        function varargout = getGeometriesFromFile(this,  varargin)
            % Get rigid body corresponding to geometry file
            % This method is a wrapper for the FusionTrack::loadRigidBody
            % C++ method. It calls then FusionTrack::getRigBodyFromFile().
            %
            %INPUT:
            %   - sn serial number of the wanted device.
            %   - fileNames paths to geometries
            %
            %OUTPUT:
            %   - geometries geometries structure 
            %
            % geometries = s.getRigidBodies( sn, fileNames )
            % ex: geometries = s.getRigidBodies( sn, ["geometry075.ini";"geometry075.ini"])
            filesCell = varargin{2}; % Cell with file names

                if ischar(filesCell) % Only works if char arrays are of the same length

                    for i = 1:height(filesCell)

                        varargin(i + 1) = {filesCell(i,:)};
                        
                    end

                elseif isstring(filesCell) 

                    for i = 1:numel(filesCell) 

                        varargin(i + 1) = {convertStringsToChars(filesCell(i))};

                    end

                end

            [varargout{1:nargout}] = sdkMatlabInterface('getgeometriesfromfile', this.LibraryHandle, varargin{:});
        end
                
		function varargout = getlastFrame(this, varargin)
            % get the lattest available frame.
            % This method is a wrapper for the FusionTrack::getLastFrame
            % C++ method. It calls then FusionTrack::getFrame().
            %
            %INPUT:
            %   - sn serial number of the wanted device.
            %
            %OUTPUT:
            %   - frame frame structure, with marker and fiducial data.
            %
            % frame = s.getlastFrame(sn)
            [varargout{1:nargout}] = sdkMatlabInterface('getframe', this.LibraryHandle, varargin{:});
        end
    end      
    
end


function [] = run( geomFileName )
%%run Starts the fusionTrack connection and demonstrates how to get a
%%marker.
%   This script shows how to connect Matlab to a fusionTrack device, set an
%   option, register a geometry and getting data.
%
%   Due to the buggy behaviour of Matlab, clearing the environment might be
%   needed after running this script in order to avoid a crash when running
%   it a second time.
%
%INPUT: geomFileName path of the INI file containing the geometry of the
%       used marker.

if nargin < 1
    error('The geometry file to load must be given');
end
 
% Constant declarations

fCheck = FrameChecker();
s = FusionTrack();
sn = s.devices;

if isempty(sn)
    error('No devices detected');
end

sn = sn(1);

fprintf ('\nDriver version %s\n\n', s.getData(sn, s.FTK_OPT_DRIVER_VER));
options = s.options(sn);
options(4) % 1001
fprintf ('Minimum value of option 1001: %f\n', s.getFloat32(sn, uint32(1001), s.FTK_MIN_VAL));
fprintf ('Value of option 1001:         %f\n',s.getFloat32(sn, uint32(1001), s.FTK_VALUE));
s.setFloat32(sn, uint32(1001), single(0.31415));
fprintf ('New value of option 1001:     %f\n',s.getFloat32(sn, uint32(1001), s.FTK_VALUE));

% Set geometries
geometries = s.getGeometriesFromFile(sn, geomFileName);
s.setGeometry(sn, geometries);

% Get frames
numberOfAcquisition = 100;
delayBetweenFrames = 0;
 
for i = 1:numberOfAcquisition
    frame = s.getlastFrame(sn);  
    
    if ~fCheck.basicChecks(frame, geometries)
        fprintf("Frame didn't pass checks\n");
        continue;
    end
    
    %Get markers info
    for j = 1: size (frame.markers,1)
        fprintf ('frame %d: marker %d, trans = (%f\t%f\t%f)\t rms = %f\n', ...
        frame.imageHeader.timestamp, ...
        frame.markers(j).geometryId, ...
        frame.markers(j).translationMM(1), ...
        frame.markers(j).translationMM(2), ...
        frame.markers(j).translationMM(3), ...
        frame.markers(j).registrationErrorMM);
       
    end

    % More visual representation of a frame structure
    if i == numberOfAcquisition       
        path = erase(which("run.m"), "run.m");
        fid = fopen(strcat(path, 'framestruct.json'), 'w');
        fprintf(fid,jsonencode(frame,'PrettyPrint',true));
        fclose('all');
    end
    pause(delayBetweenFrames);
end

s.delete;

clear s;

end

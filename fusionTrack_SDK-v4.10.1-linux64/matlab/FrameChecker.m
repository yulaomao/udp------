classdef FrameChecker < handle
    %FRAMECHECKER Checks frame's correctness
    %   This class is responsible of implementing 
    %   known best practices
    
    properties (SetAccess = private)
        frameTimestamp;% used for BP
    end
    
    properties(Constant)
        QS_OK = int8(0); % Used to compare statuses
        TEMP_DEVIATION_TOL = single(10); % Used for synthetic temperature deviations
    end

    methods
        function frameChecker = FrameChecker()
            %FRAMECHECKER Constructor
            %   Initialisation of members
            frameChecker.frameTimestamp = uint64( 0 );
        end
            
        function isOk = checkStatuses(this, frame)
            % (BP2) Checks if there's an error in frame statuses
            isOk = true;
            
            fn = fieldnames(frame.statuses); % get field names to iterate through
            for i = 1:numel(fn)      
                if frame.statuses.(fn{i}) > this.QS_OK
                    isOk = false;
                    fprintf("Error in status : %s\n", fn{i});
                end
            end
        end

        function isOk = checkTiltAngles(~, frame, geometries)
            % (BP4) Checks if there's an error in frame statuses
            isOk = true;
            for i = 1:numel(frame.markers)

                % Look for marker's Id in loaded Geometries
                mrkrIndex = 0;
                for j = 1:numel(geometries)
                    if geometries(j).geometryId == frame.markers(i).geometryId
                        mrkrIndex = j;
                    end
                end

                if mrkrIndex ~= 0

                    for iFid = 1:length(geometries(mrkrIndex).fiducials.positions)% number of fiducials

                        fidIndex = frame.markers(i).fiducialCorresp(iFid);
                        if fidIndex <= numel(frame.threeDFiducials) ...
                            && fidIndex ~= -1

                            % Marker's fiducial(iFid) position
                            fiducialPos = frame.threeDFiducials(fidIndex).positionMM;
                            
                            % Calculate actual fiducial's normal vector 
                            rotationMatrix = frame.markers(mrkrIndex).rotation;
                            fidGeomNormalVector = geometries(mrkrIndex).fiducials.normalVectors(:, iFid);

                            % Geometry not supporting Normal Vector
                            if all(~fidGeomNormalVector)
                                
                                isOk = true;
                                return;

                            end

                            fidRealNormalVector = rotationMatrix * fidGeomNormalVector;                   
                            angle = acos(-dot(fidRealNormalVector, fiducialPos)/ ...
                                (norm(fidRealNormalVector) * norm(fiducialPos))) * 180/pi;
                            
                        else 

                            fprintf("Error : no corresponding fiducial for Marker %d , fiducial index : %d\n", ...
                                frame.markers(i).geometryId, frame.markers(i).fiducialCorresp(iFid));
                            isOk = false;
                            return;

                        end
                                                  
                        if angle > 50.0

                            fprintf("Possible position accuracy loss : Marker %d - Fiducial %d - angle %f\n", ...
                                frame.markers(i).geometryId, iFid, angle);
                            isOk = false;
                            return;

                        end
                    end
                end
            end        
        end

        function isOk = checkTemperatures(this, frame)
            % (BP5) Checks if there's an error in frame statuses
            % Not all events are sent at each frame, you may need
            % to find their index in events array
            isOk = true;
            for i = 1:numel(frame.events)
                event = frame.events(i);

                if  event.type == "High Temperature"
    
                    fprintf("High Temperature, measures may deviate\n");
                    isOk = false;
                    return;

                end

                if  event.type == "Low Temperature"
    
                    fprintf("Low Temperature, measures may deviate\n");
                    isOk = false;
                    return;

                end

                if event.type == "Synthetic Temperatures"
    
                    synthTempData = event.data;
    
                    if synthTempData.currentValue > synthTempData.referenceValue + this.TEMP_DEVIATION_TOL ...
                        || synthTempData.currentValue < synthTempData.referenceValue - this.TEMP_DEVIATION_TOL

                        fprintf("Current Temperature %f°C and reference %f°C, measures may deviate\n", ...                        
                            synthTempData.currentValue, synthTempData.referenceValue );
                        isOk = false;
                        return;

                    end
                end
            end
            
        end

        function isOk = checkDysf(this, frame)
            % (BP6) Checks known dysfunction
            %   1. Receiving twice the same frame

            isOk= true;
            if (this.frameTimestamp == frame.imageHeader.timestamp)

                    isOk = false;
                    fprintf("Dysfunction : received twice the same frame\n" );
            else 

                this.frameTimestamp = frame.imageHeader.timestamp;

            end       
        end
        
        function allOk = basicChecks( this, frame, geometries)

            allOk = true;
            if ~checkStatuses(this, frame) ...
                || ~checkDysf(this, frame) ...
                || ~checkTemperatures(this, frame)

                allOk = false;

            end

            if size(frame.markers) ~= 0

                if ~checkTiltAngles(this, frame, geometries)

                    allOk = false;

                end

            end
        end    

    end
end


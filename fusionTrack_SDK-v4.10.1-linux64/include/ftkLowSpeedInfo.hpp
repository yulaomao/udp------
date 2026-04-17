#pragma once

#include <ftkPlatform.h>
#include <ftkTypes.h>

PACK1_STRUCT_BEGIN( LowSpeedMarkerStatus )
{
    uint8 Registered : 1;
    uint8 Paired : 1;
    uint8 Data_read : 1;
    uint8 Connected : 1;
    uint8 Activated : 1;
    uint8 Tracked : 1;
    uint8 Err_war_detected : 1;
    uint8 Reserved : 1;
}
PACK1_STRUCT_END( LowSpeedMarkerStatus );

PACK1_STRUCT_BEGIN( LowSpeedMarkerInfo )
{
    LowSpeedMarkerStatus Status;
    uint8 SID;
    uint16 Reserved;
    uint64 SN;
    uint64 HW_version;
    uint32 FW_version;
    uint32 FW_timestamp;
    uint16 API_vers_min;
    uint16 API_vers_maj;
    uint16 Power_cycle_cnt;
    uint8 Bat_war_trig_level;
    uint8 Bat_war_rel_level;
}
PACK1_STRUCT_END( LowSpeedMarkerInfo );


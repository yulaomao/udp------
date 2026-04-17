#pragma once

#include <ftkPlatform.h>
#include <ftkTypes.h>

PACK1_STRUCT_BEGIN( WirelessNvramHeader )
{
    uint32 Short_ID:8;
    uint32 Reserved:24;
    uint64 WM_sn;
    uint32 WM_firmVersion;
    uint32 WM_firmTimestamp;
    uint64 WM_elecVersion;
}
PACK1_STRUCT_END( WirelessNvramHeader );

PACK1_STRUCT_BEGIN( WirelessMarkerData )
{
    WirelessNvramHeader Header;
    uint8_t Data[ 256u ];
}
PACK1_STRUCT_END( WirelessMarkerData );


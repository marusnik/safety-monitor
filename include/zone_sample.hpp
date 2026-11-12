#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <atomic>
#include <cstdint>

#include "zone_monitor.hpp"
#include "clock.hpp"


/*************************************************************************************
                                      CONSTANTS
*************************************************************************************/
/************************************************************
Stale limit for a zone
************************************************************/
inline constexpr std::uint32_t STALE_ZONE_LIMIT_MS = 500;


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
/************************************************************
 A zone plus the time it was published, kept in one atomic
 so that a reader can never pair a new zone with an old
 timestamp, and can tell when the writer stopped updating
************************************************************/
struct ZoneSample
    {
    Zone            zone;
    std::uint32_t   time_ms;    // steady_clock time when published
    };

/************************************************************
 Lock-free, so that no hidden mutex sits in the safety path
************************************************************/
static_assert( std::atomic<ZoneSample>::is_always_lock_free, "ZoneSample atomic must be lock-free" );


/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

inline bool is_stale( const std::uint32_t    time_ms, 
                      const std::uint32_t    now, 
                      const std::uint32_t    limit )
    {
    /************************************************************
     now wraps every 2^32 miliseconds (~49 days)
     2^32 / ( 1000 * 60 * 60 * 24 ) = 49.71 days

     There might be a period of 'limit' ms where the return value
     is not correct every 49.7 days, if the writer
     hangs and doesn't update the zone/camera timestamp the whole 
     time.

     TODO: This is highly unlikely, but look if hardware would 
     support 64 bit time, or reader should stay in STOP until it 
     sees evidence of live writer (like a different timestamp).
     2^64 / ( 1000 * 60 * 60 * 24 ) = 585 million years
     
    ************************************************************/
    return( now - time_ms > limit);

    }
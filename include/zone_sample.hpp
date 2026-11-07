#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <atomic>
#include <chrono>
#include <cstdint>

#include "zone_monitor.hpp"


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
/************************************************************
 steady_clock, because a system clock change (NTP) could
 otherwise make old data look fresh
************************************************************/
inline std::uint32_t now_ms()
    {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch() ).count() );
    }

/************************************************************
 Reader side: returns STOP if the sample is too old, because
 a hung writer can't report its own failure
************************************************************/
inline Zone current_zone( const std::atomic<ZoneSample>& latest )
    {
    const ZoneSample sample = latest.load();

    /************************************************************
     now_ms wraps every 2^32 miliseconds (~49 days)
     2^32 / ( 1000 * 60 * 60 * 24 ) = 49.71 days

     There might be a period of STALE_ZONE_LIMIT_MS ms where 
     current zone is not correct every 49.7 days, if the writer
     hangs and doesn't update the zone timestamp the whole time.

     TODO: This is highly unlikely, but look if hardware would 
     support 64 bit time, or reader should stay in STOP until it 
     sees evidence of live writer (like a different timestamp).
     2^64 / ( 1000 * 60 * 60 * 24 ) = 585 million years
     
    ************************************************************/
    if ( now_ms() - sample.time_ms > STALE_ZONE_LIMIT_MS )
        {
        return( Zone::STOP );
        }

    return( sample.zone );

    }
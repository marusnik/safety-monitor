#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <atomic>


/*************************************************************************************
                                      CONSTANTS
*************************************************************************************/
/************************************************************
Stale limit for a camera
************************************************************/
inline constexpr std::uint32_t STALE_CAMERA_LIMIT_MS = 500;


/*************************************************************************************
                                      CLASSES
*************************************************************************************/

/************************************************************
 A person plus the time it was detected, kept in one atomic
 so that a reader can never pair a new detection with an old
 timestamp, and can tell when the writer stopped updating
************************************************************/
struct CameraSample
    {
    bool            person;     // at least one person detected
    std::uint32_t   time_ms;
    };

/************************************************************
 Lock-free atomic
************************************************************/
static_assert( std::atomic<CameraSample>::is_always_lock_free, "CameraSample atomic must be lock-free" );
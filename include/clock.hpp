#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <chrono>
#include <cstdint>

/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

/************************************************************
 Milliseconds of a steady_clock time point, wrapped to 32 bits.
 One conversion for every timestamp, so that times taken in
 different places can be subtracted safely
************************************************************/
inline std::uint32_t to_ms( std::chrono::steady_clock::time_point t )
    {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            t.time_since_epoch() ).count() );
    }

/************************************************************
 steady_clock, because a system clock change (NTP) could
 otherwise make old data look fresh
************************************************************/
inline std::uint32_t now_ms()
    {
    return to_ms( std::chrono::steady_clock::now() );
    }
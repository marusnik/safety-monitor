#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>

#include "zone_monitor.hpp"


/*************************************************************************************
                                      CONSTANTS
*************************************************************************************/

constexpr std::uint32_t LATCH_FAULT_LIMIT_MS = 500;


/*************************************************************************************
                                      CLASSES
*************************************************************************************/

enum class LatchState : std::uint8_t { OK, 
                                       FAULT_PENDING, 
                                       LATCHED };

class SafetyLatch 
    {
    public:
        Zone update_latch( bool             fault, 
                           bool             reset_pressed, 
                           std::uint32_t    now, 
                           Zone             normal_output );
        LatchState state() const { return state_; }

    private:
        LatchState        state_          = LatchState::OK;
        std::uint32_t     fault_start_ms_ = 0;
    };
#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <optional>

#include "range_status.hpp"

/*************************************************************************************
                                    CLASSES
*************************************************************************************/

class GlitchFilter
    {
    public:
        bool hold_reading( std::optional<RangeStatus> status );

    private:
        bool prev_valid_ = false;
        bool prev_held_  = false;
    };
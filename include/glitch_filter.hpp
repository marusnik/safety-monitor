#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <optional>


/*************************************************************************************
                                    CLASSES
*************************************************************************************/

enum class RangeStatus : std::uint8_t { OK                  = 0, 
                                        SIGMA_WARNING       = 1,    /* warning: decreasing singal to noise ratio             */  
                                        SIGNAL_WARNING      = 2,    /* warning: return signal is too weak                    */
                                        OUT_OF_BOUNDS_ERROR = 4,    /* error: sensor is ranging in a "non-appropriated" zone */
                                        WRAPAROUND_ERROR    = 7 };  /* target is very reflective and far away                */
                                        

class GlitchFilter
    {
    public:
        bool hold_reading( std::optional<RangeStatus> status );

    private:
        bool prev_valid_ = false;
        bool prev_held_  = false;
    };
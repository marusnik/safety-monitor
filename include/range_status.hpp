#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>

/*************************************************************************************
                                    CLASSES
*************************************************************************************/

enum class RangeStatus : std::uint8_t { OK                  = 0, 
                                        SIGMA_WARNING       = 1,    /* warning: decreasing signal to noise ratio             */  
                                        SIGNAL_WARNING      = 2,    /* warning: return signal is too weak                    */
                                        OUT_OF_BOUNDS_ERROR = 4,    /* error: sensor is ranging in a "non-appropriated" zone */
                                        WRAPAROUND_ERROR    = 7 };  /* target is very reflective and far away                */
                                        
#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include "zone_monitor.hpp"
#include "safety_latch.hpp"


/*************************************************************************************
                                     CONSTANTS
*************************************************************************************/

inline constexpr const char* RED         = "\033[31m";
inline constexpr const char* GREEN       = "\033[32m";
inline constexpr const char* YELLOW      = "\033[33m";
inline constexpr const char* COLOR_RESET = "\033[0m";


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       zone_name()
*
*   DESCRIPTION:
*       Returns a printable name for a zone
*       TODO: move all the *_name() and *_color() into a separate file
*
*****************************************************************************/
const char* zone_name( Zone zone );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       zone_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so zone changes stand out in the log
*
*****************************************************************************/

const char* zone_color( Zone z );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_name()
*
*   DESCRIPTION:
*       Returns a printable name for a person present or not
*
*****************************************************************************/

const char* cam_name( bool person_present );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so person detection stand out in the log
*
*****************************************************************************/

const char* cam_color( bool person_present );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_name()
*
*   DESCRIPTION:
*       Returns a printable name for the current latch state
*
*****************************************************************************/

const char* latch_name( LatchState s );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so latch state changes stand out in the log
*
*****************************************************************************/

const char* latch_color( LatchState latch_state );
/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include "console_format.hpp"
#include "zone_monitor.hpp"
#include "safety_latch.hpp"


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

const char* zone_name( Zone zone )
    {
    switch ( zone )
        {
        case Zone::CLEAR: return "CLEAR";
        case Zone::SLOW:  return "SLOW";
        case Zone::STOP:  return "STOP";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* zone_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       zone_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so zone changes stand out in the log
*
*****************************************************************************/

const char* zone_color( Zone z )
    {
    switch ( z )
        {
        case Zone::CLEAR: return GREEN;     
        case Zone::SLOW:  return YELLOW;
        case Zone::STOP:  return RED;       
        }

    return "\033[0m";

    } /* zone_color() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_name()
*
*   DESCRIPTION:
*       Returns a printable name for a person present or not
*
*****************************************************************************/

const char* cam_name( bool person_present )
    {
    switch ( person_present )
        {
        case true:  return "Person Present";
        case false: return "Person NOT Present";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* cam_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so person detection stand out in the log
*
*****************************************************************************/

const char* cam_color( bool person_present )
    {
    switch ( person_present )
        {
        case true:  return GREEN;
        case false: return RED;
        }

    return "\033[0m";

    } /* cam_color() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_name()
*
*   DESCRIPTION:
*       Returns a printable name for the current latch state
*
*****************************************************************************/

const char* latch_name( LatchState s )
    {
    switch ( s )
        {
        case LatchState::OK:            return "OK";
        case LatchState::FAULT_PENDING: return "FAULT_PENDING";
        case LatchState::LATCHED:       return "LATCHED";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* latch_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so latch state changes stand out in the log
*
*****************************************************************************/

const char* latch_color( LatchState latch_state )
    {
    switch ( latch_state )
        {
        case LatchState::OK:             return GREEN;
        case LatchState::FAULT_PENDING:  return YELLOW;
        case LatchState::LATCHED:        return RED;
        }

    return "\033[0m";

    } /* latch_color() */
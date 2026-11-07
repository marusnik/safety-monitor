/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include "zone_monitor.hpp"


/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       update()
*
*   DESCRIPTION:
*       Update ... TODO
*
*****************************************************************************/

Zone ZoneMonitor::update( std::optional<std::uint16_t> distance_mm )
    {

    if ( !distance_mm )
        {
        curr_zone_ = Zone::STOP;
        return( curr_zone_ );        
        }

    const std::uint16_t    dist = *distance_mm;

    if ( dist < STOP_DIST
      || ( dist < STOP_DIST + HYSTERESIS 
        && curr_zone_ == Zone::STOP ) ) 
        {
        curr_zone_ = Zone::STOP;
        return( curr_zone_ );
        }

    else if ( dist < CLEAR_DIST
           || ( dist < CLEAR_DIST + HYSTERESIS 
             && ( curr_zone_ == Zone::SLOW 
               || curr_zone_ == Zone::STOP ) ) )
        {
        curr_zone_ = Zone::SLOW;
        return( curr_zone_ );
        }

    else
        {
        curr_zone_ = Zone::CLEAR;
        return( curr_zone_ );
        }

    } /* update() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       zone()
*
*   DESCRIPTION:
*       Return the current zone. Don't allow this function to modify members 
*       of it's class with const
*
*****************************************************************************/

Zone ZoneMonitor::zone() const
    {

    return( curr_zone_ );

    }/* zone() */
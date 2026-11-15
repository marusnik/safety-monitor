/*************************************************************************************
                                    INCLUDES
*************************************************************************************/
#include "glitch_filter.hpp"
#include "range_status.hpp"


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       hold_reading()
*
*   DESCRIPTION:
*       Ignore this reading?
*
*****************************************************************************/

bool GlitchFilter::hold_reading( std::optional<RangeStatus> status )
    {
    /************************************************************
     Hold only a single wraparound right after a valid reading;
     a second one in a row, or one with no valid reading before
     it, is treated as a fault
     Wrap-around error is noticed almost consistently whenever 
     object or human enters/exits sensor's cone
     Other errors/warnings can be added if considered not usefull
    ************************************************************/
    const bool hold = status
                   && *status == RangeStatus::WRAPAROUND_ERROR
                   && prev_valid_
                   && !prev_held_;

    /************************************************************
     Update history for every reading, held or not
    ************************************************************/
    prev_valid_ = status && *status == RangeStatus::OK;
    prev_held_  = hold;

    return( hold );

    } /* hold_reading() */
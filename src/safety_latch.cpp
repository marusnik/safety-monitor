/*************************************************************************************
                                    INCLUDES
*************************************************************************************/
#include <cstdint>

#include "safety_latch.hpp"
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

Zone SafetyLatch::update_latch( bool             fault, 
                                bool             reset_pressed, 
                                std::uint32_t    now, 
                                Zone             normal_output )
    {
    switch ( state_ )
        {
        case LatchState::OK:
            /************************************************************
             Fault happened
            ************************************************************/
            if ( fault )
                {
                /************************************************************
                 Record fault start, and ignore reset
                ************************************************************/
                fault_start_ms_ = now;
                state_          = LatchState::FAULT_PENDING;
                }
            break;

        case LatchState::FAULT_PENDING:
            /************************************************************
             If fault is happening for some time, latch
            ************************************************************/
            if ( fault ) 
                {
                if ( now - fault_start_ms_ >= LATCH_FAULT_LIMIT_MS )
                    {
                    state_ = LatchState::LATCHED;
                    }
                }
            else
                {
                /************************************************************
                 Faulting stopped, go back to OK
                ************************************************************/
                state_ = LatchState::OK;
                }
            break;

        case LatchState::LATCHED:
            /************************************************************
             Only unlatch if not faulting and the reset was pressed
            ************************************************************/
            if ( !fault 
              &&  reset_pressed )
                {
                state_ = LatchState::OK;
                }
            break;

        default:
            /************************************************************
             Memory corruption or a bug. Treat as a fault
            ************************************************************/
            state_ = LatchState::LATCHED;
            break;

        } /* switch() */

    /************************************************************
     Stop for any other latch state but OK
    ************************************************************/
    if ( LatchState::OK == state_ )
        {
        return( normal_output );
        }
    else
        {
        return( Zone::STOP );
        }

    } /* update_latch() */
/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <cassert>

#include "safety_latch.hpp"
#include "zone_sample.hpp"


/*************************************************************************************
                                      CONSTANTS
*************************************************************************************/
constexpr Zone             NORMAL = Zone::CLEAR;
constexpr std::uint32_t    T0     = 1000;


/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       make_pending()
*
*   DESCRIPTION:
*       Bring a fresh latch into FAULT_PENDING, with the fault starting at T0
*
*****************************************************************************/

static void make_pending( SafetyLatch& latch )
    {

    latch.update_latch( true, false, T0, NORMAL );
    assert( latch.state() == LatchState::FAULT_PENDING );

    } /* make_pending() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       make_latched()
*
*   DESCRIPTION:
*       Bring a fresh latch into LACHED
*
*****************************************************************************/

static void make_latched( SafetyLatch& latch )
    {

    make_pending( latch );
    latch.update_latch( true, false, T0 + LATCH_FAULT_LIMIT_MS, NORMAL );
    assert( latch.state() == LatchState::LATCHED );

    } /* make_latched() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       main()
*
*   DESCRIPTION:
*       Test all the possible combinations of inputs (state_, fault, reset)
*
*    # | Current state | Faulting now?       | Reset? | Next state                                  | Output
*   ---|---------------|---------------------|--------|---------------------------------------------|--------------
*    1 | OK            | no                  | no     | OK                                          | safety_output
*    2 | OK            | no                  | yes    | OK (reset ignored)                          | safety_output
*    3 | OK            | yes                 | no     | FAULT_PENDING (record start)                | STOP
*    4 | OK            | yes                 | yes    | FAULT_PENDING (record start, reset ignored) | STOP
*    5 | FAULT_PENDING | yes, under 1 s      | no     | FAULT_PENDING                               | STOP
*    6 | FAULT_PENDING | yes, under 1 s      | yes    | FAULT_PENDING (reset ignored)               | STOP
*    7 | FAULT_PENDING | yes, 1 s or more    | no     | LATCHED                                     | STOP
*    8 | FAULT_PENDING | yes, 1 s or more    | yes    | LATCHED (reset ignored)                     | STOP
*    9 | FAULT_PENDING | no                  | no     | OK                                          | safety_output
*   10 | FAULT_PENDING | no                  | yes    | OK (reset ignored)                          | safety_output
*   11 | LATCHED       | yes                 | no     | LATCHED                                     | STOP
*   12 | LATCHED       | yes                 | yes    | LATCHED (reset ignored)                     | STOP
*   13 | LATCHED       | no                  | no     | LATCHED                                     | STOP
*   14 | LATCHED       | no                  | yes    | OK                                          | safety_output
*
*   TODO: Make this a for loop test since all the data can be placed in a table
*
*****************************************************************************/

int main()
    {
    /************************************************************
     Tests for row 1 in table
    ************************************************************/
    {
    SafetyLatch latch;
    assert( latch.update_latch( false, false, T0, NORMAL ) == NORMAL );
    assert( latch.state() == LatchState::OK );
    }

    /************************************************************
     Tests for row 2 in table
    ************************************************************/
    {
    SafetyLatch latch;
    assert( latch.update_latch( false, true, T0, NORMAL ) == NORMAL );
    assert( latch.state() == LatchState::OK );
    }

    /************************************************************
     Tests for row 3 in table
    ************************************************************/
    {
    SafetyLatch latch;
    assert( latch.update_latch( true, false, T0, NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    /************************************************************
     Tests for row 4 in table
    ************************************************************/
    {
    SafetyLatch latch;
    assert( latch.update_latch( true, true, T0, NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    /************************************************************
     Tests for row 5 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( true, false, ( T0 + LATCH_FAULT_LIMIT_MS - 1 ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    /************************************************************
     Tests for row 6 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( true, true, ( T0 + LATCH_FAULT_LIMIT_MS - 1 ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    /************************************************************
     Tests for row 7 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( true, false, ( T0 + LATCH_FAULT_LIMIT_MS ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Tests for row 8 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( true, true, ( T0 + LATCH_FAULT_LIMIT_MS ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Tests for row 9 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( false, false, ( T0 + 100 ), NORMAL ) == NORMAL );
    assert( latch.state() == LatchState::OK );
    }

    /************************************************************
     Tests for row 10 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_pending( latch );
    assert( latch.update_latch( false, true, ( T0 + 100 ), NORMAL ) == NORMAL );
    assert( latch.state() == LatchState::OK );
    }

    /************************************************************
     Tests for row 11 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_latched( latch );
    assert( latch.update_latch( true, false, ( T0 + LATCH_FAULT_LIMIT_MS + 100 ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Tests for row 12 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_latched( latch );
    assert( latch.update_latch( true, true, ( T0 + LATCH_FAULT_LIMIT_MS + 100 ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Tests for row 13 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_latched( latch );
    assert( latch.update_latch( false, false, ( T0 + LATCH_FAULT_LIMIT_MS + 100 ), NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Tests for row 14 in table
    ************************************************************/
    {
    SafetyLatch latch;
    make_latched( latch );
    assert( latch.update_latch( false, true, ( T0 + LATCH_FAULT_LIMIT_MS + 100 ), NORMAL ) == NORMAL );
    assert( latch.state() == LatchState::OK );
    }

    /************************************************************
     Test latched state; when reset is pressed while faulting, or
     reset not pressed when it's not faulting 
     Reset pressed during a fault isn’t applied when the fault 
     clears later
    ************************************************************/
    {
    SafetyLatch latch;
    make_latched( latch );
    latch.update_latch( true,  true,  T0 + 2000, NORMAL );
    assert( latch.update_latch( false, false, T0 + 2100, NORMAL ) == Zone::STOP );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Test the fault starting just before the timer wrap-around
    ************************************************************/
    {
    SafetyLatch latch;
    const std::uint32_t start = 0xFFFFFF00;              
    latch.update_latch( true, false, start, NORMAL );
    latch.update_latch( true, false, start + LATCH_FAULT_LIMIT_MS - 1, NORMAL );  
    assert( latch.state() == LatchState::FAULT_PENDING );
    latch.update_latch( true, false, start + LATCH_FAULT_LIMIT_MS, NORMAL );
    assert( latch.state() == LatchState::LATCHED );
    }

    /************************************************************
     Fault at T0, clear at T0 + 100, then a new fault at 
     T0 + limit + 200 -> must be FAULT_PENDING, not LATCHED.
    ************************************************************/
    {
    SafetyLatch latch;
    latch.update_latch( true,  false,   T0,         NORMAL );
    latch.update_latch( false, false, ( T0 + 100 ), NORMAL );
    latch.update_latch( true,  false, ( T0 + 200 ), NORMAL );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    /************************************************************
     Test default switch case
    ************************************************************/
    {
    SafetyLatch latch;
    latch.update_latch( true,  false,   T0,         NORMAL );
    latch.update_latch( false, false, ( T0 + 100 ), NORMAL );
    latch.update_latch( true,  false, ( T0 + 200 ), NORMAL );
    assert( latch.state() == LatchState::FAULT_PENDING );
    }

    return( 0 );
    
    } /* main() */
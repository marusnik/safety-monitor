/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <optional>
#include <cassert>

#include "zone_monitor.hpp"


/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       main()
*
*   DESCRIPTION:
*       Test ... TODO
*
*****************************************************************************/

int main()
    {
    ZoneMonitor     monitor;
    std::uint16_t   idx;


    // Tests for SLOW <-> CLEAR boundary
    assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS + 1 ) == Zone::CLEAR );

    for( idx = 0; idx < ZoneMonitor::HYSTERESIS; idx++ )
        {
        assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS - idx ) == Zone::CLEAR );
        }

    assert( monitor.update( ZoneMonitor::CLEAR_DIST - std::uint16_t( 1 ) ) == Zone::SLOW );

    for( idx = 0; idx < ZoneMonitor::HYSTERESIS; idx++ )
        {
        assert( monitor.update( ZoneMonitor::CLEAR_DIST + idx ) == Zone::SLOW );
        }

    assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS ) == Zone::CLEAR );



    // Tests for STOP <-> SLOW boundary
    assert( monitor.update( ZoneMonitor::STOP_DIST + ZoneMonitor::HYSTERESIS + 1 ) == Zone::SLOW );

    for( idx = 0; idx < ZoneMonitor::HYSTERESIS; idx++ )
        {
        assert( monitor.update( ZoneMonitor::STOP_DIST + ZoneMonitor::HYSTERESIS - idx ) == Zone::SLOW );
        }

    assert( monitor.update( ZoneMonitor::STOP_DIST - std::uint16_t( 1 ) ) == Zone::STOP );

    for( idx = 0; idx < ZoneMonitor::HYSTERESIS; idx++ )
        {
        assert( monitor.update( ZoneMonitor::STOP_DIST + idx ) == Zone::STOP );
        }

    assert( monitor.update( ZoneMonitor::STOP_DIST + ZoneMonitor::HYSTERESIS ) == Zone::SLOW );



    // Tests Normal and Corner Cases
    assert( monitor.zone() == Zone::SLOW );
    assert( monitor.update( ZoneMonitor::STOP_DIST  - std::uint16_t( 5 ) ) == Zone::STOP );
    assert( monitor.update( ZoneMonitor::STOP_DIST  + std::uint16_t( 5 ) ) == Zone::STOP );
    assert( monitor.update( ZoneMonitor::STOP_DIST  + std::uint16_t( 200 ) ) == Zone::SLOW );
    assert( monitor.zone() == Zone::SLOW );
    assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS - std::uint16_t( 1 ) ) == Zone::SLOW );
    assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS  ) == Zone::CLEAR );
    assert( monitor.zone() == Zone::CLEAR );
    assert( monitor.update( ZoneMonitor::STOP_DIST  - std::uint16_t( 5 ) ) == Zone::STOP );
    assert( monitor.update( ZoneMonitor::CLEAR_DIST + ZoneMonitor::HYSTERESIS - std::uint16_t( 1 ) ) == Zone::SLOW );
    assert( monitor.update( std::nullopt ) == Zone::STOP );
    assert( monitor.zone() == Zone::STOP );


    return( 0 );
    
    } /* main() */
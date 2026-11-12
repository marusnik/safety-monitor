/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <optional>
#include <cassert>

#include "safety_output.hpp"
#include "zone_sample.hpp"
#include "camera_sample.hpp"


/*************************************************************************************
                                      GLOBALS
*************************************************************************************/
ZoneSample     g_sensor;
CameraSample   g_camera;
std::uint32_t  g_now_ms;

/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       reset_test()
*
*   DESCRIPTION:
*       Reset test variables
*
*****************************************************************************/

static int reset_test()
    {
    g_sensor.time_ms = 0;
    g_sensor.zone    = Zone::STOP;
    g_camera.time_ms = 0;
    g_camera.person  = false;
    g_now_ms         = 0;

    return( 0 );
    }


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

    /************************************************************
     Tests for sensor sample being stale
    ************************************************************/
    reset_test();
    g_now_ms = STALE_ZONE_LIMIT_MS + 1; // stale zone sample
    g_sensor.zone = Zone::CLEAR;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::STOP );

    /************************************************************
     Tests for camera sample being stale
    ************************************************************/
    reset_test();
    g_sensor.time_ms = 1; // zone sample not stale
    g_now_ms = STALE_ZONE_LIMIT_MS + 1; // stale camera sample
    g_sensor.zone = Zone::CLEAR;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::STOP );

    /************************************************************
     Tests for sensor reporting STOP
    ************************************************************/
    reset_test();
    g_now_ms      = STALE_ZONE_LIMIT_MS; 
    g_sensor.zone = Zone::STOP;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::STOP );

    reset_test();
    g_camera.person = true;
    g_sensor.zone = Zone::STOP;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::STOP );

    /************************************************************
     Tests for camera detecting a person while sensor is not 
     reporting STOP
    ************************************************************/
    reset_test();
    g_camera.person = true;
    g_sensor.zone = Zone::SLOW;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::SLOW );

    reset_test();
    g_camera.person = true;
    g_sensor.zone = Zone::CLEAR;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::SLOW );

    /************************************************************
     Tests for camera not detecting a person while sensor is not 
     reporting STOP. Trust the sensor if camera not detecting.
    ************************************************************/
    reset_test();
    g_camera.person = false;
    g_sensor.zone = Zone::SLOW;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::SLOW );

    reset_test();
    g_camera.person = false;
    g_sensor.zone = Zone::CLEAR;
    assert( safety_output( g_sensor, g_camera, g_now_ms ) == Zone::CLEAR );

    return( 0 );
    
    } /* main() */
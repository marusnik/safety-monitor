/*************************************************************************************
                                    INCLUDES
*************************************************************************************/
#include "safety_output.hpp"
#include "zone_sample.hpp"
#include "camera_sample.hpp"
#include "clock.hpp"


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

Zone safety_output( ZoneSample     sensor, 
                    CameraSample   camera, 
                    std::uint32_t  now_ms )
    {

    /************************************************************
     STOP if sensor detects something close (regardless of what 
     camera detects), or any samples are stale
    ************************************************************/
    if ( ( now_ms - sensor.time_ms ) > STALE_ZONE_LIMIT_MS
      || ( now_ms - camera.time_ms ) > STALE_CAMERA_LIMIT_MS 
      || sensor.zone == Zone::STOP )
        {
        return( Zone::STOP );
        }
    /************************************************************
     In case person is off to the side (outside the sensor's cone 
     (27 degrees)) and detected only by the camera, return SLOW. 
     This is a compromise between endangering person potentially 
     close and being slow when people are around (even far away)

     NOTE: Whether that’s acceptable depends on how busy the 
     area is
    ************************************************************/
    else if ( camera.person == true 
           && ( sensor.zone == Zone::CLEAR
             || sensor.zone == Zone::SLOW ))
        {
        return( Zone::SLOW );
        }
    /************************************************************
     If camera doesn't detect anything but sensor detects 
     something is in clear or slow range, trust the sensor
    ************************************************************/
    else
        {
        return( sensor.zone );
        }

    } /* safety_output() */
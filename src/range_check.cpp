/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <iostream>
#include <cstdint>
#include <exception>
#include <format>

#include "i2c_device.hpp"
#include "vl53l1_platform_attachment.hpp"
#include "vl53l1x_api.hpp"


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       main()
*
*   DESCRIPTION:
*       Checks if time of flight sensor has booted, if it has the right ID, 
*       and if initialization went ok
*
*****************************************************************************/

int main()
    {

    try
        {
        /************************************************************
         Create sensor device object
        ************************************************************/
        I2cDevice sensor( "/dev/i2c-1", ToF_SENSOR_ADDR );

        /************************************************************
         Point global g_device to sensor
        ************************************************************/
        Vl53l1PlatformAttachment attachment( sensor );

        /************************************************************
         Local variables
        ************************************************************/
        std::uint8_t       sensor_boot_state;
        VL53L1X_ERROR      sensor_boot_err;
        std::uint16_t      sensor_id;
        bool               booted = false;
        constexpr uint32_t MIN_BOOT_TIMEOUT_MS = 1000;

        /************************************************************
         Check if time of flight sensor booted for at least 
         MIN_BOOT_TIMEOUT_MS number of milli seconds
        ************************************************************/
        for ( uint32_t min_elapsed_ms = 0; min_elapsed_ms < MIN_BOOT_TIMEOUT_MS; min_elapsed_ms++ )
            {
            sensor_boot_err = VL53L1X_BootState( ToF_SENSOR_ADDR, &sensor_boot_state );

            if( 0 != sensor_boot_err )
                {
                std::cerr << std::format( "Could not read the sensor boot state. Got an error: 0x{:02X}\n", sensor_boot_err );
                return( 1 );
                }

            if ( sensor_boot_state != 0 )
                {
                booted = true;
                break;
                }

            VL53L1_WaitMs( ToF_SENSOR_ADDR, 1 );

            }

        /************************************************************
         Report and exit if time of flight sensor didn't boot 
        ************************************************************/
        if ( !booted )
            {
            std::cerr << "Sensor did NOT boot after at least "<< MIN_BOOT_TIMEOUT_MS << " ms\n";
            return( 1 );
            }
        
        /************************************************************
         If sensor booted, get its ID
        ************************************************************/
        const VL53L1X_ERROR sensor_id_err = VL53L1X_GetSensorId( ToF_SENSOR_ADDR, &sensor_id );

        if ( 0 != sensor_id_err )
            {
            std::cerr << std::format( "Could not read the sensor ID. Got an error: 0x{:02X}\n", sensor_id_err );
            return( 1 );
            }

        /************************************************************
         Check if it is the right sensor 
        ************************************************************/
        if ( EXPECTED_SENSOR_ID != sensor_id )
            {
            std::cerr << std::format( "Wrong sensor ID: 0x{:04X} instead of 0x{:04X}\n", sensor_id, EXPECTED_SENSOR_ID );
            return( 1 );
            }

        /************************************************************
         Initialize the sensor
        ************************************************************/
        const VL53L1X_ERROR sensor_init_err = VL53L1X_SensorInit( ToF_SENSOR_ADDR );

        if ( 0 != sensor_init_err )
            {
            std::cerr << std::format( "Sensor init failed: status 0x{:02X}\n", sensor_init_err );
            return( 1 );
            }

        std::cout << "Sensor init OK\n";
        return( 0 );

        } /* try */

    catch ( const std::exception& e )
        {
        std::cerr << "Range check failed: " << e.what() << '\n';
        return ( 1 );
        }

    } /* main() */
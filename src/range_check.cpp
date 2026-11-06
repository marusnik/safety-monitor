/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <exception>
#include <format>
#include <stdexcept>
#include <iterator>

#include "i2c_device.hpp"
#include "vl53l1_platform_attachment.hpp"
#include "vl53l1x_api.hpp"


/*************************************************************************************
                                     CLASSES
*************************************************************************************/
class RangingSession
    {
    public:
        RangingSession()
            {
            const VL53L1X_ERROR err = VL53L1X_StartRanging( ToF_SENSOR_ADDR );

            if ( 0 != err )
                {
                throw std::runtime_error( std::format( "Could NOT start ranging. Got an error: 0x{:02X}", err ) );
                }
            }

        ~RangingSession()
            {
            const VL53L1X_ERROR err = VL53L1X_StopRanging( ToF_SENSOR_ADDR );

            if ( 0 != err )
                {
                std::cerr << std::format( "Could NOT stop ranging. Got an error: 0x{:02X}\n", err );
                }
            }

        RangingSession( const RangingSession& )            = delete;
        RangingSession& operator=( const RangingSession& ) = delete;
    };


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/


/*****************************************************************************
*
*   PROCEDURE NAME:
*       sensor_init()
*
*   DESCRIPTION:
*       Checks if time of flight sensor has booted, if it has the right ID, 
*       and if initialization went ok
*
*****************************************************************************/

static int sensor_init()
    {

    /************************************************************
     Local variables
    ************************************************************/
    std::uint8_t            sensor_boot_state;
    VL53L1X_ERROR           sensor_boot_err;
    std::uint16_t           sensor_id;
    bool                    booted = false;
    constexpr std::uint32_t MIN_BOOT_TIMEOUT_MS = 1000;

    /************************************************************
     Check if time of flight sensor booted for MIN_BOOT_TIMEOUT_MS 
     number of milli seconds
    ************************************************************/
    for ( std::uint32_t min_elapsed_ms = 0; min_elapsed_ms < MIN_BOOT_TIMEOUT_MS; min_elapsed_ms++ )
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

    } /* sensor_init() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       main()
*
*   DESCRIPTION:
*       main TODO...
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
         Initialize the sensor
        ************************************************************/
        if ( 0 != sensor_init() )
            {
            return( 1 );
            }

        /************************************************************
         Start ranging, RangingSession object is destroyed if 
         something fail or throw
        ************************************************************/
        RangingSession    ranging;
        
        /************************************************************
         Make and report NUM_OF_READINGS of readings
        ************************************************************/
        constexpr std::uint32_t NUM_OF_READINGS = 20;
        
        /************************************************************
         Summary of the readings; only valid readings count toward
         the min, max and average
        ************************************************************/
        std::uint32_t valid_count           = 0;
        std::uint32_t distance_sum          = 0;
        std::uint16_t min_distance          = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t max_distance          = 0;
        std::uint32_t status_counts[ 256 ]  = {};

        for ( std::uint32_t reading = 0; reading < NUM_OF_READINGS; reading++ )
            {
            /************************************************************
             Wait some amoutn of time for ranging data to be ready
            ************************************************************/
            constexpr std::uint32_t  NUM_OF_POLL_ATTEMPTS = 200;
            VL53L1X_ERROR            data_ready_err;
            std::uint8_t             is_data_ready;
            bool                     data_ready = false;

            for ( std::uint32_t poll_attempt = 0; poll_attempt < NUM_OF_POLL_ATTEMPTS; poll_attempt++ )
                {
                data_ready_err = VL53L1X_CheckForDataReady( ToF_SENSOR_ADDR, &is_data_ready );

                if( 0 != data_ready_err )
                    {
                    std::cerr << std::format( "Could not check if ranging data is ready. Got an error: 0x{:02X}\n", data_ready_err );
                    return( 1 );
                    }

                if ( 1 == is_data_ready )
                    {
                    data_ready = true;
                    break;
                    }

                VL53L1_WaitMs( ToF_SENSOR_ADDR, 1 );
                }

            /************************************************************
             Report and exit if time of flight sensor's ranging data was 
             not ready for some time
            ************************************************************/
            if ( !data_ready )
                {
                std::cerr << "Ranging data have NOT been ready in at least "<< NUM_OF_POLL_ATTEMPTS << " ms\n";
                return( 1 );
                }

            /************************************************************
             Get and report the distance and status 
            ************************************************************/
            VL53L1X_ERROR       range_status_err;
            std::uint8_t        range_status;
            VL53L1X_ERROR       distance_err;
            std::uint16_t       distance;
        
            range_status_err = VL53L1X_GetRangeStatus( ToF_SENSOR_ADDR, &range_status ); 

            if ( 0 != range_status_err )
                {
                std::cerr << std::format( "Range status error: 0x{:02X}\n", range_status_err );
                return( 1 );
                }

            distance_err = VL53L1X_GetDistance( ToF_SENSOR_ADDR, &distance );

            if ( 0 != distance_err )
                {
                std::cerr << std::format( "Distance error: 0x{:02X}\n", distance_err );
                return( 1 );
                }

            /************************************************************
             Count every status, but use the distance only if it is valid
            ************************************************************/
            status_counts[ range_status ]++;
            
            if ( 0 == range_status )
                {
                valid_count++;
                distance_sum += distance;
                min_distance  = std::min( min_distance, distance );
                max_distance  = std::max( max_distance, distance );
                }

            /************************************************************
             Finished with the measurement, clear the interrupt
            ************************************************************/
            const VL53L1X_ERROR clear_err = VL53L1X_ClearInterrupt( ToF_SENSOR_ADDR );

            if ( 0 != clear_err )
                {
                std::cerr << std::format( "Could NOT clear the interrupt. Got an error: 0x{:02X}\n", clear_err );
                return( 1 );
                }

            }

        /************************************************************
         Report the summary; without valid readings there is no
         distance to report, and dividing by zero must be avoided
        ************************************************************/
        std::cout << std::format( "valid {}/{}", valid_count, NUM_OF_READINGS );
        
        if ( valid_count > 0 )
            {
            const double avg_distance = static_cast<double>( distance_sum ) / valid_count;
        
            std::cout << std::format( "  min {}  max {}  avg {:.1f} mm", min_distance, max_distance, avg_distance );
            }
        
        std::cout << '\n';
        
        /************************************************************
         Report the invalid statuses that were seen
        ************************************************************/
        for ( std::size_t status = 1; status < std::size( status_counts ); status++ )
            {
            if ( status_counts[ status ] > 0 )
                {
                std::cout << std::format( "status {}: {} readings\n", status, status_counts[ status ] );
                }
            }

        return( 0 );

        } /* try */

    catch ( const std::exception& e )
        {
        std::cerr << "Range check failed: " << e.what() << '\n';
        return ( 1 );
        }

    } /* main() */
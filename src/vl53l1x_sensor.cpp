/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <iostream>
#include <cstdint>
#include <optional>
#include <format>
#include <stdexcept>
#include <string>

#include "vl53l1x_sensor.hpp"
#include "vl53l1x_api.hpp"


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       Vl53l1xSensor()
*
*   DESCRIPTION:
*       Initializer list in the class definition does all the work; opens the 
*       I2C bus, points global g_device to I2cDevice object, and initializes, 
*       starts/stops sensor ranging
*
*****************************************************************************/

Vl53l1xSensor::Vl53l1xSensor( const std::string&    bus_path, 
                              std::uint8_t          address ) : device_( bus_path, address ),
                                                                attachment_( device_ ),
                                                                ranging_( address ) 
      {
      /************************************************************
       All the work happens in the initializer list
       device_     - opens the I2C
       attachment_ - connects the ST driver to the sensor
       ranging_    - initializes the sensor and starts ranging
      ************************************************************/
      } /* Vl53l1xSensor() */


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
namespace{

int sensor_init( std::uint16_t address )
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
        sensor_boot_err = VL53L1X_BootState( address, &sensor_boot_state );

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

        VL53L1_WaitMs( address, 1 );
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
    const VL53L1X_ERROR sensor_id_err = VL53L1X_GetSensorId( address, &sensor_id );

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
    const VL53L1X_ERROR sensor_init_err = VL53L1X_SensorInit( address );
    
    if ( 0 != sensor_init_err )
        {
        std::cerr << std::format( "Sensor init failed: status 0x{:02X}\n", sensor_init_err );
        return( 1 );
        }

    std::cout << "Sensor init OK\n";

    return( 0 );

    } /* sensor_init() */

} /* namespace */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       RangingSession()
*
*   DESCRIPTION:
*       Initializes and starts sensor ranging
*
*****************************************************************************/

RangingSession::RangingSession( std::uint16_t address ) : address_( address )
    {
    /************************************************************
     Initialize sensor, throw if not successful
    ************************************************************/
    if ( 0 != sensor_init( address ) )
        {
        throw std::runtime_error( "Sensor init failed" );
        }

    /************************************************************
     Start ranging and throw if not successful
    ************************************************************/
    const VL53L1X_ERROR err = VL53L1X_StartRanging( address_ );

    if ( 0 != err )
        {
        /************************************************************
         Best-effort stop, because the write may have reached the
         sensor even though an error was reported, and the destructor
         won't run if this constructor throws. Stop's result is
         ignored because we're already failing with the start error.
        ************************************************************/
        (void)VL53L1X_StopRanging( address_ );
    
        throw std::runtime_error( std::format( "Could NOT start ranging. Got an error: 0x{:02X}", err ) );
        }

    } /* RangingSession() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       ~RangingSession()
*
*   DESCRIPTION:
*       Ranging destructor that stops ranging when object is out of scope
*
*****************************************************************************/

RangingSession::~RangingSession()
    {
    /************************************************************
     Stop ranging and report if not successfull
    ************************************************************/
    const VL53L1X_ERROR err = VL53L1X_StopRanging( address_ );

    if ( 0 != err )
        {
        std::cerr << std::format( "Could NOT stop ranging. Got an error: 0x{:02X}\n", err );
        }

    } /* ~RangingSession() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       read()
*
*   DESCRIPTION:
*       Waits for sensor data to be ready before atempting to read. Returns
*       distance and status of the measurement. Clears the interrupt at the end
*
*****************************************************************************/

std::optional<RangeReading> Vl53l1xSensor::read() 
    {
    /************************************************************
     Wait some amounth of time for ranging data to be ready
    ************************************************************/
    constexpr std::uint32_t  NUM_OF_POLL_ATTEMPTS = 200;
    VL53L1X_ERROR            data_ready_err;
    std::uint8_t             is_data_ready;
    bool                     data_ready = false;
    const std::uint8_t       address = device_.address();

    for ( std::uint32_t poll_attempt = 0; poll_attempt < NUM_OF_POLL_ATTEMPTS; poll_attempt++ )
        {
        data_ready_err = VL53L1X_CheckForDataReady( address, &is_data_ready );

        if( 0 != data_ready_err )
            {
            last_error_ = std::format( "Could not check if ranging data is ready. Got an error: {}", data_ready_err );
            return( std::nullopt );
            }

        if ( 1 == is_data_ready )
            {
            data_ready = true;
            break;
            }

        VL53L1_WaitMs( address, 1 );
        }

    /************************************************************
     Report and exit if time of flight sensor's ranging data was 
     not ready for some time
    ************************************************************/
    if ( !data_ready )
        {
        last_error_ = std::format( "Ranging data have NOT been ready in at least {} ms", NUM_OF_POLL_ATTEMPTS );
        return( std::nullopt );
        }

    VL53L1X_ERROR       range_status_err;
    std::uint8_t        range_status;
    VL53L1X_ERROR       distance_err;
    std::uint16_t       distance;

    /************************************************************
     Get distance and status 
    ************************************************************/
    range_status_err = VL53L1X_GetRangeStatus( address, &range_status ); 
    distance_err     = VL53L1X_GetDistance(    address, &distance     );

    /************************************************************
     Clear first before return, even after a failed read, 
     because otherwise the old result can look new if interrupt
     is not cleared
    ************************************************************/
    const VL53L1X_ERROR clear_err = VL53L1X_ClearInterrupt( address );

    if ( 0 != range_status_err 
      || 0 != distance_err 
      || 0 != clear_err )
        {
        last_error_ = std::format( "Read failed: status {}, distance {}, clear {}", range_status_err, 
                                                                                    distance_err, 
                                                                                    clear_err );
        return( std::nullopt );
        }

    return RangeReading{ range_status, distance };

    } /* read() */
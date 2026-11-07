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
#include "vl53l1x_sensor.hpp"


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/

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
         Create sensor object
        ************************************************************/
        Vl53l1xSensor sensor( "/dev/i2c-1", ToF_SENSOR_ADDR );

        /************************************************************
         Make and report NUM_OF_READINGS of distance readings
        ************************************************************/
        constexpr std::uint32_t NUM_OF_READINGS = 20;
    
        /************************************************************
         Summary of the readings; only valid readings count toward
         the min, max and average
        ************************************************************/
        std::uint32_t                  valid_count           = 0;
        std::uint32_t                  distance_sum          = 0;
        std::uint16_t                  min_distance          = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t                  max_distance          = 0;
        std::uint32_t                  status_counts[ 256 ]  = {};
        std::optional<RangeReading>    sensor_reading;


        for ( std::uint32_t reading = 0; reading < NUM_OF_READINGS; reading++ )
            {
            /************************************************************
             Read the distance and status of the measurement from ToF sensor
            ************************************************************/
            sensor_reading = sensor.read();

            if ( sensor_reading != std::nullopt )
                {
                /************************************************************
                 Count every status, but use the distance only if it is valid
                ************************************************************/
                status_counts[ sensor_reading->status ]++;
        
                if ( 0 == sensor_reading->status )
                    {
                    valid_count++;
                    distance_sum += sensor_reading->distance_mm;
                    min_distance  = std::min( min_distance, sensor_reading->distance_mm );
                    max_distance  = std::max( max_distance, sensor_reading->distance_mm );
                    }
                }

            } /* for */

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
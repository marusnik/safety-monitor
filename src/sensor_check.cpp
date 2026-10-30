/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <iostream>
#include <format>
#include <cstdint>
#include <exception>

#include "i2c_device.hpp"


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       main
*
*   DESCRIPTION:
*       Verifies the VL53L1X identity over I2C. Returns 0 only if the expected 
*       device answers.
*
*****************************************************************************/

int main()
    {

    try
        {
        constexpr std::uint8_t  SENSOR_ADDR          = 0x29;
        constexpr std::uint16_t SENSOR_MODEL_ID_REG  = 0x010F;
        constexpr std::uint8_t  EXPECTED_MODEL_ID    = 0xEA;
        constexpr std::uint8_t  EXPECTED_MODULE_TYPE = 0xCC;

        /************************************************************
         Create sensor device; 0x29 is sensor's (VL53L1X) default 
         address, and I2C bus is exposed in /dev/i2c-1 file
        ************************************************************/
        I2cDevice sensor( "/dev/i2c-1", SENSOR_ADDR );

        /************************************************************
         Read sensor's identity register (0x010F)
        ************************************************************/
        constexpr std::size_t READ_BYTES_SIZE = 2;
        std::uint8_t buffer[ READ_BYTES_SIZE ];

        const bool received = sensor.read_register( SENSOR_MODEL_ID_REG, buffer, READ_BYTES_SIZE );

        if ( !received )
            {
            std::cerr << std::format( "Register 0x{:04X} could NOT be read\n", SENSOR_MODEL_ID_REG );
            return( 1 );
            }

        /************************************************************
         Report sensor's model ID
        ************************************************************/
        std::cout << "Read model ID and type: " << std::format( "0x{:02X} 0x{:02X}\n", buffer[ 0 ], buffer[ 1 ] );

        if ( EXPECTED_MODEL_ID    != buffer[ 0 ]
          || EXPECTED_MODULE_TYPE != buffer[ 1 ] )
            {
            std::cerr << std::format( "Unexpected device: expected 0x{:02X} 0x{:02X}\n",
                                       EXPECTED_MODEL_ID, EXPECTED_MODULE_TYPE );
            return( 1 );
            }

        return( 0 );

        } /* try */

    catch ( const std::exception& e )
        {
        std::cerr << "Sensor check failed: " << e.what() << '\n';
        return ( 1 );
        }

    } /* main() */
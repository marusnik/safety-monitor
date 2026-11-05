/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <iostream>
#include <format>
#include <algorithm>
#include <cstdint>
#include <exception>

#include "vl53l1x_api.hpp"
#include "i2c_device.hpp"


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_bytes()
*
*   DESCRIPTION:
*       Report mismatch between written, original and last read register
*
*****************************************************************************/

static void print_bytes( const char*         label, 
                         const std::uint8_t* bytes )
    {
    std::cerr << std::format( "{}: 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X}\n", label,
                                                                           bytes[ 0 ], 
                                                                           bytes[ 1 ], 
                                                                           bytes[ 2 ], 
                                                                           bytes[ 3 ] );
    } /* print_bytes() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       main()
*
*   DESCRIPTION:
*       A quick test to make sure it is possible to write to the time of flight 
*       sensor register (intermeasurement period register). Original value of 
*       the register restored at the end of the test. Inputs to write_register
*       tested
*
*****************************************************************************/

int main()
    {
    try
        {
        /************************************************************
         Create sensor device; 0x29 is sensor's (VL53L1X) default 
         address, and I2C bus is exposed in /dev/i2c-1 file
        ************************************************************/
        I2cDevice sensor( "/dev/i2c-1", ToF_SENSOR_ADDR );

        std::uint8_t buffer[ REGISTER_SIZE ]           = {};
        std::uint8_t orig_buffer[ REGISTER_SIZE ]      = {};
        std::uint8_t pattern_readback[ REGISTER_SIZE ] = {};
        std::uint8_t restore_readback[ REGISTER_SIZE ] = {};

        /************************************************************
         Before test starts check inputs for write_register()
         null pointer passed
        ************************************************************/
        if ( sensor.write_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, nullptr, REGISTER_SIZE ) )
            { 
            std::cerr << "Passing the null pointer should fail, and it didn't. Something is wrong.\n";
            return( 1 );
            }

        /************************************************************
         Passed array is too big
        ************************************************************/
        std::uint8_t too_big[ REGISTER_SIZE + 1 ] = {};

        if ( sensor.write_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, too_big, sizeof( too_big ) ) )
            { 
            std::cerr << "Passing too big a buffer should fail, and it didn't. Something is wrong.\n";
            return( 1 );
            }

        /************************************************************
         Passed length of array is equal to zero
        ************************************************************/
        if ( sensor.write_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, buffer, 0 ) )
            { 
            std::cerr << "Passing length of zero should fail, and it didn't. Something is wrong.\n";
            return( 1 );
            }

        /************************************************************
         Read what is in the original register; fail the test if 
         unable to read. We don't want to restore garbage at the end
        ************************************************************/
        const bool read_ok = sensor.read_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, orig_buffer, REGISTER_SIZE );

        if ( !read_ok )
            {
            std::cerr << "Reading the register failed\n";
            return( 1 );
            }

        /************************************************************
         Write something to the register, then read and test what
         was written
        ************************************************************/
        buffer[ 0 ] = 0x01;
        buffer[ 1 ] = 0x02;
        buffer[ 2 ] = 0x03;
        buffer[ 3 ] = 0x04;
        
        const bool pattern_ok = sensor.write_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, buffer,           REGISTER_SIZE )
                             && sensor.read_register(  VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, pattern_readback, REGISTER_SIZE )
                             && std::equal( buffer, buffer + REGISTER_SIZE, pattern_readback );

        /************************************************************
         Restore the original register value
        ************************************************************/
        const bool restore_ok = sensor.write_register( VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, orig_buffer,      REGISTER_SIZE )
                             && sensor.read_register(  VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD, restore_readback, REGISTER_SIZE )
                             && std::equal( restore_readback, restore_readback + REGISTER_SIZE, orig_buffer );

        /************************************************************
         Fail if something went wrong
        ************************************************************/
        if ( !pattern_ok
          || !restore_ok )
            {
            std::cerr << "TEST FAILED: pattern_ok= " << pattern_ok << ", restore_ok= " << restore_ok << "\n";
            print_bytes( "Pattern written", buffer           );
            print_bytes( "Original       ", orig_buffer      );
            print_bytes( "Pattern read   ", pattern_readback );
            print_bytes( "Restore read   ", restore_readback );
            return( 1 );
            }

        /************************************************************
         Otherwise pass
        ************************************************************/
        std::cout << "TEST PASSED\n";

        return( 0 );

        }
    catch(const std::exception& e)
        {
        std::cerr << "I2C device test failed: " << e.what() << '\n';
        return ( 1 );
        }

    } /* main() */
/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <thread>
#include <stdexcept>
#include <chrono>
#include <cstdint>

#include "i2c_device.hpp"
#include "vl53l1_platform_attachment.hpp"
#include "vl53l1_platform.h"
#include "vl53l1_types.h"


/*************************************************************************************
                                     GLOBALS
*************************************************************************************/

namespace
    {
    I2cDevice* g_device = nullptr;
    }


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       Vl53l1PlatformAttachment::Vl53l1PlatformAttachment
*
*   DESCRIPTION:
*       Vl53l1PlatformAttachment constructor. Allows global g_device to point
*       to only one I2cDevice object
*
*****************************************************************************/

Vl53l1PlatformAttachment::Vl53l1PlatformAttachment( I2cDevice& device )
    {
    /************************************************************
     Throw an error if already pointing to I2cDevice object
    ************************************************************/
    if ( g_device != nullptr )
        {
        throw std::runtime_error( "VL53L1X platform already has a device attached" );
        }

    /************************************************************
     Point global g_device to the only I2cDevice object; Make sure
     to check that g_device is not null (destroyed) in every 
     function that needs to handle this object
    ************************************************************/
    g_device = &device;

    } /* Vl53l1PlatformAttachment() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       Vl53l1PlatformAttachment::~Vl53l1PlatformAttachment
*
*   DESCRIPTION:
*       Vl53l1PlatformAttachment destructor
*
*****************************************************************************/
Vl53l1PlatformAttachment::~Vl53l1PlatformAttachment()
    {
    /************************************************************
     Detach the device so adapter calls fail instead of using it. 
     Declare the attachment after the I2cDevice, so this runs 
     before the device is destroyed.
    ************************************************************/
    g_device = nullptr;

    } /* ~Vl53l1PlatformAttachment() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_WriteMulti()
*
*   DESCRIPTION:
*       Write multiple bytes to the device
*
*****************************************************************************/

int8_t VL53L1_WriteMulti( uint16_t 	dev,
                          uint16_t  index,
                          uint8_t  *pdata,
                          uint32_t  count )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr
      || pdata == nullptr
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Write multiple bytes
    ************************************************************/
    if ( false == g_device->write_register( index, pdata, count ) )
        {
        return( -1 );
        }
    
    return( 0 );

    } /* VL53L1_WriteMulti() */

   
/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_ReadMulti()
*
*   DESCRIPTION:
*       Read multiple bytes from the device
*
*****************************************************************************/

int8_t VL53L1_ReadMulti( uint16_t  dev,
                         uint16_t  index,
                         uint8_t  *pdata,
                         uint32_t  count )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr
      || pdata == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Read multiple bytes
    ************************************************************/
    if ( false == g_device->read_register( index, pdata, count ) )
        {
        return( -1 );
        }
    
    return( 0 );

    } /* VL53L1_ReadMulti() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_WrByte()
*
*   DESCRIPTION:
*       Write single byte to the device
*
*****************************************************************************/

int8_t VL53L1_WrByte( uint16_t  dev,
                      uint16_t  index,
                      uint8_t   data )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Write a byte
    ************************************************************/
    if ( false == g_device->write_register( index, &data, sizeof( data ) ) )
        {
        return( -1 );
        }
    
    return( 0 );

    } /* VL53L1_WrByte() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_WrWord()
*
*   DESCRIPTION:
*       Write a word to the device
*
*****************************************************************************/

int8_t VL53L1_WrWord( uint16_t  dev,
                      uint16_t  index,
                      uint16_t  data )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Pack 16 bit word into 8 bit tx array with high and low byte
    ************************************************************/
    const std::uint8_t bytes [ 2 ] = { static_cast<std::uint8_t>( data >> 8 ),
                                       static_cast<std::uint8_t>( data & 0xFF ) };
    
    if ( false == g_device->write_register( index, bytes, sizeof( bytes ) ) )
        {
        return( -1 );
        }    

    return( 0 );

    } /* VL53L1_WrWord() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_WrDWord()
*
*   DESCRIPTION:
*       Write a double word to the device
*
*****************************************************************************/

int8_t VL53L1_WrDWord( uint16_t  dev,
                       uint16_t  index,
                       uint32_t  data )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Pack double word into 8 bit tx array with high to low byte
    ************************************************************/
    const std::uint8_t bytes[ 4 ] = { static_cast<std::uint8_t>( data >> 24 ),
                                      static_cast<std::uint8_t>( data >> 16 ),
                                      static_cast<std::uint8_t>( data >>  8 ),
                                      static_cast<std::uint8_t>( data       ) };
    
    if ( false == g_device->write_register( index, bytes, sizeof( bytes ) ) )
        {
        return( -1 );
        }    

    return( 0 );

    } /* VL53L1_WrDWord() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_RdByte()
*
*   DESCRIPTION:
*       Read a single byte from the device
*
*****************************************************************************/

int8_t VL53L1_RdByte( uint16_t  dev,
                      uint16_t  index,
                      uint8_t  *pdata )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr
      || pdata == nullptr
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Read single byte
    ************************************************************/
    if ( false == g_device->read_register( index, pdata, sizeof( uint8_t ) ) )
        {
        return( -1 );
        }
    
    return( 0 );

    } /* VL53L1_RdByte() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_RdWord()
*
*   DESCRIPTION:
*       Read a word from the device
*
*****************************************************************************/

int8_t VL53L1_RdWord( uint16_t  dev,
                      uint16_t  index,
                      uint16_t *pdata )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr 
      || pdata == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Read two bytes; the sensor sends the high byte first
    ************************************************************/
    std::uint8_t bytes[ sizeof( uint16_t ) ];

    if ( false == g_device->read_register( index, bytes, sizeof( bytes ) ) )
        {
        return( -1 );
        }
    
    /************************************************************
     Combine the bytes into one word
    ************************************************************/
    *pdata = static_cast<std::uint16_t>( ( bytes[ 0 ] << 8 ) | bytes[ 1 ] );

    return( 0 );

    } /* VL53L1_RdWord() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_RdDWord()
*
*   DESCRIPTION:
*       Read a double word from the device
*
*****************************************************************************/

int8_t VL53L1_RdDWord( uint16_t  dev,
                       uint16_t  index,
                       uint32_t *pdata )
    {
    /************************************************************
     Input check
    ************************************************************/
    if ( g_device == nullptr 
      || pdata == nullptr 
      || dev != g_device->address() )
        {
        return( -1 );
        }

    /************************************************************
     Read four bytes; the sensor sends the high byte first
    ************************************************************/
    std::uint8_t bytes[ sizeof( uint32_t ) ];

    if ( false == g_device->read_register( index, bytes, sizeof( bytes ) ) )
        {
        return( -1 );
        }
    
    /************************************************************
     Combine the bytes into one double word
    ************************************************************/
    *pdata = ( static_cast<std::uint32_t>( bytes[ 0 ] ) << 24 )
           | ( static_cast<std::uint32_t>( bytes[ 1 ] ) << 16 )
           | ( static_cast<std::uint32_t>( bytes[ 2 ] ) <<  8 )
           | ( static_cast<std::uint32_t>( bytes[ 3 ] )       );

    return( 0 );

    } /* VL53L1_RdDWord() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       VL53L1_WaitMs()
*
*   DESCRIPTION:
*       Wait number of milli seconds
*
*****************************************************************************/

int8_t VL53L1_WaitMs( uint16_t /* dev */,
                      int32_t  wait_ms )
    {
    /************************************************************
     Wait
    ************************************************************/
    std::this_thread::sleep_for( std::chrono::milliseconds( wait_ms ) );

    return( 0 );

    } /* VL53L1_WaitMs() */
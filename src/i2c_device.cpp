/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <format>
#include "i2c_device.hpp"


/*************************************************************************************
                                     FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       I2cDevice::I2cDevice
*
*   DESCRIPTION:
*       I2cDevice constructor. Opens a bus and selects the device at specified 
*       address
*
*****************************************************************************/

I2cDevice::I2cDevice( const std::string& bus, 
                      std::uint8_t       address )
    {
    /************************************************************
     Open bus for read/write. Throw an error if not successful
    ************************************************************/
    fd_ = open( bus.c_str(), O_RDWR );

    if( fd_ == -1 )
        {
        throw std::runtime_error( "Could not open " + bus );
        }
    
    /************************************************************
     Select the device at ‘address’ for all following reads/writes
    ************************************************************/
    if( ioctl( fd_, I2C_SLAVE, address ) == -1 )
        {
        close( fd_ );
        throw std::runtime_error( std::format( "Could not select I2C device at address 0x{:02X} on bus {}", address, bus ) );
        }

    } /* I2cDevice::I2cDevice */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       I2cDevice::~I2cDevice
*
*   DESCRIPTION:
*       I2cDevice destructor. Closes the bus.
*
*****************************************************************************/

I2cDevice::~I2cDevice()
    {
    /************************************************************
     Close the bus if it was opened
    ************************************************************/
    if( fd_ >= 0 )
        {
        close( fd_ );
        }

    } /* I2cDevice::~I2cDevice */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       read_register
*
*   DESCRIPTION:
*       
*
*****************************************************************************/

bool I2cDevice::read_register( std::uint16_t  reg, 
                               std::uint8_t*  buffer, 
                               std::size_t    length )
    {
    /************************************************************
     The time of flight sensor (VL53L1X) uses 16-bit register 
     addresses, send high byte first
    ************************************************************/
    const std::uint8_t reg_bytes[2] = { static_cast<std::uint8_t>( reg >> 8 ),
                                        static_cast<std::uint8_t>( reg & 0xFF ) };

    /************************************************************
     Tell the device which register to read from
    ************************************************************/
    const ssize_t written = write( fd_, reg_bytes, sizeof( reg_bytes ) );

    if ( written != static_cast<ssize_t>( sizeof( reg_bytes ) ) )
        {
        return( false );
        }

    /************************************************************
     Read 'length' bytes starting at that register
    ************************************************************/
    const ssize_t received = read( fd_, buffer, length );

    if ( received < 0 
      || static_cast<std::size_t>( received ) != length )
        {
        return( false );
        }

    return( true );

    } /* read_register() */
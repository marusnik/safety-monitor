/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <format>
#include <linux/i2c.h>
#include <limits>

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
                      std::uint8_t       address ) : address_( address )
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
     Check for validity of inputs
     len member of struct i2c_msg is uint16_t
    ************************************************************/
    if ( buffer == nullptr
      || length == 0
      || length > std::numeric_limits<std::uint16_t>::max() )
        {
        return( false );
        }

    /************************************************************
     The time of flight sensor (VL53L1X) uses 16-bit register 
     addresses, send high byte first
    ************************************************************/
    std::uint8_t reg_bytes[ 2 ] = { static_cast<std::uint8_t>( reg >> 8 ),
                                    static_cast<std::uint8_t>( reg & 0xFF ) };

    /************************************************************
     Message 0 writes the register address, message 1 reads the
     data back; the kernel sends both as one transaction
    ************************************************************/
    i2c_msg messages[ 2 ] = {};

    messages[ 0 ].addr  = address_;
    messages[ 0 ].flags = 0;                          // write
    messages[ 0 ].len   = sizeof( reg_bytes );
    messages[ 0 ].buf   = reg_bytes;

    messages[ 1 ].addr  = address_;
    messages[ 1 ].flags = I2C_M_RD;                   // read
    messages[ 1 ].len   = static_cast<std::uint16_t>( length );
    messages[ 1 ].buf   = buffer;

    /************************************************************
     Hand both messages to the kernel in a single ioctl
    ************************************************************/
    i2c_rdwr_ioctl_data transfer = {};

    transfer.msgs  = messages;
    transfer.nmsgs = 2;

    if ( ioctl( fd_, I2C_RDWR, &transfer ) != 2 )
        {
        return( false );
        }

    return( true );

    } /* read_register() */
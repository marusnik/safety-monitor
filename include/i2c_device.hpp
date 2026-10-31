#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstddef>
#include <cstdint>
#include <string>

/*************************************************************************************
                                      CLASSES
*************************************************************************************/

class I2cDevice 
    {
    public:
        I2cDevice( const std::string& bus, std::uint8_t address );    // opens the bus, selects the device
        ~I2cDevice();                                                 // closes it automatically

        I2cDevice( const I2cDevice& )            = delete;            // no copies
        I2cDevice& operator=( const I2cDevice& ) = delete;

        bool read_register( std::uint16_t  reg, 
                            std::uint8_t*  buffer, 
                            std::size_t    length );

    private:
        int          fd_     = -1;                                    // the file descriptor
        std::uint8_t address_ = 0;                                    // sensor address on i2c bus (0X29)
    };
#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstddef>
#include <cstdint>
#include <string>

/*************************************************************************************
                                      CONSTANTS
*************************************************************************************/
static constexpr std::uint8_t  ToF_SENSOR_ADDR      = 0x29;
static constexpr std::uint8_t  EXPECTED_MODEL_ID    = 0xEA;
static constexpr std::uint8_t  EXPECTED_MODULE_TYPE = 0xCC;
static constexpr std::uint16_t EXPECTED_SENSOR_ID = static_cast<std::uint16_t>( ( EXPECTED_MODEL_ID    << 8 ) 
                                                                              | ( EXPECTED_MODULE_TYPE      ) );
static constexpr std::size_t  REGISTER_SIZE         =    4; /* biggest single write in VL53L1 api (WrDWord) */
static constexpr std::size_t  REG_HIGH_LOW_BYTES    =    2;


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

        bool write_register( std::uint16_t       reg, 
                             const std::uint8_t* data, 
                             std::size_t         length );
        
        std::uint8_t address();

    private:
        int          fd_     = -1;                                    // the file descriptor
        std::uint8_t address_ = 0;                                    // sensor address on i2c bus (0X29)
    };
#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <optional>
#include <string>

#include "i2c_device.hpp"
#include "vl53l1_platform_attachment.hpp"
#include "vl53l1x_api.hpp"

/*************************************************************************************
                                     CLASSES
*************************************************************************************/

class RangingSession
    {
    public:
        explicit RangingSession( std::uint16_t address );
        ~RangingSession();

        RangingSession( const RangingSession& )            = delete;
        RangingSession& operator=( const RangingSession& ) = delete;

    private:
        std::uint16_t address_;
    };


struct RangeReading 
    {
    std::uint8_t    status;
    std::uint16_t   distance_mm;

    // Only status 0 means distance_mm can be trusted
    bool valid() const { return status == 0; }
    };


class Vl53l1xSensor 
    {
    public:
        /************************************************************
         Opens the I2C bus, attaches the ST driver, initializes the
         sensor and starts ranging.

         Throws std::runtime_error if any step fails, so that an
         existing Vl53l1xSensor object always means a working sensor.

         Only one instance can exist at a time, because the ST
         driver reaches the bus through the single global g_device.
         A second instance throws.
        ************************************************************/
        Vl53l1xSensor( const std::string&    bus_path,
                       std::uint8_t          address ); 

        /************************************************************
         Waits for the next measurement, then returns its status and
         distance and clears the interrupt.

         Blocks for at least NUM_OF_POLL_ATTEMPTS ms if no data 
         arrives: NUM_OF_POLL_ATTEMPTS polls with a 1 ms wait each, 
         plus the I2C time per poll. sensor_loop and the staleness 
         limit both depend on this worst case.

         Returns nullopt on a timeout or a driver error. A reading
         can come back with status != 0, so check valid() before
         trusting distance_mm.
        ************************************************************/
        std::optional<RangeReading> read(); // nullopt on timeout or driver error

        const std::string& last_error() const { return last_error_; } // Why the last read() returned nullopt

        Vl53l1xSensor( const Vl53l1xSensor& )            = delete;
        Vl53l1xSensor& operator=( const Vl53l1xSensor& ) = delete;

    private:
        I2cDevice                   device_;
        Vl53l1PlatformAttachment    attachment_;
        RangingSession              ranging_;
        std::string                 last_error_;
    };
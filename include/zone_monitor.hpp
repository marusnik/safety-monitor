#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <optional>


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
enum class Zone : std::uint8_t { CLEAR, 
                                 SLOW, 
                                 STOP };

class ZoneMonitor 
    {
    public:
        Zone update( std::optional<std::uint16_t> distance_mm );   // process one reading, return the zone
        Zone zone() const;                                         // current zone
    
        static constexpr std::uint16_t CLEAR_DIST = 1500;
        static constexpr std::uint16_t STOP_DIST  =  500;
        static constexpr std::uint16_t HYSTERESIS =  100;
    
    private:
        Zone curr_zone_ = Zone::STOP;
    };
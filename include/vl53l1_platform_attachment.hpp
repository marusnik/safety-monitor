#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include"i2c_device.hpp"


/*************************************************************************************
                                      CLASSES
*************************************************************************************/

class Vl53l1PlatformAttachment 
    {
    public:
        explicit Vl53l1PlatformAttachment( I2cDevice& device );
                ~Vl53l1PlatformAttachment();

        Vl53l1PlatformAttachment( const Vl53l1PlatformAttachment& )            = delete;
        Vl53l1PlatformAttachment& operator=( const Vl53l1PlatformAttachment& ) = delete;
    };
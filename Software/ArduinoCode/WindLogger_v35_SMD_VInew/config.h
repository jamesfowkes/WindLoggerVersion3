#ifndef _CFG_READER_H_
#define _CFG_READER_H_

/*
 * Defines and Typedefs
 */

enum field_type
{
    // Electrical values
    VOLTAGE,
    CURRENT,

    // Temperature values
    TEMPERATURE_C,
    TEMPERATURE_F,
    TEMPERATURE_K,
    
    // Solar values
    IRRADIANCE_WpM2,

    // Wind values
    CARDINAL_DIRECTION, //N, NE, E, etc.
    DEGREES_DIRECTION,

    INVALID_TYPE
};
typedef enum field_type FIELD_TYPE;

/* Each FIELD_TYPE has a data structure associated with it 
 * in order to perform conversions from raw values to units.
  In addition, the number of settings is #defined so that the
  settings module can figure out if everything is set */
struct voltagechannel
{
    float mvPerBit;
    float R1;
    float R2;
};
typedef struct voltagechannel VOLTAGECHANNEL;

struct currentchannel
{
    float mvPerBit;
    float offset;
    float mvPerAmp;
};
typedef struct currentchannel CURRENTCHANNEL;

struct thermistorchannel
{
    float R25;
    float B;
    float otherR;
    float maxADC;
    bool highside;
}; 
typedef struct thermistorchannel THERMISTORCHANNEL;

#define MAX_CHANNELS (32)

/*
 * Public Functions
 */

bool CFG_read_channels_from_sd(SdFat * sd);

#endif

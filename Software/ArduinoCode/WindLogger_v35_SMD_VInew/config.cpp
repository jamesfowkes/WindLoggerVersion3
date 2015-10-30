/*
 * config.cpp
 *
 * Sets per-channel settings based on lines read from config file or similar
 *
 * Author: James Fowkes
 *
 * www.re-innovation.co.uk
 */

/************ External Libraries*****************************/
#include <Arduino.h>
#include <SdFat.h>

/************ Application Libraries*****************************/
#include "utility.h"
#include "sd.h"
#include "config.h"

#define MAX_LINE_LENGTH (32)

/*
 * Private Variables
 */

/*
 * For each of the channels that can be stored,
 * the bitfield stores a 1 if a setting has been set.
 * For example, the VOLTAGECHANNEL needs mvPerBit (0x01),
 * R1 (0x02) and R2 (0x04) settings.
 * 
 * If only R1 and mvPerBit are set, the value in the bitfield will be
 * 0x03, not 0x07 and the field will not be considered complete.
 */

static void * s_channels[MAX_CHANNELS];
static uint8_t s_valuesSetBitFields[MAX_CHANNELS];

static FIELD_TYPE s_fieldTypes[MAX_CHANNELS];

static const char s_voltage_channel_name[] PROGMEM = "voltage";
static const char s_current_channel_name[] PROGMEM = "current";
static const char s_temperature_c_channel_name[] PROGMEM = "temperature_c";

static const char * const s_channelTypes[] PROGMEM = {
    s_voltage_channel_name,
    s_current_channel_name,
    s_temperature_c_channel_name    
};

static const char s_config_filename[] = "channels.conf";

/*
 * Private Functions
 */

static bool parse_setting_as_float(float * pResult, char const * const setting)
{
    char * pStartOfConv = (char *)setting;
    char * pEndOfConv = pStartOfConv;
    *pResult = strtod(pStartOfConv, &pEndOfConv);
    return (pStartOfConv != pEndOfConv);
}

static bool try_parse_as_voltage_setting(uint8_t ch, char * pSettingName, char * pValueString)
{
    float setting;

    bool settingParsedAsFloat = parse_setting_as_float(&setting, pValueString);

    static const char s_voltage_setting_mvperbit[] PROGMEM = "mvperbit";
    static const char s_voltage_setting_r1[] PROGMEM = "r1";
    static const char s_voltage_setting_r2[] PROGMEM = "r2";

    char * voltage_setting;
    
    voltage_setting = PStringToRAM(s_voltage_setting_mvperbit);
    if (0 == strncmp(pSettingName, voltage_setting, 8))
    {
        if (!settingParsedAsFloat) { return false; }
        ((VOLTAGECHANNEL*)s_channels[ch])->mvPerBit = setting;
        s_valuesSetBitFields[ch] |= 0x01;
        return true;
    }

    voltage_setting = PStringToRAM(s_voltage_setting_mvperbit);
    if (0 == strncmp(pSettingName, s_voltage_setting_r1, 2))
    {
        if (!settingParsedAsFloat) { return false; }
        ((VOLTAGECHANNEL*)s_channels[ch])->R1 = setting;
        s_valuesSetBitFields[ch] |= 0x02;
        return true;
    }

    voltage_setting = PStringToRAM(s_voltage_setting_mvperbit);
    if (0 == strncmp(pSettingName, s_voltage_setting_r2, 2))
    {
        if (!settingParsedAsFloat) { return false; }
        ((VOLTAGECHANNEL*)s_channels[ch])->R2 = setting;
        s_valuesSetBitFields[ch] |= 0x04;
        return true;
    }

    return false;
}

static bool try_parse_as_current_setting(uint8_t ch, char * pSettingName, char * pValueString)
{
    float setting;
    
    bool settingParsedAsFloat = parse_setting_as_float(&setting, pValueString);
    
    static const char s_current_setting_mvperbit[] PROGMEM = "mvperbit";
    static const char s_current_setting_offset[] PROGMEM = "offset";
    static const char s_current_setting_mvperamp[] PROGMEM = "mvperamp";

    char * current_setting;

    current_setting = PStringToRAM(s_current_setting_mvperbit);
    if (0 == strncmp(pSettingName, current_setting, 8))
    {
        if (!settingParsedAsFloat) { return false; }
        ((CURRENTCHANNEL*)s_channels[ch])->mvPerBit = setting;
        s_valuesSetBitFields[ch] |= 0x01;
        return true;
    }
    
    current_setting = PStringToRAM(s_current_setting_offset);
    if (0 == strncmp(pSettingName, current_setting, 6))
    {
        if (!settingParsedAsFloat) { return false; }
        ((CURRENTCHANNEL*)s_channels[ch])->offset = setting;
        s_valuesSetBitFields[ch] |= 0x02;
        return true;
    }

    current_setting = PStringToRAM(s_current_setting_mvperamp);
    if (0 == strncmp(pSettingName, current_setting, 8))
    {
        if (!settingParsedAsFloat) { return false; }
        ((CURRENTCHANNEL*)s_channels[ch])->mvPerAmp = setting;
        s_valuesSetBitFields[ch] |= 0x04;
        return true;
    }

    return false;
}

static bool try_parse_as_thermsitor_setting(uint8_t ch, char * pSettingName, char * pValueString)
{
    float setting;
    
    bool settingParsedAsFloat = parse_setting_as_float(&setting, pValueString);
    
    static const char s_thermistor_setting_mvperbit[] PROGMEM = "maxadc";
    static const char s_thermistor_setting_b[] PROGMEM = "b";
    static const char s_thermistor_setting_r25[] PROGMEM = "r25";
    static const char s_thermistor_setting_otherr[] PROGMEM = "otherr";
    static const char s_thermistor_setting_highside[] PROGMEM = "highside";

    char * thermistor_setting;

    thermistor_setting = PStringToRAM(s_thermistor_setting_mvperbit);
    if (0 == strncmp(pSettingName, thermistor_setting, 6))
    {
        if (!settingParsedAsFloat) { return false; }
        ((THERMISTORCHANNEL*)s_channels[ch])->maxADC = setting;
        s_valuesSetBitFields[ch] |= 0x01;
        return true;
    }
    
    thermistor_setting = PStringToRAM(s_thermistor_setting_b);
    if (0 == strncmp(pSettingName, thermistor_setting, 1))
    {
        if (!settingParsedAsFloat) { return false; }
        ((THERMISTORCHANNEL*)s_channels[ch])->B = setting;
        s_valuesSetBitFields[ch] |= 0x02;
        return true;
    }

    thermistor_setting = PStringToRAM(s_thermistor_setting_r25);
    if (0 == strncmp(pSettingName, thermistor_setting, 3))
    {
        if (!settingParsedAsFloat) { return false; }
        ((THERMISTORCHANNEL*)s_channels[ch])->R25 = setting;
        s_valuesSetBitFields[ch] |= 0x04;
        return true;
    }
    
    thermistor_setting = PStringToRAM(s_thermistor_setting_otherr);
    if (0 == strncmp(pSettingName, thermistor_setting, 6))
    {
        if (!settingParsedAsFloat) { return false; }
        ((THERMISTORCHANNEL*)s_channels[ch])->otherR = setting;
        s_valuesSetBitFields[ch] |= 0x08;
        return true;
    }

    thermistor_setting = PStringToRAM(s_thermistor_setting_highside);
    if (0 == strncmp(pSettingName, thermistor_setting, 8))
    {
        ((THERMISTORCHANNEL*)s_channels[ch])->highside = (*pValueString != '0');
        s_valuesSetBitFields[ch] |= 0x10;
        return true;
    }

    return false;
}

static void setup_channel(uint8_t ch, FIELD_TYPE type)
{
    s_valuesSetBitFields[ch] = 0;
    switch(type)    
    {
    case VOLTAGE:
        s_channels[ch] = new VOLTAGECHANNEL();
        break;
    case CURRENT:
        s_channels[ch] = new CURRENTCHANNEL();
    case TEMPERATURE_C:
    case TEMPERATURE_F:
    case TEMPERATURE_K:
        s_channels[ch] = new THERMISTORCHANNEL();
        break;
    default:
    case INVALID_TYPE:
        break;
    }
}

static FIELD_TYPE parse_setting_as_type(char const * const setting)
{
    uint8_t i;
    char lcaseSetting[31];

    strncpy_safe(lcaseSetting, setting, 31);
    to_lower_str(lcaseSetting);

    for (i = 0; i < N_ELE(s_channelTypes); ++i)
    {
        char * channel_type = PStringToRAM(s_channelTypes[i]);
        if (0 == strncmp(lcaseSetting, channel_type, strlen(channel_type)))
        {
            return (FIELD_TYPE)i;
        }
    }

    return INVALID_TYPE;
}

static int8_t get_channel(char const * const pSetting)
{
    long channel = -1;

    char * pStartOfConv = (char *)pSetting;
    char * pEndOfConv = pStartOfConv;
    channel = strtol(pStartOfConv, &pEndOfConv, 10);
    if (pStartOfConv == pEndOfConv) { return -1; }
    
    return (int8_t)channel;
}

static  int8_t get_channel_from_setting(char const * const setting)
{
    char lcaseSetting[31];
    int8_t channel = -1;

    if (!setting) { return -1; }

    uint32_t length = strlen(setting);
    if (length > 30) { return -1; }

    strncpy_safe(lcaseSetting, setting, 31);
    to_lower_str(lcaseSetting);

	if (0 == strncmp(lcaseSetting, "ch", 2))
    {
        channel = get_channel(lcaseSetting+2);      
    }

    if (channel < 1) { return -1; }

    // Channels in file are CH1, CH2, CH3..., which correspond to indexes 0, 1, 2..
    // i.e. file is one-indexed, code is zero-indexed.
    return channel - 1; 
}

static bool process_config_line(char * setting)
{
	char * pSettingString;
    char * pValueString;
    char * pChannelString;
    char * pChannelSettingString;

    uint8_t length = strlen(setting);

    char lowercaseCopy[MAX_LINE_LENGTH];

    strncpy_safe(lowercaseCopy, setting, length+1);
    to_lower_str(lowercaseCopy);

    bool success = true;

    if (*setting == '#') { return true; } // Line is a comment
    
    // If line is blank, fail early
    if (string_is_whitespace(setting)) { return true; }

    // Split the string by '=' to get setting and name
    success &= split_and_strip_whitespace(lowercaseCopy, '=', &pSettingString, NULL, &pValueString, NULL);
    if (!success) { return false; }

    // Split the setting to get channel and setting name

    success &= split_and_strip_whitespace(pSettingString, '.', &pChannelString, NULL, &pChannelSettingString, NULL);
    if (!success) { return false; }

    int8_t ch = get_channel_from_setting(pChannelString);
    if (ch == -1) { return false; }

    if (0 == strncmp(pChannelSettingString, "type", 4))
    {
        // Try to interpret setting as a channel type
        s_fieldTypes[ch] = parse_setting_as_type(pValueString);
        if (s_fieldTypes[ch] == INVALID_TYPE) { return false; }

        setup_channel(ch, s_fieldTypes[ch]);
        return true;
    }

    /* If processing got this far, the setting needs to be interpreted based on the channel datatype */
    switch (s_fieldTypes[ch])
    {
    case VOLTAGE:
        return try_parse_as_voltage_setting(ch, pChannelSettingString, pValueString);
    case CURRENT:
        return try_parse_as_current_setting(ch, pChannelSettingString, pValueString);
    case TEMPERATURE_C:
    case TEMPERATURE_F:
    case TEMPERATURE_K:
        return try_parse_as_thermsitor_setting(ch, pChannelSettingString, pValueString);

    case INVALID_TYPE:
    default:
        // If the channel type is not set prior to any other settings, this is an error.
        return false;
    }
}


/*
 * Public Functions
 */

bool CFG_read_channels_from_sd(SdFat * sd)
{
	char buffer[MAX_LINE_LENGTH];
	int count;
	bool success = true;

    char * filename = PStringToRAM(s_config_filename);

	if (!SD_CardIsPresent()) { return false;}

	if (!sd->exists(filename)) { return false; }

	ifstream sdin(filename);
	while (sdin.getline(buffer, MAX_LINE_LENGTH, '\n') || (count = sdin.gcount()))
	{
	    if (!sdin.fail())
	    {
	    	success &= process_config_line(buffer);
	    }
  	}

  	return success;
}
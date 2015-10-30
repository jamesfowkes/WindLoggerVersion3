#ifndef _UTILITY_H_
#define _UTILITY_H_

/*
 * Defines and Typedefs
 */

#define N_ELE(x) (sizeof(x)/sizeof(x[0]))

/*
 * Public Function Prototypes
 */

byte DecToBcd(byte value);
char* PStringToRAM(const char* str);

void to_lower_str(char * pStr);
char * skip_spaces_rev(const char * line);
char * skip_spaces(const char * line);
uint32_t strncpy_safe(char * dst, char const * src, uint32_t max);
bool split_and_strip_whitespace(char * toSplit, char splitChar, char ** pStartOnLeft, char ** pEndOnLeft, char ** pStartOnRight, char ** pEndOnRight);
bool string_is_whitespace(char const * str);

/*
 * FixedLengthAccumulator
 *
 * Copied from Datalogger project (https://github.com/re-innovation/DataLogger)
 * Wrapper for a char buffer to allow easy creation one char at a time.
 * More control than strncpy, less powerful than full-blown String class
 */

class FixedLengthAccumulator
{
    public:
        FixedLengthAccumulator(char * buffer, uint16_t length);
        ~FixedLengthAccumulator();
        bool writeChar(char c);
        bool writeString(const char * s);
        bool writeLine(const char * s);
    
        void remove(uint32_t chars);
                
        void reset(void);
        char * c_str(void);
        
        bool isFull(void);
        void attach(char * buffer, uint16_t length);
        void detach(void);
        uint16_t length(void);

    private:
        char * m_buffer;
        uint16_t m_maxLength;
        uint16_t m_writeIndex;
};

#endif
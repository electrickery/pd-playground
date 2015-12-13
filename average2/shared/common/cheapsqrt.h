#ifndef fastsqrt_h
#define fastsqrt_h


// Fast square root method as described by Chris Lomont and Charles McEniry
// Converted from inverse square root to square root calculation by
// Katja Vetter. Supports both single and double float types

#include "m_pd.h"


#if PD_FLOAT_PRECISION == 64

inline t_float cheapsqrt(t_float value)
{
    union
    {
        t_float f;
        long long i;
    }alias;
    
    const t_float half = 0.5;
    const t_float oneandhalf = 1.5;
    alias.f = value;
    alias.i = 0x5fe6eb50c7b537aa - (alias.i>>1);
    value *= alias.f * (oneandhalf - half * value * alias.f * alias.f);
    return value;
}


#else    // if not 64 bit precision, assume 32 bit (default)

inline t_float cheapsqrt(t_float value)
{
    union
    {
        t_float f;
        int i;
    }alias;
    
    const t_float half = 0.5;
    const t_float oneandhalf = 1.5;
    alias.f = value;
    alias.i = 0x5f3759df - (alias.i>>1);
    value *= alias.f * (oneandhalf - half * value * alias.f * alias.f);
    return value;
}


#endif  // endif PD_FLOAT_PRECISION

#endif  // end ifndef fastsqrt_h

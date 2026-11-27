/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File              : lib_random.h
**  Created By    : Alan.Niu
**  Created On    : 11/25/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice:  ÊôÓÚ¸½Êô¿âº¯Êý
****************************************************************************/
#ifndef __RANDOM_H
#define __RANDOM_H

#include <cstdlib>
#include <iostream>
#include <cmath>

#ifndef RAND_MAX
#define RAND_MAX 32768
#endif
 
#define RAND16_MIN (1.0/RAND_MAX)

#define RAND32_MAX 2147483647
#define RAND32_MIN (1.0/RAND32_MAX)
#define RAND32_ALPHA 20536//16807

#define RANDWH_X_MAX 30269
#define RANDWH_Y_MAX 30307
#define RANDWH_Z_MAX 30323
#define RANDWH_X_MIN (1.0/RANDWH_X_MAX)
#define RANDWH_Y_MIN (1.0/RANDWH_Y_MAX)
#define RANDWH_Z_MIN (1.0/RANDWH_Z_MAX)
#define RANDWH_X_ALPHA 171
#define RANDWH_Y_ALPHA 172
#define RANDWH_Z_ALPHA 170

#ifndef RANDX
#define RANDX 32
#endif

#if RANDX == 48
#ifndef _MSC_VER
#define randx() drand48()
#define srandx(seed) srand48(seed)
#else
#define randx() rand32()
#define srandx(seed) srand32(seed)
#endif
#elif RANDX == 32
#define randx() rand32()
#define srandx(seed) srand32(seed)
#elif RANDX == 16
#define randx() rand16()
#define srandx(seed) srand(seed)
#else
#define randx() randwh()
#define srandx(seed) srandwh(seed)
#endif


//#ifdef _MSC_VER
//    #define atoll(a) _atoi64(a)
//    typedef __int64 longlong;
//#else
//    typedef long long longlong; 
//#endif

static int initSeedTime = 0;//times of initializing seed
//static longlong rand32_seed = 10;
word64 rand32_seed = 10;

static long randwh_x_seed = 1;
static long randwh_y_seed = 1;
static long randwh_z_seed = 1;

inline double rand16 ()    {
    return((double)rand() * RAND16_MIN);
}

// 32-bit pseudo-random  numbers
inline double rand32 ()    {
    rand32_seed = (RAND32_ALPHA * rand32_seed) % RAND32_MAX;
    return((double)rand32_seed * RAND32_MIN);
}
inline void srand32 (long seed)    {
    if (seed)    
    {
        if(initSeedTime ==0)
        {
            rand32_seed = seed;
            initSeedTime++;
//            LOG2DEV << "rand32 seed = " << seed << "\n";
        }
        else
        {
//            LOG2DEV << "rand32 seed = " << (long)rand32_seed << std::endl;
        }
    }
}

// According to Wichmann-Hill algorithm, 48-bit PN  numbers
inline double randwh ()    {
    randwh_x_seed = (RANDWH_X_ALPHA * randwh_x_seed) % RANDWH_X_MAX;
    randwh_y_seed = (RANDWH_Y_ALPHA * randwh_y_seed) % RANDWH_Y_MAX;
    randwh_z_seed = (RANDWH_Z_ALPHA * randwh_z_seed) % RANDWH_Z_MAX;
    double rand_out = (double)randwh_x_seed * RANDWH_X_MIN 
        + (double)randwh_y_seed * RANDWH_Y_MIN
        + (double)randwh_z_seed * RANDWH_Z_MIN;
    rand_out -= floor(rand_out);
    return(rand_out);
}
inline void srandwh (long seed)    {
    if (seed)    {
        randwh_x_seed = seed;
        randwh_y_seed = seed;
        randwh_z_seed = seed;
    }
//    LOG2DEV << "randwh seed = " << seed << "\n";
}

#endif






#ifndef RAND_H
#define RAND_H

#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <stdbool.h>
#include <math.h>

#define USE_FAST_RNG

#ifdef USE_FAST_RNG

#define ROTL(d,lrot) ((d<<(lrot)) | (d>>(8*sizeof(d)-(lrot))))
#define TWO31 0x80000000u
#define TWO32f (TWO31*2.0)

#endif


#define sigma 0.03
#define mu 0.5
#define two_pi 2*M_PI

bool initialized = false;

uint64_t xState = 0x0DDB1A5E5BAD5EEDull,
     yState = 0x519fb20ce6a199bbull;  // set to nonzero seed

void rand_init() {
    #ifdef USE_FAST_RNG
	if (!initialized){
	    srand(time(NULL));
            xState = (((uint64_t)(unsigned int)rand() << 32) + (uint64_t)(unsigned int)rand());
            yState = (((uint64_t)(unsigned int)rand() << 32) + (uint64_t)(unsigned int)rand());
	}
    #else
	if (!initialized) srand(time(NULL)):
    #endif
}

uint64_t romuDuoJr_random () {
    uint64_t xp = xState;
    xState = 15241094284759029579u * yState;
    yState = yState - xp;  yState = ROTL(yState,27);

    return xp;
}

inline double rand_32_double() {
    #ifdef USE_FAST_RNG
        unsigned int in = (unsigned int) romuDuoJr_random();
        double y = (double) in;
        return y/TWO32f;
    #else
        return (double) rand() / RAND_MAX;
    #endif
}

unsigned int rand_32_int (unsigned int max) {
    #ifdef USE_FAST_RNG
        unsigned int in = (unsigned int) romuDuoJr_random();
    #else
        unsigned int in = rand()
    #endif

    return in % max;
}

inline double rand_32_double_gauss() {
	double epsilon = DBL_EPSILON;

	double u1, u2;
	do
	{
		u1 = rand_32_double();
		u2 = rand_32_double();
	}
	while (u1 <= epsilon);

	double z0 = sqrt(-2.0 * log(u1)) * cos(two_pi * u2);
	// auto z1 = sqrt(-2.0 * log(u1)) * sin(two_pi * u2); // not used here!
	//
	return fmin(fmax(z0 * sigma + mu, 0.0), 1.0);
}
#endif

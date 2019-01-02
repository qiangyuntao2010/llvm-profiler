#include <stdio.h>
#include <inttypes.h>
#include "x86timer.hpp"

x86timer plain_t;

extern "C"
void plainInitialize(void){
	plain_t.start();
}

extern "C"
void plainFinalize(void){
	printf ("\n%f\n", plain_t.stop() * (1.0e-9));
}

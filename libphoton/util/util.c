#include <stdlib.h>

void photon_gettime_(double *s) {

	struct timeval tp;
	
	gettimeofday(&tp, NULL);
	*s = ((double)tp.tv_sec) + ( ((double)tp.tv_usec) / 1000000.0);
	return;
}


#include "helper_functions.h"
#include "type_definitions/error_code_definitions.h"
#ifndef _WIN32
#include <unistd.h>  // usleep for POSIX
#endif


#pragma region Public Function Definitions

int is_power_of_two(unsigned int bucket_size) {
    return (bucket_size != 0) && ((bucket_size & (bucket_size - 1)) == 0);
}


void portable_sleep_ms(unsigned long ms) {
#ifdef _WIN32
    Sleep(ms);
#else  // Linux/POSIX
    usleep(ms * 1000UL);
#endif
}

#pragma endregion


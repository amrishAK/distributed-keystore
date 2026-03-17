#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

/**
 * @fn is_power_of_two
 * @brief Checks if a given unsigned integer is a power of two.
 * @param bucket_size The unsigned integer to check.
 * @return int Returns 1 (true) if the number is a power of two, otherwise returns 0 (false).
 */
int is_power_of_two(unsigned int bucket_size);

/**
 * @fn portable_sleep_ms
 * @brief Cross-platform function to sleep for a specified number of milliseconds.
 * @param ms The number of milliseconds to sleep.
 */
void portable_sleep_ms(unsigned long ms);

#endif // HELPER_FUNCTIONS_H
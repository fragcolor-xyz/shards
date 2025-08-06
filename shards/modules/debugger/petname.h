#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Generates a random pet name with the specified number of words and separator
/// 
/// @param words_count Number of words to generate
/// @param separator Separator between words
/// @return A C string containing the generated name (must be freed by caller)
char* petname_generate(unsigned char words_count, const char* separator);

/// Frees a string allocated by petname_generate
/// 
/// @param ptr Pointer to the string to free
void petname_free_string(char* ptr);

#ifdef __cplusplus
}
#endif
#ifndef STRING_MANIPULATION_H

static inline void concatenate(const char* input_a, const char* input_b, char* output)
{
	while((*output = *input_a++)) ++output;
	while((*output++ = *input_b++));
}

#define STRING_MANIPULATION_H
#endif

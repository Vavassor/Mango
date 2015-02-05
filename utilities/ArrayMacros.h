#ifndef ARRAY_MACROS_H

#include <cstring>

#define ARRAY_COUNT(x) \
	((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define FILL(pointer, count, value) \
	memset((pointer), (value), sizeof *(pointer) * ((size_t)(count)))

#define CLEAR(pointer, count) \
	memset((pointer), 0, sizeof *(pointer) * ((size_t)(count)))

#define COPY(from, to, count) \
	memcpy((to), (from), sizeof *(from) * ((size_t)(count)))


#define CLEAR_ARRAY(pointer) \
	CLEAR(pointer, ARRAY_COUNT(pointer))

#define ARRAY_MACROS_H
#endif

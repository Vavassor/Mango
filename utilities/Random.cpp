#include "Random.h"

namespace random {

namespace
{
	unsigned long buffer[5];
}

// MULTIPLY-WITH-CARRY GENERATOR
//-------------------------------------------------------------------------------------------------

static unsigned long mwc_random()
{
	unsigned long* x = buffer;

	unsigned long long sum;
	sum = 2111111111ull * (unsigned long long)(x[3]) +
		1492ull * (unsigned long long)(x[2]) +
		1776ull * (unsigned long long)(x[1]) +
		5115ull * (unsigned long long)(x[0]) +
		(unsigned long long)(x[4]);

	x[3] = x[2];
	x[2] = x[1];
	x[1] = x[0];
	x[4] = sum >> 32; // carry
	x[0] = sum;
	return x[0];
}

unsigned long sow(unsigned long seed)
{
	unsigned long s = seed;

	// initialize buffer with a few random numbers
	for(int i = 0; i < 5; ++i)
	{
		s = s * 29943829 - 1;
		buffer[i] = s;
	}

	// iterate a few times to increase randomness
	for(int i = 0; i < 19; ++i) mwc_random();

	return seed;
}

int reap_integer(int min, int max)
{
	return min + static_cast<int>(mwc_random() % static_cast<unsigned long>(max - min + 1));
}

} // namespace random
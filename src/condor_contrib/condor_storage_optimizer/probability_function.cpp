#include "probability_function.h"

ProbabilityFunction::ProbabilityFunction()
{
	m_type = GAUSSIAN;
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type)
{
	m_type = type;
}

double ProbabilityFunction::getProbability(double time_to_fail)
{
	// We will replace this with a function which calculate pdf
//	double failure_rate = (double)rand() / (double)RAND_MAX;
	// TODO: temporarily set failure_rate to 0.4 and make sure there will be always three redundanduncy
	// selected given max_failure_rate = 0.1. This is for testing purpose and should be switched back to
	// random failure rate or other distribution
	double failure_rate = 0.4;
	return failure_rate;
}

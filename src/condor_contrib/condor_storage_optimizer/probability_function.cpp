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
	double failure_rate = (double)rand() / (double)RAND_MAX;
	return failure_rate;
}

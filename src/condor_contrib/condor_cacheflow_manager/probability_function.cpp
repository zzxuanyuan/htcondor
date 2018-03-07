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
	return 0.2;
}

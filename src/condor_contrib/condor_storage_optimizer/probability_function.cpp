#include "probability_function.h"

ProbabilityFunction::ProbabilityFunction()
{
	m_type = UNKNOWN;
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type)
{
	m_type = type;
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type, int duration_minutes)
{
	m_type = type;
	if(type == UNIFORM) {
		m_duration_minutes = duration_minutes;
	}
}

double ProbabilityFunction::getProbability(double constant)
{
	return constant;
}

double ProbabilityFunction::getProbability()
{
	return (double)rand() / (double)RAND_MAX;
}

double ProbabilityFunction::getProbability(time_t start_time, time_t current_time, int time_to_failure_minutes)
{
	// We will replace this with a function which calculate pdf
	double failure_rate = 0.0;
	if(m_type == GAUSSIAN) {
		// TODO: calculate gaussian pdf
		failure_rate = 0.0;
	} else if(m_type == UNIFORM) {
		time_t end_time = start_time + m_duration_minutes * 60;
		double time_to_failure_seconds = time_to_failure_minutes * 60.0;
		double time_to_end_seconds = (end_time - current_time) * 1.0;
		failure_rate = time_to_failure_seconds / time_to_end_seconds;
	}
	return failure_rate;
}

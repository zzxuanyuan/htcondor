#ifndef __PROBABILITY_FUNCTION_H__
#define __PROBABILITY_FUNCTION_H__

#include <iostream>

typedef enum {
	GAUSSIAN,
	UNIFORM,
	RANDOM,
	CONSTANT,
	UNKNOWN
} DISTRIBUTION_TYPE;

class ProbabilityFunction {
public:
	ProbabilityFunction();
	ProbabilityFunction(DISTRIBUTION_TYPE type);
	ProbabilityFunction(DISTRIBUTION_TYPE type, int duration_minutes);
	virtual ~ProbabilityFunction(){}

	// constant distribution
	double getProbability(double constant);
	// random distribution
	double getProbability();
	// uniform, gaussian
	double getProbability(time_t start_time, time_t current_time, int time_to_failure_minutes);

private:
	int m_type;
	int m_duration_minutes;
};

#endif

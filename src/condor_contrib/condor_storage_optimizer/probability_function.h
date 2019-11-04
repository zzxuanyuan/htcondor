#ifndef __PROBABILITY_FUNCTION_H__
#define __PROBABILITY_FUNCTION_H__

#define NBINS 200
#define NROLLS 1000000
#define WEIBULL_A 2  // weibull shape parameter
#define WEIBULL_B 50 // weibull scale parameter
#include <iostream>
#include <random>

typedef enum {
	BIMODALJOHNSON,
	WEIBULL,
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
	ProbabilityFunction(DISTRIBUTION_TYPE type, double parameter1, double parameter2);
	virtual ~ProbabilityFunction();

	// constant distribution
	double getProbability(double constant);
	// random distribution
	double getProbability();
	// uniform, gaussian, weibull
	double getProbability(time_t start_time, time_t current_time, int time_to_failure_minutes);

private:
	int m_type;
	std::string m_distribution_name;
	int m_shape_parameter;
	int m_scale_parameter;
	int m_duration_minutes;
	long long int *m_histogram;
};

#endif

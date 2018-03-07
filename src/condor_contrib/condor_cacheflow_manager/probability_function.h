#ifndef __PROBABILITY_FUNCTION_H__
#define __PROBABILITY_FUNCTION_H__

#include <iostream>

typedef enum {
	GAUSSIAN,
	UNIFORM
} DISTRIBUTION_TYPE;

class ProbabilityFunction {
public:
	ProbabilityFunction();
	ProbabilityFunction(DISTRIBUTION_TYPE type);
	virtual ~ProbabilityFunction(){}

	double getProbability(double time_to_fail);

private:
	int m_type;
};

#endif

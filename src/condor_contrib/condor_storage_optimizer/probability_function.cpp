#include "probability_function.h"
#include <fstream>

ProbabilityFunction::ProbabilityFunction()
{
	m_type = UNKNOWN;
	m_histogram = nullptr;
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type)
{
	m_type = type;
	m_histogram = nullptr;
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type, int duration_minutes)
{
	m_type = type;
	m_histogram = nullptr;
	if(type == UNIFORM) {
		m_distribution_name = "uniform";
		m_duration_minutes = duration_minutes;
	}
}

ProbabilityFunction::ProbabilityFunction(DISTRIBUTION_TYPE type, double parameter1, double parameter2)
{
	m_type = type;
	m_histogram = nullptr;
	if(type == WEIBULL) {
		m_distribution_name = "weibull";
		m_duration_minutes = -1;
		m_shape_parameter = parameter1;
		m_scale_parameter = parameter2;
		// we need to create an extra bin which stores all out-of-range data
		m_histogram = new long long int[NBINS+1];
		for(int i = 0; i < NBINS+1; ++i) {
			m_histogram[i] = 0;
		}
		// generating histogram pdf
		std::default_random_engine generator;
		std::weibull_distribution<double> distribution(m_shape_parameter, m_scale_parameter);
		for(int i = 0; i < NROLLS; ++i) {
			double number = distribution(generator);
			if(number < NBINS) {
				++m_histogram[int(number)];
			} else {
				// all number that are larger than NBINS go into the very last bin.
				// remember there are NBINS+1 bins.
				++m_histogram[NBINS];
			}
		}
	}
	std::fstream histogram_fs;
	histogram_fs.open("/home/centos/histogram.txt", std::fstream::out | std::fstream::app);
	histogram_fs << m_distribution_name << " (" << parameter1 << "," << parameter2 << ");" << std::endl;
	for (int i = 0; i < NBINS; ++i) {
		histogram_fs << i << "-" << (i+1) << ": ";
		// scale histogram to 1/200
		histogram_fs << std::string(m_histogram[i]/200,'*') << ", " << m_histogram[i] << "/" << NROLLS << std::endl;
	}
	histogram_fs.close();
}

ProbabilityFunction::~ProbabilityFunction() {
	if(m_histogram) {
		delete [] m_histogram;
	}
	m_histogram = nullptr;
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
	// we need to think about different cases:
	// 1. time_to_failure_seconds > 0, time_to_end_seconds > 0, time_to_failure_seconds < time_to_end_seconds (valid location);
	// 2. time_to_failure_seconds > 0, time_to_end_seconds > 0, time_to_failure_seconds >= time_to_end_seconds (designated to fail);
	// 3. time_to_failure_seconds > 0, time_to_end_seconds <=0, (pass the pdf's expected deadline - should fail);
	// 4. time_to_failure_seconds <=0, time_to_end_seconds > 0, (cache's expiry has been passed, so cache is safe now to be deleted);
	// 5. time_to_failure_seconds <=0, time_to_end_seconds <=0, time_to_failure_seconds < time_to_end_seconds (failure_rate > 1.0 but cache is safe to be deleted now)
	// 6. time_to_failure_seconds <=0, time_to_end_seconds <=0, time_to_failure_seconds >=time_to_end_seconds (0.0 < failure_rate < 1.0 but cache is safe to be deleted now)
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
	} else if(m_type == WEIBULL) {
		time_t time_at_pdf = current_time - start_time;
		int time_at_pdf_minutes = int(time_at_pdf/60.0);
		if(time_at_pdf_minutes > NBINS) return 1.0;
		long long int time_to_failure_histogram = 0;
		long long int time_to_end_histogram = 0;
		for(int i = time_at_pdf_minutes; i <= NBINS; ++i) {
			if(i <= time_at_pdf_minutes+time_to_failure_minutes) {
				time_to_failure_histogram += m_histogram[i];
			}
			time_to_end_histogram += m_histogram[i];
		}
		failure_rate = time_to_failure_histogram * 1.0 / time_to_end_histogram;
	}
	return failure_rate;
}

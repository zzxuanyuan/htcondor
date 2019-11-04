#include "probability_function.h"
#include <fstream>

double bimodal_johnson_cdf[NBINS+1] = {0.0, 0.29298488396698014, 0.2752498185972666, 0.23009456430307562, 0.2019088714040791, 0.18146856942842648, 0.16552663822137306, 0.15256430423777437, 0.14172857731429533, 0.13248680279627656, 0.12448223283610468, 0.11746366533395704, 0.11124740659721626, 0.1056950869819134, 0.10069994977107127, 0.0961779791022396, 0.09206194110510744, 0.08829725598405823, 0.08483906283523947, 0.0816500853822549, 0.07869904971458277, 0.07595949117260653, 0.07340884106487303, 0.07102771817245415, 0.06879937248395793, 0.06670924369336698, 0.06474460731911141, 0.06289428849686376, 0.06114842859200455, 0.059498293437191, 0.05793611466514583, 0.05645495757108909, 0.05504861040373927, 0.05371149108715844, 0.052438568215082554, 0.051225293803700486, 0.05006754578763071, 0.048961578633011316, 0.047903980747505626, 0.04689163760912421, 0.045921699728628605, 0.044991554714871596, 0.04409880283706252, 0.04324123557897871, 0.04241681676247595, 0.0416236658850736, 0.040860043371863934, 0.04012433748784343, 0.039415052694816766, 0.03873079926873819, 0.03807028401988663, 0.03743230198055194, 0.03681572894368569, 0.03621951475184832, 0.03564267724925513, 0.03508429682118795, 0.03454351145482622, 0.03401951226392919, 0.03351153942699502, 0.033018878494713605, 0.03254085702787518, 0.03207684153152041, 0.031626234655128364, 0.031188472632125593, 0.030763022935037902, 0.030349382125258707, 0.029947073878730284, 0.029555647170869392, 0.02917467460585766, 0.02880375087699155, 0.028442491346175453, 0.028090530731868464, 0.027747521895881234, 0.027413134720382083, 0.027087055067326747, 0.02676898381328656, 0.02645863595332734, 0.026155739768195952, 0.025860036049611575, 0.025571277378941926, 0.02528922745497815, 0.025013660466910146, 0.02474436050895395, 0.02448112103339604, 0.024223744339103206, 0.02397204109280091, 0.023725829880654, 0.023484936787891507, 0.02324919500440545, 0.023018444454424646, 0.022792531448518765, 0.022571308356328194, 0.022354633298543237, 0.022142369856771458, 0.021934386800037915, 0.021730557826758704, 0.021530761321115935, 0.021334880122841744, 0.021142801309491966, 0.020954415990356152, 0.020769619111211328, 0.020588309269182165, 0.02041038853702053, 0.020235762296163657, 0.02006433907797185, 0.01989603041258532, 0.019730750684873913, 0.01956841699698608, 0.019408949037031192, 0.01925226895345603, 0.019098301234699307, 0.018946972593729235, 0.01879821185708796, 0.018651949858083335, 0.01850811933378315, 0.018366654825480024, 0.018227492582306267, 0.018090570467688857, 0.017955827868344956, 0.017823205605532088, 0.017692645848290188, 0.01756409202846331, 0.017437488757415315, 0.017312781744703555, 0.017189917720017694, 0.017068844363090776, 0.016949510258603657, 0.01683186494390116, 0.016715859364500534, 0.016601448546813218, 0.0164886105257068, 0.016377554287885197, 0.016273993673512722, 0.01698850366235896, 0.06091993119542141, 0.1736794130169068, 0.2917618268896647, 0.3944164744466578, 0.4795881126194941, 0.5495284225779667, 0.6070617728683924, 0.6546822882325464, 0.6944032231747772, 0.7275826139426751, 0.7444845759667864, 0.7384077827294743, 0.7237123191798537, 0.7062008585088382, 0.6878989836573373, 0.6696617075945861, 0.6519057434844946, 0.6348559097989576, 0.6186425432588886, 0.6033450094312126, 0.5890130715365698, 0.5756782393738751, 0.5633602324427088, 0.5520709175047511, 0.5418168785852673, 0.5326012178806375, 0.5244249101904626, 0.5172878911473191, 0.5111899834890609, 0.5061317240727856, 0.5021151314149507, 0.49914444102444405, 0.4972268292444679, 0.496373143285112, 0.4965986542310742, 0.49792385026150976, 0.5003752886678573, 0.5039865272263235, 0.5087991578754513, 0.5148639682526497, 0.5222422591251434, 0.5310073475052826, 0.5412462851262918, 0.5530618178231712, 0.566574599182722, 0.5819256440191999, 0.5992789494406945, 0.6188240958954639, 0.6407784134587986, 0.6653878504946866, 0.6929247846037698, 0.7236791718551743, 0.7579354919965088, 0.7959190691964146, 0.8376737590512001, 0.8827743808204652, 0.9295881310091605, 0.9729633362091804, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

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
	} else if(m_type == BIMODALJOHNSON) {
		time_t time_at_pdf = current_time - start_time;
		int time_at_pdf_minutes = int(time_at_pdf/60.0);
		if(time_at_pdf_minutes > NBINS) return 1.0;
		failure_rate = bimodal_johnson_cdf[time_at_pdf_minutes];
	}
	return failure_rate;
}

#import "gsoap_daemon_core_types.h"

//gsoap condor service name: condorCollector

int condor__queryStartdAds(char *constraint,
						   struct ClassAdStructArray & result);

int condor__queryScheddAds(char *constraint,
						   struct ClassAdStructArray & result);

int condor__queryMasterAds(char *constraint,
						   struct ClassAdStructArray & result);

int condor__querySubmittorAds(char *constraint,
							  struct ClassAdStructArray & result);

int condor__queryLicenseAds(char *constraint,
							struct ClassAdStructArray & result);

int condor__queryStorageAds(char *constraint,
							struct ClassAdStructArray & result);

int condor__queryAnyAds(char *constraint,
						struct ClassAdStructArray & result);



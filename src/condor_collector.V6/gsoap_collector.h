
#import "../condor_daemon_core.V6/gsoap_daemon_core_types.h"

//gsoap condorCollector service namespace: urn:condor-collector
//gsoap condorCollector service name: condorCollector
//gsoap condorCollector service encoding: encoding-style

int condorCollector__queryStartdAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__queryScheddAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__queryMasterAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__querySubmittorAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__queryLicenseAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__queryStorageAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);

int condorCollector__queryAnyAds(char *constraint,
	struct condorCore__ClassAdStructArray & result);



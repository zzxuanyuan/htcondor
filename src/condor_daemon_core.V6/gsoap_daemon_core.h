#import "gsoap_daemon_core_types.h"

//gsoap condor service name: condor
//gsoap condor service encoding: encoded

int condor__getInfoAd(void *_, struct condor__ClassAdStructAndStatusResponse & ad);
int condor__getVersionString(void *_, struct condor__StringAndStatusResponse & verstring);
int condor__getPlatformString(void *_, struct condor__StringAndStatusResponse & verstring);

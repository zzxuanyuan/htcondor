#import "gsoap_daemon_core_types.h"

//gsoap condor service name: condor
//gsoap condor service encoding: encoding-style

int condor__getInfoAd(void *_, struct condor__ClassAdStructAndStatus & ad);
int condor__getVersionString(void *_, struct condor__StringAndStatus & verstring);
int condor__getPlatformString(void *_, struct condor__StringAndStatus & verstring);

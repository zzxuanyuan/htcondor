#import "gsoap_daemon_core_types.h"

//gsoap condorCore service name: condorCore
//gsoap condorCore service encoding: encoding-style

int condorCore__getInfoAd(void *_, struct condorCore__ClassAdStructAndStatus & ad);
int condorCore__getVersionString(void *_, struct condorCore__StringAndStatus & verstring);
int condorCore__getPlatformString(void *_, struct condorCore__StringAndStatus & verstring);

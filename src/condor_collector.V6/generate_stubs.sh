#!/bin/sh

/bin/mkdir junk
soapcpp2 -d junk -p soap_collector gsoap_collector.h
cp junk/soap_collectorC.cpp .
cp junk/soap_collectorServer.cpp .
cp junk/condorCollector.nsmap .
cp junk/soap_collectorH.h .
cp junk/soap_collectorStub.h .
cp junk/condorCollector.wsdl .
touch soap_collectorStub.C
/bin/rm -rf junk

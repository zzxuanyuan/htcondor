#!/bin/sh

/bin/mkdir junk
soapcpp2 -d junk -p soap_schedd gsoap_schedd.h
cp junk/soap_scheddC.cpp .
cp junk/soap_scheddServer.cpp .
cp junk/condorSchedd.nsmap .
cp junk/soap_scheddH.h .
cp junk/soap_scheddStub.h .
cp junk/condorSchedd.wsdl .
touch soap_scheddStub.C
/bin/rm -rf junk

#!/bin/sh

#export VERSION=2.7.6c-p1
export VERSION=2.7.6c

rm -rf gsoap-${VERSION}
tar xfz ../../externals/bundles/gsoap/${VERSION}/gsoap-${VERSION}.tar.gz
rm -rf gsoap_install_extras
mkdir gsoap_install_extras
cp gsoap-${VERSION}/soapcpp2/stdsoap2.cpp gsoap_install_extras/stdsoap2.C
cp gsoap-${VERSION}/soapcpp2/dom.c gsoap_install_extras/dom.C
cp gsoap-${VERSION}/soapcpp2/stdsoap2.h gsoap_install_extras/
cp gsoap-${VERSION}/soapcpp2/typemap.dat gsoap_install_extras/
cp -r gsoap-${VERSION}/soapcpp2/import gsoap_install_extras/
cp -r gsoap-${VERSION}/soapcpp2/plugin gsoap_install_extras/
mv gsoap-${VERSION}/soapcpp2/plugin/wsseapi.c gsoap_install_extras/plugin/wsseapi.C
mv gsoap-${VERSION}/soapcpp2/plugin/smdevp.c gsoap_install_extras/plugin/smdevp.C
rm -rf gsoap-${VERSION}
rm -f soapStub.h
rm -f soapH.h
ln -s soap_submitStub.h soapStub.h
ln -s soap_submitH.h soapH.h

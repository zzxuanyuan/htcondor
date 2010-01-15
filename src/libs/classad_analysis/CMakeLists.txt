
file( GLOB AnalSrcs analysis.* boolExpr.* condition.* multiProfile.* profile.* resourceGroup.* boolValue.* explain.* interval.* result.* )

condor_static_lib( analysis "${AnalSrcs}" )
condor_static_lib( conversion "conversion.cpp;conversion.h" )
# add_library( gangmatch STATIC portGraph.cpp gmr.cpp gangster.cpp)
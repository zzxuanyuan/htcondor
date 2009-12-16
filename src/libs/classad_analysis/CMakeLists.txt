
file( GLOB AnalSrcs analysis.* boolExpr.* condition.* multiProfile.* profile.* resourceGroup.* boolValue.* explain.* interval.* result.* )

add_library( analysis STATIC ${AnalSrcs} )
add_library( conversion STATIC conversion.cpp conversion.h )
# add_library( gangmatch STATIC portGraph.cpp gmr.cpp gangster.cpp)
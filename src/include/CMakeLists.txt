file( GLOB Includes condor_*.h README)
message (STATUS "TODO: src/include narrow down list of include files")

install(FILES		${Includes}
	DESTINATION	"${C_INCLUDE}")


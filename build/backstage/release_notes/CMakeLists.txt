file( GLOB Docs README* INSTALL* DOC* ../../../NOTICE.txt ../../../LICENSE-2.0.txt)


install(FILES		${Docs}
        DESTINATION	"${C_DOC}")

install(DIRECTORY	examples
	DESTINATION	"${C_DOC}")


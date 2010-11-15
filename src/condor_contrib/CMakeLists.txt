##########################################################################
# add the contrib subtree for build & test only

if (WANT_CONTRIB)
  add_subdirectories("${CMAKE_CURRENT_SOURCE_DIR}" "" "")
else(WANT_CONTRIB)
  dprint("skipping contrib modules")
endif(WANT_CONTRIB)



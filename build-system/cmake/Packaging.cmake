include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS keystore
  EXPORT distributed_keystoreTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/keystore
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/keystore
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

set(DISTRIBUTED_KEYSTORE_CMAKE_INSTALL_DIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/distributed_keystore")

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/distributed_keystoreConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/distributed_keystoreConfig.cmake
  INSTALL_DESTINATION ${DISTRIBUTED_KEYSTORE_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/distributed_keystoreConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(EXPORT distributed_keystoreTargets
  FILE distributed_keystoreTargets.cmake
  NAMESPACE distributed_keystore::
  DESTINATION ${DISTRIBUTED_KEYSTORE_CMAKE_INSTALL_DIR}
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/distributed_keystoreConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/distributed_keystoreConfigVersion.cmake
  DESTINATION ${DISTRIBUTED_KEYSTORE_CMAKE_INSTALL_DIR}
)

include(CPack)

cmake_minimum_required(VERSION 3.20)
project(distributed_keystore_package_consumer LANGUAGES C)

if(DEFINED DISTRIBUTED_KEYSTORE_VERSION)
	find_package(distributed_keystore ${DISTRIBUTED_KEYSTORE_VERSION} EXACT CONFIG REQUIRED)
else()
	find_package(distributed_keystore CONFIG REQUIRED)
endif()

add_executable(package_consumer main.c)
target_link_libraries(package_consumer PRIVATE distributed_keystore::keystore)
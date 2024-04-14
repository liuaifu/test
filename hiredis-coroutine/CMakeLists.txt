cmake_minimum_required(VERSION 3.12)
PROJECT(hiredis-coroutine)

aux_source_directory(. DIRSRCS)

ADD_EXECUTABLE(${PROJECT_NAME} ${DIRSRCS})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
if(WIN32)
	set(BOOST_ROOT d:/library/boost_1_84_0)
	set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost
	1.84.0
	REQUIRED
	COMPONENTS thread
	)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
endif(Boost_FOUND)

include_directories(D:/library/hiredis/include)
if(UNIX)
	target_link_libraries(${PROJECT_NAME} PRIVATE hiredis pthread)
else()
	target_link_libraries(${PROJECT_NAME} PRIVATE D:/library/hiredis/lib/hiredis.lib)
endif()
INSTALL(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

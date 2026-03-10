project(tcs_sim)

find_package(ITC_Common REQUIRED QUIET COMPONENTS itc_logger)
find_package(NOSENGINE REQUIRED QUIET COMPONENTS common transport client uart)

include_directories(inc
                    ${sim_common_SOURCE_DIR}/inc
                    ${ITC_Common_INCLUDE_DIRS}
                    ${NOSENGINE_INCLUDE_DIRS})

set(tcs_sim_src
    src/tcs_hardware_model.cpp
    src/tcs_42_data_provider.cpp
    src/tcs_data_provider.cpp
    src/tcs_data_point.cpp
)

# For Code::Blocks and other IDEs
file(GLOB tcs_sim_inc inc/*.hpp)

set(tcs_sim_libs
    sim_common
    ${ITC_Common_LIBRARIES}
    ${NOSENGINE_LIBRARIES}
)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:$ORIGIN/../lib") # Pick up .so in install directory

add_library(tcs_sim SHARED ${tcs_sim_src} ${tcs_sim_inc})
target_link_libraries(tcs_sim ${tcs_sim_libs})
install(TARGETS tcs_sim LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

add_library(trace_analyser_lib TraceAnalyser.cpp)

get_property(SPARTA_INCLUDE_PROP TARGET SPARTA::sparta PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(trace_analyser_lib SYSTEM PRIVATE ${SPARTA_INCLUDE_PROP})

target_link_libraries(trace_analyser_lib decode instgen)


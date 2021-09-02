add_library(fm_si470x INTERFACE)

target_include_directories(fm_si470x
    INTERFACE
    ./include)

target_sources(fm_si470x
    INTERFACE
    fm_si470x.c
)

target_link_libraries(fm_si470x
    INTERFACE
    hardware_i2c
)

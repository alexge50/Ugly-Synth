cmake_minimum_required(VERSION 3.8)
project(Synthesizer)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH cmake)
find_package(Portaudio REQUIRED)
find_package(Portmidi REQUIRED)


include_directories(${PORTAUDIO_INCLUDE_DIRS} ${PORTMIDI_INCLUDE_DIR})

add_executable(Synthesizer main.cpp)
target_link_libraries(Synthesizer dl ${PORTAUDIO_LIBRARIES} ${PORTMIDI_LIBRARIES})

add_executable(midi_test midi_test.cpp)
target_link_libraries(midi_test ${PORTMIDI_LIBRARIES})

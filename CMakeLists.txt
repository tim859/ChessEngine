cmake_minimum_required(VERSION 4.1)
project(Chess)

set(CMAKE_CXX_STANDARD 20)

set(SFML_DIR "G:/Libraries/SFML-3.0.2/lib/cmake/SFML")

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

add_executable(Chess main.cpp game.cpp engine.cpp boardview.cpp audio.cpp)

target_link_libraries(Chess PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio)

add_executable(ChessUCI ucimain.cpp game.cpp engine.cpp ucisession.cpp)

add_custom_command(TARGET Chess POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Chess>/assets)
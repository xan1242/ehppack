# TODO: maybe add minimum C++ version due to the sorting algo, not sure how necessary that is...

cmake_minimum_required(VERSION 3.10)

# project name
project("ehppack")

# add the executable
add_executable("${PROJECT_NAME}" "ehppack/ehppack.cpp")

# install script
install(TARGETS "${PROJECT_NAME}" DESTINATION bin)
install(FILES "ehppack/ehppack.cpp" DESTINATION src)

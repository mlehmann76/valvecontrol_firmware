# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set (My_Project_Title "valvecontrol")
set (My_Project_Binary "valvecontrol.elf")
set (My_Project_Description "")
set (My_Project_Author "M. Lehmann")
set (My_Project_Revised_Date "2021-XX-XX")
set (My_Project_Version_Major 0)
set (My_Project_Version_Minor 4)
set (My_Project_Version_Patch 0)

set (My_Project_Version "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")

set (My_Project_Copyright_Date "2016-2021")
set (My_Project_Copyright "Copyright © ${My_Project_Copyright_Date} ${My_Project_Author}.")

string(TOUPPER ${My_Project_Title} My_Project_Title_Caps  )
string(REGEX REPLACE " " "_" My_Project_Title_Caps ${My_Project_Title_Caps} )

set(CMAKE_CXX_STANDARD 17)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${My_Project_Title})
set(COMPONENTS "main")
target_add_binary_data(${My_Project_Binary} "main/config.json" TEXT)
spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)

include(components/espfs/cmake/include.cmake)
target_add_espfs(${My_Project_Binary} espfs app)

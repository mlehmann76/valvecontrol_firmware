idf_component_register(SRCS "base85.c"
                            "FreeRTOSCPP.cpp"
                            "MainClass.cpp"
                            "MqttRepAdapter.cpp"
                            "RepositoryHandler.cpp"
                            "WifiManager.cpp"
                            "app_main.cpp"
                            "FreeRTOSCPP.cpp"
                            "channelAdapter.cpp"
                            "channelFactory.cpp"
                            "config.cpp"
                            "mqttWorker.cpp"
                            "otaHandler.cpp"
                            "otaWorker.cpp"
                            "property.cpp"
                            "repository.cpp"
                            "sht1x.cpp"
                            "sntp.cpp"
                            "utilities.cpp"
                            "tasks.cpp" 
                            "Cipher.cpp" 
                            "LedFlasher.cpp"
                            INCLUDE_DIRS
                            "." REQUIRES FreeRTOScpp mqtt http variant nvs_flash
                                json njson app_update pthread logger spiffs libespfs)

target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Wstack-usage=128)
#target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage )


find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    INCLUDE(GetGitRevisionDescription)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags HEAD OUTPUT_VARIABLE SHORT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE)

    SET(REVISION ${SHORT_SHA} CACHE STRING "git describe tags" FORCE)

    # only use the plugin to tie the configure state to the tag to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    #get_git_head_revision(REFSPEC COMMITHASH)
else()
    message(WARNING "Git not found, cannot set version info")

    SET(REVISION "unknown")
endif()

# generate version.h
include_directories(${CMAKE_BINARY_DIR})
configure_file("${CMAKE_SOURCE_DIR}/main/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)
                              

cmake_minimum_required(VERSION 3.6)
project(UDP_ClientServer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES1
        server.c)

#set(SOURCE_FILES2
#        client.c)

add_executable(UDP_Server ${SOURCE_FILES1})
#add_executable(UDP_Client ${SOURCE_FILES2})
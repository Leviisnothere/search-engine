cmake_minimum_required(VERSION 3.26)
project(Index C)

set(CMAKE_C_STANDARD 11)

add_executable(Index index.c
        merge.c
        merge.h
        varbyte.h
        varbyte.c
        index.h
        hashmap.h
        hashmap.c
        query.c
        query.h)
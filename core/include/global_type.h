#ifndef GLOBAL_TYPE_H
#define GLOBAL_TYPE_H 

// 定义在某个公共头文件中，例如 export.h
#pragma once

#ifdef _WIN32
    #ifdef YALN_CORE_EXPORTS
        #define YALN_CORE_EXPORT __declspec(dllexport)
    #else
        #define YALN_CORE_EXPORT __declspec(dllimport)
    #endif
#else
    #define MYLIB_API __attribute__((visibility("default")))
#endif

#endif  // GLOBAL_TYPE_H
add_executable(cache-sratch cache-scratch.cpp)

target_link_libraries(cache-sratch  rmalloc libm.a)
#/usr/lib/x86_64-linux-gnu/libmimalloc.so
#/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4.5.16
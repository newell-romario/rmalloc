add_executable(linux-scalability linux-scalability.c)
target_link_libraries(linux-scalability rmalloc libm.a)
#/usr/lib/x86_64-linux-gnu/libmimalloc.so
#/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4.5.16
#/usr/lib/x86_64-linux-gnu/libjemalloc.so
add_executable(mstress mstress.c)
target_link_libraries(mstress rmalloc libm.a)
#/usr/lib/x86_64-linux-gnu/libmimalloc.so
#/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4.5.16
#/usr/lib/x86_64-linux-gnu/libjemalloc.so
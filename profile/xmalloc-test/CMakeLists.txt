add_executable(xmalloc xmalloc-test.c)
target_link_libraries(xmalloc rmalloc libm.a)
#/usr/lib/x86_64-linux-gnu/libjemalloc.so
#/usr/lib/x86_64-linux-gnu/libmimalloc.so
#/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4.5.16
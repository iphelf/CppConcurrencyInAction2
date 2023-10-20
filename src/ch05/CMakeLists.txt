add_executable(ch05-spinlock_mutex spinlock_mutex.cpp)
set_property(TARGET ch05-spinlock_mutex PROPERTY CXX_STANDARD 20)

add_executable(ch05-atomics atomics.cpp)

add_executable(ch05-relaxed relaxed.cpp)
set_property(TARGET ch05-relaxed PROPERTY CXX_STANDARD 20)

add_executable(ch05-acq_rel acq_rel.cpp)
set_property(TARGET ch05-acq_rel PROPERTY CXX_STANDARD 20)

add_executable(ch05-fence fence.cpp)
set_property(TARGET ch05-fence PROPERTY CXX_STANDARD 20)

add_executable(ch04-threadsafe_queue threadsafe_queue.cpp)
add_executable(ch04-future future.cpp)
add_executable(ch04-p_sort p_sort.cpp)
add_executable(ch04-the_atm_example the_atm_example.cpp)

find_package(stdexec CONFIG REQUIRED)

add_executable(ch04-continuation continuation.cpp)
set_property(TARGET ch04-continuation PROPERTY CXX_STANDARD 20)
target_link_libraries(ch04-continuation PRIVATE STDEXEC::stdexec)

add_executable(ch04-wait_on_futures wait_on_futures.cpp)
set_property(TARGET ch04-wait_on_futures PROPERTY CXX_STANDARD 20)
target_link_libraries(ch04-wait_on_futures PRIVATE STDEXEC::stdexec)

add_executable(ch04-latch latch.cpp)
set_property(TARGET ch04-latch PROPERTY CXX_STANDARD 20)

add_executable(ch04-barrier barrier.cpp)
set_property(TARGET ch04-barrier PROPERTY CXX_STANDARD 20)

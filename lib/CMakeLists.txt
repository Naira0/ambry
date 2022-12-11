project(ambry_lib)

add_library(ambry_lib
        db.cpp db.cpp
        cache.hpp cache.cpp
        io_manager.hpp io_manager.cpp
        types.hpp
        util.hpp util.cpp
        transaction.hpp transaction.cpp
        rw.hpp rw.cpp)
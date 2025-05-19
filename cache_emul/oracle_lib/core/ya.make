LIBRARY()

SRCS(
    request.cpp
    simulation.cpp
)

PEERDIR(
    cache_emul/oracle_lib/proto

    library/cpp/yson/node
)

END()

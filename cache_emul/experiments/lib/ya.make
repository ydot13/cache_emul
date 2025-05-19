LIBRARY()

SRCS(
    config.cpp
    load_requests.cpp
    report.cpp
)

PEERDIR(
    cache_emul/experiments/lib/proto
    cache_emul/oracle_lib

    library/cpp/yson
)

END()

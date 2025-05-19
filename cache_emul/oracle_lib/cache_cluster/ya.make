LIBRARY()

SRCS(
    cache_cluster.cpp
    remotr_cache_instance.cpp
    simulator.cpp
)

PEERDIR(
    cache_emul/oracle_lib/cache_cluster/proto
    cache_emul/oracle_lib/core
)

END()

LIBRARY()

PEERDIR(
    cache_emul/oracle_lib/cache_cluster
    cache_emul/oracle_lib/cli
    cache_emul/oracle_lib/core
    cache_emul/oracle_lib/generators/file_generator
    cache_emul/oracle_lib/generators/prod_generator
    cache_emul/oracle_lib/proto
)

END()

RECURSE(
    proto
)

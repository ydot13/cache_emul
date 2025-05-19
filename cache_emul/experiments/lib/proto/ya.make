PROTO_LIBRARY()

SRCS(
    config.proto
    request.proto
)

PEERDIR(
    cache_emul/oracle_lib/cache_cluster/proto
    cache_emul/oracle_lib/generators/prod_generator/proto
)

END()

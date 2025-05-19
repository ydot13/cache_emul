LIBRARY()

SRCS(
    generator.cpp
)

PEERDIR(
    library/cpp/yson/node
    cache_emul/oracle_lib/core
)

GENERATE_ENUM_SERIALIZATION(
    generator.h
)

END()
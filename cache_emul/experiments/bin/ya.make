PROGRAM()

SRCS(
    main.cpp
    diff_main.cpp
    main_solo.cpp
)

PEERDIR(
    cache_emul/experiments/lib
    cache_emul/oracle_lib
    library/cpp/getopt
    library/cpp/protobuf/util
)

SPLIT_DWARF()

END()

PY3_LIBRARY()

PY_SRCS(
    __init__.py
)

PEERDIR(
    devtools/ya/app
    devtools/ya/build
    devtools/ya/build/build_opts
    devtools/ya/core/yarg
)

END()

RECURSE(
    tests
)

RECURSE_FOR_TESTS(
    bin
    tests
)

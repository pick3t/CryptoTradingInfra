#!/usr/bin/env bash
set -e

RUN_TESTS="OFF"
PROJECT_ROOT=$(realpath $(dirname "${BASH_SOURCE[0]}"))

parse_args() {
    while getopts ":t" opt; do
        case $opt in
            t)
                RUN_TESTS="ON"
                ;;
            *)
                echo "Invalid option: -$OPTARG" >&2
                exit 1
                ;;
        esac
    done
}

create_build_dir() {
    if [ -d build ]; then
        rm -rf build
    fi
    mkdir build
}

detect_os_and_toolchain() {
    if [ "$(uname)" == "Darwin" ]; then
        TOOLCHAIN_FILE="${PROJECT_ROOT}/toolchains/homebrew-llvm-toolchain.cmake"
        CORENUM=$(sysctl -n hw.ncpu)
    else
        TOOLCHAIN_FILE=""
        if command -v nproc &>/dev/null; then
            CORENUM=$(nproc --all)
        else
            CORENUM=$(getconf _NPROCESSORS_ONLN)
        fi
    fi

    JOBS=$((CORENUM - 4))
    if [ $JOBS -lt 1 ]; then
        JOBS=1
    fi
}

build() {
    cmake .. -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} -DRUN_TESTS=${RUN_TESTS}
    make -j $JOBS
}

main() {
    parse_args "$@"
    detect_os_and_toolchain

    create_build_dir
    cd build/
    build

    echo "run test=$RUN_TESTS"
    if [ "$RUN_TESTS" == "ON" ]; then
        cd ${PROJECT_ROOT}/build/tests/
        ./test
    fi
}

main "$@"

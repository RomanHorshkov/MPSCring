#!/usr/bin/env bash
# shellcheck shell=bash
# mpscring_project.sh — what the ABI gate (utils/check_abi.sh, kyr-build share/abi) treats as MPSCring's public ABI: the
# one public header app/mpscring.h, and the prefixes of its types, functions and numeric constants. The build scripts do not read
# this file; the linker map's node is MPSCRING_<major>.
KYR_LIB="mpscring"
KYR_ABI_NAME="MPSCring"
KYR_ABI_INCLUDE_DIR="app"
KYR_ABI_UMBRELLA="mpscring.h"
KYR_ABI_TYPE_PREFIX="mpsc_ring_"
KYR_ABI_FUNC_PREFIX="mpsc_ring_"
KYR_ABI_CONST_PREFIX="MPSC_RING_"
KYR_ABI_NODE="MPSCRING"

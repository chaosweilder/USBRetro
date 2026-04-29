#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DPICO_BOARD=seeed_xiao_rp2040 -B build

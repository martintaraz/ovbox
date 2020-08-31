#!/bin/bash
export JACK_NO_AUDIO_RESERVATION=1
(
    cd cfg
    VER=$(git rev-parse --short HEAD)
    DEV=$(../udpmirror/getmacaddr)
    curl -u device:device "http://box.orlandoviols.com/?setver=${DEV}&ver=ovbox-${VER}"
    ../udpmirror/devconfigclient -l http://box.orlandoviols.com
)

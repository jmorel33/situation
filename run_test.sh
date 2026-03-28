#!/bin/bash
git checkout HEAD sit/situation_impl.h sit/situation_impl_audio.h sit/aud/tone_synth.h
python3 patch_all.py

#!/bin/bash

/usr/bin/adb start-server

/usr/bin/adb wait-for-device

/usr/bin/adb reverse tcp:12345 tcp:12345 || true

cd /home/hak/.local/bin/USBAudio/

exec ./audio_sender

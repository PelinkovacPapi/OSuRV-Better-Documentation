#!/bin/bash

exit 0


./waf configure
./waf build && ./build/test_app_servo_ctrl w 0 25
./waf build && ./build/test_app_servo_ctrl w 0 125


#!/bin/bash

APP_NAME=Raytracer

DISABLED_WARNINGS="-Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable"

CFILES="../src/main.cpp"

pushd ../build/ > /dev/null

g++ $CFILES -DDEBUG -DDENIS_LINUX -Werror -Wall $DISABLED_WARNINGS -o $APP_NAME -lX11 -lrt -lpthread

popd > /dev/null

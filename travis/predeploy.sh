#!/usr/bin/env bash

set -euxo pipefail
TARGET_OS=${TARGET_OS:-$TRAVIS_OS_NAME}

mkdir -p $DEPLOY_DIR
if [[ "$TARGET_OS" == "windows" ]]; then
	mv bin/$BUILD_CONFIG/ocgcore.dll $DEPLOY_DIR
elif [[ "$TARGET_OS" == "android" ]]; then
	ARCH=("armeabi-v7a" "arm64-v8a" "x86" "x86_64" )
	OUTPUT=("libocgcorev7.so" "libocgcorev8.so" "libocgcorex86.so" "libocgcorex64.so")
	for i in {0..3}; do
		CORE="libs/${ARCH[i]}/libocgcore.so"
		if [[ -f "$CORE" ]]; then
			mv $CORE "$DEPLOY_DIR/${OUTPUT[i]}"
		fi
	done
else
	if [[ "$TARGET_OS" == "osx" ]]; then
		mv bin/$BUILD_CONFIG/libocgcore.dylib $DEPLOY_DIR
	else
		mv bin/$BUILD_CONFIG/libocgcore.so $DEPLOY_DIR
	fi
fi

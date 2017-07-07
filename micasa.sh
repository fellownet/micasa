#!/bin/sh

until ./micasa "$@"; do
	echo "Micasa stopped with exit code $?. Restarting ..." >&2
	sleep 1
done
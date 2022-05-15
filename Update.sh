#!/bin/bash
rm main.c
wget http://$1/SmartUmbrellaCan/main.c
make board=galileo
#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
javac HelloWorld.java
javah HelloWorld
gcc -I /opt/jdk1.8.0/include/ -I /opt/jdk1.8.0/include/linux/ -shared libHelloWorld.c -o libHelloWorld.so
java HelloWorld

#!/bin/bash
# Rebuild script for ARM Linux target

echo "Cleaning previous build..."
make clean

echo "Rebuilding for ARM architecture..."
make CC=arm-linux-gcc

echo "Setting execute permissions..."
chmod +x demo

echo "Verifying binary type..."
file demo

echo "Build complete!"


#!/bin/sh

# Verify arguments
if [ $# -ne 2 ]
then
    echo "Error: Two arguments required"
    exit 1
fi

writefile=$1
writestr=$2

# Create directory path if needed
mkdir -p "$(dirname "$writefile")"

# Create/overwrite file
echo "$writestr" > "$writefile"

# Verify creation
if [ $? -ne 0 ]
then
    echo "Error: Could not create file"
    exit 1
fi

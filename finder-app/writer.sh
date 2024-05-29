#!/bin/sh

# First argument is the path to the file
# Second argument is the content to write to the file

if [ $# -ne 2 ]; then
    echo "Usage: $0 <file> <content>"
    exit 1
fi

# Create dirrrectory and file if it doesn't exist
mkdir -p $(dirname $1)

# Write content to the file
echo $2 > $1

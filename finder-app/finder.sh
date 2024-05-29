#!/bin/sh

#accepts 2 arguments
#1. filesdir - directory where files are stored
#2. searchstr - text string to search for within files

filesdir=$1
searchstr=$2

#check if parameters are provided
if [ $# -ne 2 ]; then
  echo "Error: invalid number of arguments"
  exit 1
fi

#check if filesdir is a directory
if [ ! -d $filesdir ]; then
  echo "Error: $filesdir is not a directory"
  exit 1
fi

#check if searchstr is not empty
if [ -z "$searchstr" ]; then
  echo "Error: search string is empty"
  exit 1
fi

#count the number of files in the directory and all subdirectories
num_files=$(find $filesdir -type f | wc -l)

#count the number of matching lines in the files
num_matching_lines=$(grep -r -o $searchstr $filesdir | wc -l)

echo "The number of files are $num_files and the number of matching lines are $num_matching_lines"
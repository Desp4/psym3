## PSYM3
A rewrite in C of my first program
###### what?
This unit goes through the specified folders, records every occurance of every file with a given set of extensions, groups them in chuncks by creation time, shuffles them and saves to a file. Then that file can be traversed to extract those chunks and copy every file from each chunk in a destination folder.
### Usage
I hope `psym --help` will make sense :)
### Building
Targets Windows specifically, so Win API is required. msvc and mingw-w64 work just fine
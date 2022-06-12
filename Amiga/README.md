# SonicArranger packed format converter

The SonicArranger audio application on Amiga has two options to save
the created songs:
1. The standard format that can be loaded into the application again
or
2. a stripped-down format including the player routine to be used in
your own programs.

The tool (written in Amiga SAS/C) in this repository can be used to
convert the second, binary format to the first format again, so it
can be loaded again into SonicArranger.

# Compilation

With an already installed SAS/C you can compile with: `sc sonicconv.c link`

# Usage

Usage: `sonicconv &lt;inputfile&gt; &lt;outputfile&gt;`

`inputfile` is the name of the binary file containing the SonicArranger
module.
`outputfile` is the name of a new file where the converted module will
be written to.

The newly written file should then be readable and playable by
SonicArranger on the Amiga again.
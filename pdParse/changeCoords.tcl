#!/usr/bin/env tclsh
#
# changeCoords.tcl v0.3
# fjkraan@xs4all.nl

if {$argc < 4} {
    error "Usage: tclsh changeCoords.tcl patchName xcoord ycoord line1 ?line2? ..."
}

set patchName [lindex $argv 0]
set xcoord    [lindex $argv 1]
set ycoord    [lindex $argv 2]
set lineArgCount $argc
set lineList {}

#puts "patchName $patchName; xcoord $xcoord; ycoord $ycoord lineArgCount; $lineArgCount"
# line arguments may be numeric or a numeric range "nn-mm" (inclusive)

for {set start 3} {$lineArgCount > $start} {incr start} {
        set arg [lindex $argv $start]
#        puts "$start: $arg"
        if [regexp {(\d+)-(\d+)} $arg lineRange startLine endLine] {
                if {$startLine < $endLine} {
#                        puts "startLine $startLine; endLine $endLine;;  $lineRange"
                        for {set line $startLine} {$line <= $endLine} {incr line} {
                                lappend lineList $line
                        }
                }
        } else {
#                puts "line $arg"
                lappend lineList $arg
        }
}

#puts "lines not to patch: $lineList"

set lineCount 1
set f [open $patchName]
while {[gets $f patchLine] >= 0} {
    if [regexp {\#[AN] } $patchLine] {
#        puts -nonewline "$lineCount: "
        puts $patchLine
        incr lineCount
        continue
    }
    if [regexp {\#[X] } $patchLine] {
        if {[lsearch $lineList $lineCount] == -1} { # >= 0 for matching lines, == -1 for non-matching lines
            if [regexp {\#X obj (\d+) (\d+) (.+)} $patchLine allOfLine orgX orgY restOfLine] {
#                puts -nonewline "$lineCount: "
                puts "#X obj $xcoord $ycoord $restOfLine"
            } else {
#                puts -nonewline "$lineCount: "
                puts $patchLine
            }
        } else {
#            puts -nonewline "$lineCount: "
            puts $patchLine
        }
    } else {
#         puts -nonewline "$lineCount: "
         puts $patchLine
    }
    incr lineCount

}
close $f

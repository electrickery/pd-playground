#!/usr/bin/env tclsh
#
# pdParse.tcl - Parses pd patch files. The goal it to make editing 
# possible. Based on 
# http://puredata.info/Members/pierpower/Favorites/fileformat
# fjkraan@xs4all.nl, 2015-01-04
# version 0.5

# pdParse.tcl currently parses a Pd patch and puts each line in a list
# with an 'item-locator' prefixed. The item locator is the combination
# of the patch number and object number, separated by a dot. Non-object
# items have "--" as object number, as they do not count. Some 
# indentation is added, but this is just presentation. 
# The list is created to make a future streaming editor for pd-patches
# possible, as removing objects and fix the connections can now be done
# on a line-by-line basis.
# The added functionality is that a second argument is interpreted as a 
# object specification to be removed. An example: 
# tclsh pdParse.tcl /usr/lib/pd-extended/extra/vanilla/hradio-help.pd "#X obj -*\d+ -*\d+ pddp/pddplink http://wiki.puredata.info/en/hradio" > new_hradio-help.pd

# ToDo: - get rid of the 'global lines lines' as it shouldn't be needed.

if {$argc < 1} {
    error "Usage: pdParse.tcl fileName \[removePattern\]"
}
set file   [open [lindex $argv 0] r]
set object [lindex $argv 1]

proc fixRestoreObjNumber {lines objCount} {
	# fixes the object number. The #X restore is detected in the 
	# subpatch but it should have an object number from one level
	# higher. Hence this kludge.
	set lastLine [lindex $lines end]
	if {[regexp -- "\#X restore" $lastLine]} {
		regsub {\?\?} $lastLine $objCount lastLine
	}
	return $lastLine
}

proc parseCanvas {parentNo oldSpaces} { # recursive patch parser
	set spaces " $oldSpaces"
	global file file
	global lines lines
	global number number
	incr number 
	set myNumber $number
	set objCount 0
	while {[gets $file line] >= 0} {
	    # constructing complete lines
	    set lastChar [string index $line end]
	    while {![string equal $lastChar \;]} {
		set lineAppendix ""
		gets $file lineAppendix
		set line "$line\n$lineAppendix"
		set lastChar [string index $line end]
	    } 
	    # parsing the lines
	    # end sub patch handler, a #X special case
	    if {[regexp -- "\#X restore" $line]} {
		lappend lines "${parentNo}.??: $line"
		return $lines
	    }
	    # start sub patch handler
	    if {[regexp -- \#N $line]} {
		lappend lines "${myNumber},--: $line"
		set lines [parseCanvas $myNumber $spaces]
		set newLine [fixRestoreObjNumber $lines $objCount]
		set lines [lreplace $lines end end $newLine]
		incr objCount
		continue
	    }
	    # normal object lines handler
	    if {[regexp -- \#X $line]} {
		if { [regexp -- {\#X connect} $line] } {
			lappend lines "${myNumber},--: $line"
		} else {
			lappend lines "${myNumber},$objCount: $line"
			incr objCount
		}
	    }
	    # array contents handler
	    if {[regexp -- \#A $line]} {
		lappend lines "${myNumber},--: $line"
	    }
	}
	return $lines
}

proc addSpace { spaces } {
	return "$spaces "
}

proc removeSpace { spaces } {
	return [string replace $spaces end end]
}

set number -1

set lines {}

set lines [parseCanvas $number ""]

#close $file

set spaces ""

if {[string match $object ""]} {
#puts "===================== indenting lister ====================="
# list items from $lines. format <patchNo>.<objectNo>: <objectSpecification> 
	foreach item  $lines {
		if {[regexp -- "\#X restore" $item]} {
			set spaces [removeSpace $spaces]
		}
		puts "${spaces}$item"
		if {[regexp -- "\#N canvas" $item]} {
			set spaces [addSpace $spaces]
		}
	}
}

set patchNum  ""
set objectNum ""
set outObjectNum ""
set inObjectNum ""
set resultList {}

if {![string match $object ""]} {
#	puts "===================== find and remove ====================="
# The strategy: find the first non-connect object matching $object and
# store its object and patch number. Then find all connects in the 
# correct patch and correct the inlet or outlet object numbers or both. 
# All connects to and from the object are removed.
	foreach item  $lines {
		if {![regexp -- "\#X connect" $item]} { # not the connects
			if {[regexp -- "$object" $item] && [regexp -- $patchNum ""]} { 
#				puts "---> found $item; remove it" 
				set objectLocation [lindex $item 0]
				regexp {(\d+),(\d+):}  $objectLocation coordSet patchNum objectNum
#				puts "patchNum: $patchNum , objectNum: $objectNum"  
			} else {
				lappend resultList $item
			}
		} else { # "\#X connect"
			if {[regexp -- "^${patchNum}," $item]} { # correct patch
				regexp {connect (\d+) \d+ (\d+) \d+;} $item dummy outObjectNum inObjectNum
				if {$objectNum == $inObjectNum || $objectNum == $outObjectNum} {
#					puts "connect *let == match: $item; remove it"
					continue
				} 
				set itemAsList [split $item " "]
				if {$objectNum < $outObjectNum} {
					set newObjectNum [expr $outObjectNum - 1]
#					puts "connect outlet > match: $item; decrease outlet object number $outObjectNum to $newObjectNum"
					set itemAsList [lreplace $itemAsList 3 3 $newObjectNum]
				}
				if {$objectNum < $inObjectNum} {
					set newObjectNum [expr $inObjectNum - 1]
#					puts "connect inlet > match: $item; decrease inlet object number $inObjectNum to $newObjectNum"
					set itemAsList [lreplace $itemAsList 5 5 $newObjectNum]
				}
				set newItem [join $itemAsList]
#				puts $newItem
				lappend resultList $newItem
			} else { # other patches
				lappend resultList $item
			}
		}
	}
} else {
	set resultList $lines
}

if {![string match $object ""]} {
#puts "===================== result lister ====================="
	foreach item $resultList {
	#	puts $item
		puts [join [lreplace [split $item] 0 0]]
	}
}

# Golden-output regression runner.
#
#   tclsh run_golden.tcl <libtclslang.so> <case> <actual-output-dir>
#
# <case> is a .sv/.v file, or a directory whose .sv/.v files form one design.
# The expected output lives next to it as <case-without-extension>.golden.
# A `// top: <name>` line in the case selects which module to dump (default
# `top`).
#
# Exits 0 on match, 1 on mismatch (printing a unified diff). Set
# TCLSLANG_UPDATE_GOLDEN=1 to rewrite the golden file instead of comparing.

lassign $argv lib case actualDir
if {$actualDir eq ""} {
    puts stderr "usage: tclsh run_golden.tcl <lib> <case> <actual-output-dir>"
    exit 2
}

source [file join [file dirname [file normalize [info script]]] lib dump.tcl]

set lib [file normalize $lib]
set actualDir [file normalize $actualDir]
set case [file normalize $case]
load $lib

# Run from the cases directory so file names in diagnostics are relative.
cd [file dirname $case]
set name [file tail $case]
if {[file isdirectory $case]} {
    set files [lsort [glob -directory $name *.sv *.v]]
} else {
    set files [list $name]
}

set top top
foreach f $files {
    set fh [open $f]
    set src [read $fh]
    close $fh
    regexp -line {^//\s*top:\s*(\S+)} $src -> top
}

set actual [dump::design $files $top]

set name [file rootname $name]
set golden [file rootname $case].golden
set actualFile [file join $actualDir $name.actual]
file mkdir $actualDir
set fh [open $actualFile w]
puts -nonewline $fh $actual
close $fh

if {[info exists env(TCLSLANG_UPDATE_GOLDEN)] && $env(TCLSLANG_UPDATE_GOLDEN)} {
    file copy -force $actualFile $golden
    puts "updated $golden"
    exit 0
}

if {![file exists $golden]} {
    puts "missing golden file: $golden"
    puts "---- actual output ----"
    puts -nonewline $actual
    exit 1
}

if {[catch {exec diff -u $golden $actualFile} diff]} {
    puts "FAIL: $name differs from golden"
    puts $diff
    exit 1
}
puts "PASS: $name"

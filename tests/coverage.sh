#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

# Count Intel source mnemonics against the translator's instruction table.
# The table also contains assembler directives, which are excluded here.
LC_ALL=C awk '
    FNR == NR {
        if ($0 ~ /^} tab\[\] = \{$/) in_table = 1
        if ($0 == "};") in_table = 0
        if (!in_table || $0 !~ /^[[:space:]]*\{ "/) next
        split($0, entry, "\"")
        mnemonic = entry[2]
        if (mnemonic !~ /^(org|equ|db|dw|ds)$/) expected[mnemonic] = 1
        next
    }
    {
        sub(/;.*/, "")
        sub(/\r$/, "")
        if (NF == 0) next
        mnemonic = tolower($0 ~ /^[[:space:]]/ ? $1 : $2)
        if (mnemonic in expected) seen[mnemonic] = 1
    }
    END {
        for (mnemonic in expected) {
            total++
            if (mnemonic in seen) covered++
            else print "Missing mnemonic: " mnemonic
        }
        if (total == 0) {
            print "Could not read the instruction table"
            exit 1
        }
        printf "8080 mnemonic source coverage: %d/%d (%.0f%%)\n",
            covered, total, 100 * covered / total
        exit (covered != total)
    }
' trans.c examples/tst8080.asm examples/hello.asm tests/andflags.asm tests/opcodes.asm

#!/usr/bin/env python3
"""Compare the ISS trace with an RTL trace and report the first difference.

usage: python3 compare_traces.py iss_trace.txt rtl_trace.txt
Each line looks like:  <pc> <instruction> [xN=<value>]
"""
import sys


def read_lines(path):
    with open(path) as f:
        return [line.strip() for line in f if line.strip()]


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare_traces.py iss_trace.txt rtl_trace.txt")
    iss = read_lines(sys.argv[1])
    rtl = read_lines(sys.argv[2])
    for i in range(min(len(iss), len(rtl))):
        if iss[i] != rtl[i]:
            print(f"MISMATCH at instruction #{i + 1}")
            print(f"  ISS (expected): {iss[i]}")
            print(f"  RTL (actual)  : {rtl[i]}")
            sys.exit(1)
    if len(iss) != len(rtl):
        print(f"Traces match for {min(len(iss), len(rtl))} instructions, "
              f"but lengths differ (ISS {len(iss)}, RTL {len(rtl)}).")
        sys.exit(1)
    print(f"OK: traces match for all {len(iss)} instructions.")


if __name__ == "__main__":
    main()

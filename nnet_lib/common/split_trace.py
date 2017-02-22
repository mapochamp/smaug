#!/usr/bin/env python
#
# Splits a big trace file of multiple top level functions into separate traces
# for each function.
#
# This is useful for breaking up an accelerator into smaller blocks, where each
# block might call multiple other functions.
#
# Author: Sam Xi

import argparse
import gzip
import os
import re

LABELMAP_START = "%%%% LABEL MAP START %%%%"
LABELMAP_END = "%%%% LABEL MAP END %%%%"

RET_OP = 1

def strip(trace_file):
  for line in trace_file:
    line = line.strip()
    if not line:
      continue
    yield line


def parse_labelmap(trace_file):
  result = LABELMAP_START + "\n"
  for line in strip(trace_file):
    if line == LABELMAP_START:
      continue
    if line == LABELMAP_END:
      result += line
      break
    result += line + "\n"

  result += "\n\n"
  return result

def copy_function(main_trace, first_line, func, sub_trace):
  sub_trace.write(first_line + "\n")

  # Don't strip (makes the copying easier).
  for line in main_trace:
    if line[0] == "0":
      components = line.split(",")
      func_name = components[2]
      opcode = int(components[5])
      # Finish if we are returning from this top level function.
      if func_name == func and opcode == RET_OP:
        sub_trace.write(line)
        return

    sub_trace.write(line)


def split_trace(trace_fname, top_level_funcs):
  """ Splits a dynamic trace of multiple functions into individual traces. """

  top_level_funcs = [f.strip() for f in top_level_funcs]
  sub_trace_files = dict((func, gzip.open("%s.gz" % func, "wb")) for func in top_level_funcs)
  labelmap = ""
  curr_func = ""

  with gzip.open(trace_fname, "rb") as main_trace:
    # Just look for and write the labelmap, if it exists.
    for line in strip(main_trace):
      if line == LABELMAP_START:
        labelmap = parse_labelmap(main_trace)
        break
      break

    if labelmap:
      for f in sub_trace_files.itervalues():
        f.write(labelmap)

    # Now process the remainder of the trace.
    for line in strip(main_trace):
      if line[0] == "0":
        components = line.split(",")
        func_name = components[2]
        if not func_name in top_level_funcs:
          continue

        copy_function(main_trace, line, func_name, sub_trace_files[func_name])

  for f in sub_trace_files.itervalues():
    f.close()

def main():
  parser = argparse.ArgumentParser(description="Splits a long dynamic trace "
      "file of multiple top level functions into separate trace files for "
      "each function. ")
  parser.add_argument("trace", help="Dynamic trace file.")
  parser.add_argument("top_level_functions", help="CSV list of top level functions.")
  args = parser.parse_args()

  split_trace(args.trace, args.top_level_functions.split(","))

if __name__ == "__main__":
  main()

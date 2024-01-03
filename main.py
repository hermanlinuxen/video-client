#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Python3.10 or higher required.

####################
# Libraries
import os, sys, sqlite3, time
from pathlib import Path

####################
# Variables
home = str(Path('~').expanduser())
cutconfigpath = "/.config/video-client/"
configfilename = "video-client.ini"
configpath = os.path.join(home + cutconfigpath)
configfile = os.path.join(configpath + configfilename)

####################
# Colors
class color:
  red = '\033[0;91m'
  green = '\033[0;92m'
  yellow = '\033[0;93m'
  cyan = '\033[36m'
  reset = '\033[0m'

####################
# Compatability check
if not os.name == 'posix':
  print("OS not compatible!")
  exit(1)

####################
# Logging
def log(message, severity = 0): # Severity: 0-Info, 1-Warning, 2-Error, 3-Fatal
  timeprefix = time.strftime('%Y-%m-%d %X', time.localtime())
  if severity == 0:
    typeprefix = " Info:    "
  elif severity == 1:
    typeprefix = " Warning: "
  elif severity == 2:
    typeprefix = " Error:   "
  elif severity == 3:
    typeprefix = " Fatal:   "
  log = timeprefix + typeprefix + message
  print(log)

def setup():
  print()
  run_setup_config = False
  
  if not os.path.exists(configpath): # Check config folder.
    os.makedirs(configpath)
    log("Created config folder: ", configpath)
    run_setup_config = True


def main():
  
  log("test")
  #if not os.path.exists(configfile) or setup_config:
  #  setup()

if __name__ == "__main__":
  main()

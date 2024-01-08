#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Python3.10 or higher required.

####################
# Libraries
import os, sys, sqlite3, time, configparser
from pathlib import Path

####################
# Variables
logfilename = "video-client.log"
configfilename = "video-client.ini"
cutconfigpath = "/.config/video-client/"
home = str(Path('~').expanduser())
configpath = os.path.join(home + cutconfigpath)
configfile = os.path.join(configpath + configfilename)
logfile = os.path.join(configpath + logfilename)
cf = configparser.ConfigParser()

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
# Draw to Terminal
def draw(h, w, txt, drawcolor = "none"):
    if drawcolor == "none":
      print(f"\033[{h};{w}H{txt}")
    else:
      print(drawcolor + f"\033[{h};{w}H{txt}" + color.reset)

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
  file = open(logfile, 'a')
  file.write(log + "\n")
  file.close()

def setup():
  print()
  run_setup_config = False
  
  if not os.path.exists(configpath): # Check config folder.
    os.makedirs(configpath)
    log("Created config folder: ", configpath)
    run_setup_config = True

def draw_ui(w, h):
  os.system("clear")
  minh = 20
  minw = 40
  h, w = os.popen('stty size', 'r').read().split()
  h, w = int(h), int(w)
  if (w < minw) or (h < minh):
    print("Terminal size too small!")
    return
  #print(h, w)

  
  for lineh in range(1, h):
    for linew in range(1, w):
      if linew == 1 or linew == w - 1 or lineh == 1 or lineh == h - 1:
        draw(lineh, linew, "A", color.cyan)
        #print("\033[{0};{1}H{2}".format(lineh, linew, "X"))
      #else:
        #print("\033[{0};{1}H{2}".format(lineh, linew, "-"))
      #time.sleep(0.001)


  
def main():

  # Main variables:

  if not os.path.exists(configpath): # Create if not exist, config folder.
    log("Config folder not exist, creating folder: " + configpath)
    os.makedirs(configpath)
  if not os.path.exists(logfile): # Create if not exist, log file.
    log("Logfile does not exist, creating logfile: " + logfile)
    open(logfile, 'w').close()
  if not os.path.exists(configfile): # Create if not exist, config file, and add main config header.
    log("Configfile does not exist, creating configfile: " + configfile)
    open(configfile, 'w').close()
    editconfigfile = open(configfile, 'w')
    cf.add_section('video-client')
    cf.write(editconfigfile)
    editconfigfile.close()


  os.system("clear")

  h, w = os.popen('stty size', 'r').read().split()
  update_ui = True

  while True:
    if update_ui:
      draw_ui(w, h)
      update_ui = False

    tmp_h, tmp_w = os.popen('stty size', 'r').read().split()
    if not tmp_h == h or not tmp_w == w:
      update_ui = True
      h, w = tmp_h, tmp_w
      
    
        
    
    time.sleep(0.1)

if __name__ == "__main__":
  try:
    main()
  except KeyboardInterrupt:
    os.system("clear")
    print("\nUser Interrupted!")
    log("User exited the program with KeyboardInterrupt!", 2)
    exit(1)

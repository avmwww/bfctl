# Betaflight cli configuration tool 

Supports cli interface configuration of betaflight based controller 

ESC configuration and firmware update 


_Depends on library: https://github.com/avmwww/library_ 

To comppile simple try 
```
make
```

or 
```
make "C_FLAGS=-DDEBUG"
```

Windows version can be compiled using cross compiler 
```
make CROSS_COMPILE=i686-w64-mingw32-
```

Usage 

```
Betaflight control utility, version 1.00, build on Mar 31 2025, 12:36:58
Usage: ./bfctl [options]
Options:
	-b, --baud baud rate, default 115200
	-d, --device serial device, default /dev/ttyUSB0 or /dev/ttyACM0 on Linux
		COM1 on Windows
	-h, --help help usage
	    --raw send raw data, ; or space delimiter
	    --msp use MSP protocol for serial commucations, usually to FC, try "help" to show help
MSP commands:
	info         print board info
	help         help usage
	tx_info      print tx info
	serial       print serial config
	analog       print analog
	cli          enter to cli mode
	dshot        <cmd> send dshot command
	exit         exit from cli mode
	send         <cmd> send cli command
	sendfile     <filename> send file
	reboot       <tag> reboot, tag=1 to DFU mode, tag=0 reboot firmware
	gmotor       get motors values
	smotor       <values>, set motor values, maximum number of motors is 16
	pass         set passthrough serial mode
	tlm          <motor or empty to all> get motor telemetry
	esc_pass     <channel or 255 for all> set esc passthrough
	esc          esc commands, try help to view available commands
ESC commands:
	sset         <channel> <name> <value> set esc setting
	sdump        <channel> esc settings dump
	swrite       <channel> <file> flash settings bin file to esc
	sread        <channel> <file> read settings to bin file from esc
	iname        esc interface name
	write        <channel> <addr> <byte>write byte to esc flash at address
	init         <channel> esc initalize
	reset        <channel> reset esc
	exit         exit from esc mode
	version      <channel> <major> <minor>set esc firmware version
	flash        <channel> <file> flash firmware bin file to esc
	flashall     <channel> <file> alias of commands: pass, init, mark, version, flash, mark, boot, exit
	send         <cmd args> send to esc
	dump         <channel> <addr> <size> esc flash dump, addr for firmware 0x1000, for settings 0x7c00
	mark         <channel> <string> esc set device_info to string
	boot         <channel> <byte> write byte at 0 position of settings, 1 is valid firmware
	help         help usage
```

Examples 

Turn on 4 motors:

```
bfctl --msp "smotor 1100 1100 1100 1100"
```

Turn on 1 motor:

```
bfctl --msp "smotor 2000 0 0 0"
```

Set esc passthrough mode and dump esc channel 1 parameters 

```
bfctl --msp "esc_pass 255"
bfctl --msp "esc sdump 0"
```

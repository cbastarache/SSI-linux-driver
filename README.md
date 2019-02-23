# SSI-linux-driver
creates a GPIO SSI device for reading data from microcontrollers

This driver can be compiled for the Raspberry Pi kernel (tested on 4.19.y).

It is based off of a standard SSI communication structure but uses the GPIO instead of an RS-422 interface.
https://en.wikipedia.org/wiki/Synchronous_Serial_Interface

## Load the module using the load script
  * takes in the number of bits to read from the device as an argument
  * `./load 32` would configure the driver to read 32 bits
  
## Read from the driver
  * the driver gets put into the sysfs interface at `/sys/ssi/stream`
  * to read simply read from the location with a command like `more /sys/ssi/stream`
  * returns a 32 bit integer on each line 

Hardware somewhat compatible with solo1: https://github.com/solokeys/solo1

Boot0 is floating in this PCB, an option bit needs to be set to ignore boot0 for boot mode:  
`openocd -f interface/cmsis-dap.cfg -f target/stm32l4x.cfg -c init -c halt -c "flash list" -c "stm32l4x option_write 0 0x20 0xfbfff8aa" -c reset -c exit`

To read the option bits:  
`openocd -f interface/cmsis-dap.cfg -f target/stm32l4x.cfg -c init -c halt -c "flash list" -c "stm32l4x option_read 0 0x20" -c reset -c exit`

The capacitive threshold for solo1 is set in https://github.com/solokeys/solo1/blob/master/targets/stm32l432/src/sense.c.
To build solo1: install [rustup](https://www.rust-lang.org/tools/install) (note, does not seem available from the apt-get), then in solo1/targets/stm32l432, `make cbor` and `make build-hacker`.
The code will not fit on a STM32L432KB (128kB variant), it will overflow by 20kB.

The code in this repository includes part of the touch sensor code of solo1.
Breakpoints and debugger variable inspection can be used to calibrate the sensor thresholds in the absence of a separate UART line for printf.

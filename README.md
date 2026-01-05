# led_alarm

It's a RGB LED strip based alarm clock with PHP webpage

## C Program technicalities

The C program is meant to run as a daemon and uses a `libconfig` style configuration, in order to update
the state of the leds while running the configuration file is changed and a signal is sent to the process
in order to reload the configuration file, this forms a rudimentary one-way configuration channel.

### main.c

In `main.c`, the main program operation is performed in `main_loop` while initialization and teardown takes
place in `main`. `handle_flags` is the primary method for handling inter-process-communication / signals.
Upon intialization of the signal traps, ws2812 led strip, pthread mutexes, and any/all memory allocations, an
optional PID file is created. The `flags` are then set to the `DEFAULT_FLAGS` value, and the `main_loop` is
entered, immediately triggerring the same code path that sending a `SIGUSR1` would do, leading to the 
configuration implicity being loaded during startup.

`main_loop` performs all operations, including getting the time, checking for signals, and does this regularly
once per second. It has two sources of time, system and fake time, system time is based from the kernel's local
time and thus has timezones pre-applied. Fake time starts at zero, and once per main loop event it is incremented
by the configuration value of `fake_time`, once it gets too low or too high, it is automatically reset to the
nearest bound, by default when using fake_time the current day is used, this can be overriden via `fake_day`.

#### Accepted signals

Signal name | Function
------------|-------------------------------------
`SIGINT`    | Gracefully stops the program
`SIGUSR1`   | Reloads the configuration file
`SIGUSR2`   | Resets the faked time value to zero

### config.c

Configuration parsing takes place in `config.c` defined by `config.h`. The type value in here isn't a simple
integer, however it is an mask with different bits representing different value types, the lowest nibble
represents the base c type. This allows all config entries to be programmatically defined in an array. Some
configuration entries append "overrides", these are essentially booleans that are combined into a single
large mask containing all non-user-relevent / internally used flags set when the configuration is parsed.

#### Configuration options
 Option           | Type  | Description
------------------|-------|-----------------------------------------
`normal-time`     | int   | Default led alarm begin time in minutes
`ramp-up-time`    | int   | LED light ramp up time in minutes
`keep-on-time`    | int   | LED light keep on time in minutes
`brightness`      | int   | Total LED Strip brightness, 0 - 255 accepted
`override-time`   | int   | Force a new begin time, this is auto_reset
`override-color`  | int   | Ignore all alarms, force a specific color
`sunday-time`     | int   | Alarm begin time for sunday
`monday-time`     | int   | Alarm begin time for monday
`tuesday-time`    | int   | Alarm begin time for tuesday
`wednesday-time`  | int   | Alarm begin time for wednesday
`thursday-time`   | int   | Alarm begin time for thursday
`friday-time`     | int   | Alarm begin time for friday
`saturday-time`   | int   | Alarm begin time for saturday
`fake-time`       | int   | Fake time increment value
`fake-day`        | int   | Fake day to use in fake_time is set
`verbosity`       | int   | Verbosity level, 0 - info, 1 - verbose, 2 - debug
`noise-type`      | int   | Type of noise to apply, 0 - Off, 1 - Random, 2 - Sine, 3 - Clouds / Perlin
`noise-intensity` | int   | Intensity of noise from 0-100
`line-fade`       | int   | Intensity of line fade (brightness drop per led), 0-1000

#### Configuration types
 Type  | In C | Description       | Example
-------|------|-------------------|-----------------------------
 int   | int  | Generic integer   | `0`, `68`, `-13`
 time  | int  | TODO - Time value | `7:00 am`, `7:00`
 dur   | int  | TODO - Duration   | `4h`, `5m`, `2h 18m`
 color | int  | TODO - RGB Color  | `#FF0000`, `red`, `255,0,0`

### args.c

Argument parsing takes place in `args.c` and uses the `getopts_long` with code shamelessly stolen from the
rpi_ws2811x libraries test utilities. Since I didn't write most of it, this file is the only readable code.
 
Please see `args.c` for what arguments exist, or run with `-h/--help` as I couldn't be bothered to add a table.

## Web interface

The web interface is built for an ultra-minimal lighttpd + php setup in buildroot

It is not yet implemented

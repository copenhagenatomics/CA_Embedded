# Specification Document - AC

Author: Luke Walker
Date: 28/05/2026

# Introduction

This document contains the specification for the AC board/box project. Sub-components may have additional specifications in separate documents, where appropriate.

# Specification

* Shall accept the following commands:
  * Standard Copenhagen Atomics (CA) protocol commands
  * Standard CA AC/DC protocol commands
  * "fan on" - forces the fan to be on
  * "fan off" - enables the fan to work in normal overheat control mode
* Implementation of all on / all off:
  * all on shall be specified with a duration (in seconds), otherwise return an undefined command message
* Implementation of portState:
  * portState "on" commands must specify a duration (in seconds), otherwise return an undefined command message
  * If portState "on" commands specify a percentage, the AC signal must be PWM-d with a corresponding duty cycle. For other PWM details, see below.
  * Any command to turn a pin on 100% (e.g. all on, pX on) shall take effect immediately
  * Any command to turn a pin on 0% to 100% (e.g. pX on YY%) should take effect on the next PWM period
  * Any command to turn a pin off, or on 0% (e.g. pX on 0%, all off) should take effect immediately
* PWM
  * AC PWM frequency should be 1 Hz, resolution 100 (to match zero crossings on a 50 Hz signal)
    * Note: How to handle 60 Hz?
  * To reduce noise spikes on the input, multiple PWM channels must be staggered. 
  * The difference between the maximum and minimum current draw (from the input) should be no more than a single output channel's current draw
* Overheat Control (if user has not forced fan on)
  * If the heatsink temperature rises above 55 degC, enable the fan
  * If the heatsink temperature rises above 70 degC, set the overheat error bit and enable the fan
  * If the heatsink temperature sinks below 50 degC, disable the fan
* E-fuse
  * If the monitored current on a particular channel exceeds 12A RMS, measured over 0.1s, the channel shall be disabled and the overcurrent error bit set.
  * The limit should be assessed and action taken as frequently as possible
  * The overcurrent bit should be cleared at the next command affecting that channel
  * The channel should continue to work as normal at the next command
  * The current limit should be configurable, per channel, in calibration memory (calibration items 1, 2, 3 and 4)
* Calibration
  * Calibration inputs which are out of range should be detected and rejected with an error message
  * Calibration data should be saved using a CRC to detect data corruption. If corrupted data is detected, issue a warning and reset to the default.
* Uptime
  * uptime data should record on time and PWM on-time per channel, in minutes
* Fault recording
  * Fatal fault exceptions should trigger an appropriate exception handler
  * The exception handler should capture stack and register traces and record them at the end of calibration memory
  * After attempting to save the stack / register traces, the exception handler should reset the device
  * Upon boot, the FW should check if a fault exception was triggered, and print the traces if so



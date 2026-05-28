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
  * If portState "on" commands specify a percentage, the AC signal must be PWM-d with the corresponding frequency (See below)
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




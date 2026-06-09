# Specification Document - DC

Author: Luke Walker
Date: 01/06/2026

# Introduction

This document contains the specification for the DC board/box project. Sub-components may have additional specifications in separate documents, where appropriate.

# Specification

* Shall accept the following commands:
  * Standard Copenhagen Atomics (CA) protocol commands
  * Standard CA AC/DC protocol commands
* Implementation of all on / all off:
  * all on shall be specified with a duration (in seconds), otherwise return an undefined command message
* Implementation of portState:
  * If portState "on" commands specify a percentage, the DC signal must be PWM-d with a corresponding duty cycle. For other PWM details, see below.
  * All commands may take effect on the subsequent PWM period
* PWM
  * DC PWM period frequency should be 1 kHz, resolution 1000
* Timeout
  * In the event of no USB communication for greater than 5 s, all ports should be shut off.
* Buttons
  * Take input from 6 buttons. When the button is pressed, the corresponding channel should be "on 100%", for as long as the button is pressed.
  * Turn on behaviour from buttons should operate "OR" with regular commands. E.g. channel on = button on OR command on.
  * Buttons should not effect durations from regular commands, unless the button is held on over the end of a duration, in which case the channel should remain on until the button is released.
* E-fuse
  * If the monitored current on a particular channel exceeds the limit (6 A default) the channel shall be disabled and the overcurrent error bit set.
  * The limit should be assessed and action taken within 0.1 s
  * The overcurrent bit should be cleared at the next command affecting that channel
  * The channel should continue to work as normal at the next command
  * The current limit should be configurable, per channel, in calibration memory (calibration items 1 to 6)
  * The current limiting action should also apply to button presses, with the same logic as regular commands
* Calibration
  * Calibration inputs which are out of range should be detected and rejected with an error message
  * Calibration data should be saved using a CRC to detect data corruption. If corrupted data is detected, issue a warning and reset to the default.
* Uptime
  * uptime data should record on time and PWM on-time per channel, in minutes




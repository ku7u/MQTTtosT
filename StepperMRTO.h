/*
 * StepperMRTO.h - Stepper library for model railroad turnout control
 *
 * Derived from original stepper.h and modifications by Tom Igoe et. al.
 * Total rewrite for microstepper device         by George Hofmann 1/5/2022
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * Drives a bipolar stepper motor for model railroad turnout control
 *
 *
 * The sequence of control signals for 4 control wires is as follows:
 *
 * Step C0 C1 C2 C3
 *    1  1  0  1  0
 *    2  0  1  1  0
 *    3  0  1  0  1
 *    4  1  0  0  1
 *
 */

// ensure this library description is only included once
#ifndef StepperMRTO_h
#define StepperMRTO_h
#include <Arduino.h>

// library interface description
class StepperMRTO
{
public:
  // constructors:
  StepperMRTO(int stepsPerRevolution, int motorAPlus, int motorAMinus,
              int motorBPlus, int motorBMinus);

  // speed setter method:
  void setSpeed(long rpmSpeed);

  // set the magnitude of the stroke in steps
  void setStrokeSteps(int strokeSteps);

  // start method primes the code to run on subsequent loops
  // either a button push or a remote command could call this
  void setReady(bool direction);

  // repeatedly called in the loop
  bool run(void);

  // set current to zero at end of stroke to prevent stressing output pin
  void release(void);

  // limit length of individual step to control torque
  void setTorqueLimit(unsigned long stepLimit);

  //check running state, running or not
  bool getRunState();

  //get ready to run state
  bool getReadyState();

  // set the default run direction
  void setReversed(bool reversed);

  // report the last position
  int getLastCommanded();

  int version(void);

private:
  void stepMotor(int this_step);

  bool _direction;          // Direction of rotation
  uint16_t _stepInterval;   // delay between steps, in ms, based on speed
  uint16_t _torqueInterval; // shortened interval to reduce torque and average current
  uint16_t _strokeSteps;    // steps to take in this run
  uint32_t _now;
  uint32_t _lastStepStartTime; // time stamp in us of when the last step was started
  uint32_t _lastSliceStartTime;
  bool _isRunning;
  bool _readyToRun;
  bool _currentDirection;
  int _currentStep; // which step the motor is on
  int _stepsLeftToGo;
  int _stepsPerRevolution;
  int _lastCommanded;
  bool _reversed;

  // motor pin numbers:
  int _motorAPlus;
  int _motorAMinus;
  int _motorBPlus;
  int _motorBMinus;

};

#endif

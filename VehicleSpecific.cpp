/*
 * VehicleSpecific.h - All code specific to your particular vehicle. This keeps the rest of code generic.
 *
 Copyright (c) 2016 Collin Kidder, Michael Neuweiler, Charles Galpin

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The purpose of this file is to create a "device" that is used to interface with all of the other enabled
 * devices in order to create a custom control system for your car. Each main device has configuration options
 * and is meant for generalized usage. But, what if you want to go further? Perhaps you've got a PowerKeyPro
 * keypad and you want to be able to select different regen powers or drive power settings? The default device
 * drivers don't support such things because they're generic. Or, perhaps you want to allow
 * your BMS and motor controller to talk? Maybe the BMS can limit regen or drive power automatically based on battery
 * conditions. The code doesn't currently support such things but it would be possible to read the BMS here and then update
 * the motor controller. The options are endless. The below code will be updated to show what Collin has done with a reference car
 * in order to show what is possible and how it could be coded. Feel free to delete any code not appropriate to your car and/or
 * add any extra code you need to make GEVCU do what you want.
 * This is a device like any other so it is disabled by default and will have to be enabled if you need to use it.
*/

#include "VehicleSpecific.h"

#define PUMPPWM_PIN         7
#define IGNITION_IN_PIN     2
#define IGNITION_OUT_PIN    0

/*
 * Constructor
 */
VehicleSpecific::VehicleSpecific() : Device() {
    prefsHandler = new PrefHandler(VEHICLESPECIFIC);
    
    commonName = "VehicleSpecific";
    didInitialSetup = false;
    waitTicksStartup = 20;
}

/*
 * Setup the device.
 */
void VehicleSpecific::setup() {
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: VehicleSpecific (id: %X, %X)", VEHICLESPECIFIC, this);

    loadConfiguration();

    Device::setup(); //call base class

    //Use same tick interval as a pot based pedal would have used.
    tickHandler.attach(this, CFG_TICK_INTERVAL_VEHICLE);
}

/*
 * Process a timer event. This is where you should be doing checks and updates. By default this
 * function is called 10 times per second.
 */
void VehicleSpecific::handleTick() {
    Device::handleTick(); // Call parent which controls the workflow
    Logger::debug("VS Tick Handler");

    MotorController * motor = deviceManager.getMotorController();

    if (waitTicksStartup > 0) 
    {
        waitTicksStartup--;
        if (waitTicksStartup == 0) {
            Logger::info("Vehicle specific: Done waiting");
        }
        return;
    }
    
    if (!didInitialSetup)
    {
        didInitialSetup = true;
        Logger::info("Vehicle specific: Initial setup");
        // disable ignition
        systemIO.setDigitalOutput(IGNITION_OUT_PIN, false);
        // default pump speed
        systemIO.setAnalogOut(PUMPPWM_PIN, 200);
    }
    // check ignition
    bool ignitionBttnState = systemIO.getDigitalIn(IGNITION_IN_PIN);
    if (ignitionBttnState != lastIgnitionBttnState) {
        lastIgnitionBttnStateChange = millis();
        Logger::info("Vehicle specific: Ignition bttn changed to %d", ignitionBttnState);
    }
    unsigned long duration = millis() - lastIgnitionBttnStateChange;
    if (ignitionState == false) {
        // check if button held for 3 seconds
        if (ignitionBttnState == true && lastIgnitionBttnStateChange > 3000) {
            ignitionState = true;
            Logger::info("Vehicle specific: Ignition on");
        }
    }
    else {
        // check if button held for 3 seconds
        if (ignitionBttnState == true && lastIgnitionBttnStateChange > 3000) {
            ignitionState = false;
            Logger::info("Vehicle specific: Ignition off");
        }   
    }
    systemIO.setDigitalOutput(IGNITION_OUT_PIN, ignitionState);
    // Handle temperature monitoring for the coolant pump
    int16_t tempMotor = motor->getTemperatureMotor();
    int16_t tempInverter = motor->getTemperatureInverter();
    int16_t tempSystem = motor->getTemperatureSystem();
    Logger::debug("Motor Temp: %d, Inverter Temp: %d, System Temp: %d", tempMotor, tempInverter, tempSystem);
    if (tempInverter > 100) {
        systemIO.setAnalogOut(PUMPPWM_PIN, 255);
    } else {
        systemIO.setAnalogOut(PUMPPWM_PIN, 200);
    }
}

/*
 * Return the device ID
 */
DeviceId VehicleSpecific::getId() {
    return (VEHICLESPECIFIC);
}

DeviceType VehicleSpecific::getType()
{
    return DEVICE_MISC;
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void VehicleSpecific::loadConfiguration() {

    Device::loadConfiguration(); // call parent
}

/*
 * Store the current configuration to EEPROM
 */
void VehicleSpecific::saveConfiguration() {

    Device::saveConfiguration(); // call parent
}



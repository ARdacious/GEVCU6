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

/*
 * Constructor
 */
VehicleSpecific::VehicleSpecific() : Device() {
    prefsHandler = new PrefHandler(VEHICLESPECIFIC);
    
    commonName = "VehicleSpecific";
    didInitialSetup = false;
    waitTicksStartup = (5000000ul / CFG_TICK_INTERVAL_VEHICLE);
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

#define PRECHARGE      0
#define MAINCONTACTOR  1
#define PUMPPWM        6
#define IGNITION_PIN   2
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
        return;
    }
    
    if (!didInitialSetup)
    {
        didInitialSetup = true;
        systemIO.setDigitalOutput(PRECHARGE, false);
        systemIO.setDigitalOutput(MAINCONTACTOR, false);
        systemIO.setAnalogOut(PUMPPWM, 200);
    }

    bool pinState = systemIO.getDigitalIn(IGNITION_PIN);
    if (pinState != lastIgnitionState) {
        // update timestamp
        lastIgnitionStateChange = millis();
        lastIgnitionState = pinState;
    }
    if (millis() - lastIgnitionState > 3000) {
        if (pinState) {
            if (!preChargeEnabled) {
                // Turn on Precharge
                preChargeEnabled = true;
                preChargeEnabledAt = millis();
            }
            else {
                // Turn off HV & Precharge
                preChargeEnabled = false;
                mainContactorsEnabled = false;
            }
        }
    }
    if (preChargeEnabled) {
        if (millis() - preChargeEnabledAt > 10000) {
            // Turn on HV
            mainContactorsEnabled = true;
        }
    }
    systemIO.setDigitalOutput(PRECHARGE, preChargeEnabled);
    systemIO.setDigitalOutput(MAINCONTACTOR, mainContactorsEnabled);
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



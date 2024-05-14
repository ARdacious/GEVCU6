/*
 * PotGearSelector.cpp
 
   Monitor an ADC pin and use it to select which gear we should be in.
 
Copyright (c) 2022 Collin Kidder

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

#include "PotGearSelector.h"

PotGearSelector::PotGearSelector() : Device()
{
    prefsHandler = new PrefHandler(POTGEAR);
    commonName = "Potentiometer Gear Selector";
    positionAccum = 0;
    counter = 0;
}

void PotGearSelector::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: Pot Gear Selector (id:%X, %X)", POTGEAR, this);

    loadConfiguration();

    Device::setup(); // run the parent class version of this function

    tickHandler.attach(this, CFG_TICK_INTERVAL_POT_THROTTLE);
}

void PotGearSelector::handleTick() {
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();
    MotorController* motorController = (MotorController*) deviceManager.getMotorController();

    Device::handleTick(); //kick the ball up to papa

    if (config->adcPin == 255) return;

    int16_t gearSelector = systemIO.getAnalogIn(config->adcPin);

    //this will trend the gear position. It's deceptive. You might think
    //this would trend it over 5 readings but that's not how the math actually
    //shakes out. It takes far longer, perhaps 10-15 readings to actually get
    //pretty close.It takes on the order of 30 iterations to actually settle
    //at the exact value. 
    positionAccum = ((positionAccum * 4) + gearSelector) / 5;
    gearSelector = positionAccum;

    //and even then, don't actually look at the readings except every so often
    counter++;
    if (counter < 8) return;

    counter = 0;

    Logger::debug("PotGear Val: %d", gearSelector);

    uint16_t diffs[4];

    diffs[0] = abs(gearSelector - config->parkPosition);
    diffs[1] = abs(gearSelector - config->neutralPosition);
    diffs[2] = abs(gearSelector - config->drivePosition);
    diffs[3] = abs(gearSelector - config->reversePosition);

    MotorController::Gears gear = MotorController::NEUTRAL;
    MotorController::OperationState opState = MotorController::DISABLED;

    uint16_t lowest = 0xFFFF;

    //to give some hysteresis we might actually change the below comparisons to be more like:
    //if (diff[0] < (lowest - config->hysteresis)) or if (diffs[0] < (lowest - config->hysteresis/2)) or something to that effect.

    if (diffs[0] < lowest) //park
    {
        gear = MotorController::NEUTRAL;
        opState = MotorController::DISABLED;
        lowest = diffs[0];
        Logger::debug("Setting gear to Park(neutral)");
    }

    if (diffs[1] < lowest) //neutral
    {
        gear = MotorController::NEUTRAL;
        opState = MotorController::DISABLED;
        lowest = diffs[1];
        Logger::debug("Setting gear to Neutral");
    }

    if (diffs[2] < lowest) //drive
    {
        gear = MotorController::DRIVE;
        opState = MotorController::ENABLE;
        lowest = diffs[2];
        Logger::debug("Setting gear to Drive");
    }

    if (diffs[3] < lowest) //Reverse
    {
        gear = MotorController::REVERSE;
        opState = MotorController::ENABLE;
        lowest = diffs[3];
        Logger::debug("Setting gear to Reverse");
    }

    //if the value is too close to the outer edges then something is wrong
    if (gearSelector < 100 || gearSelector > 4000)
    {
        Logger::debug("Gear selector ADC out of bounds! Is it misconfigured?");
        gear = MotorController::NEUTRAL;
        opState = MotorController::DISABLED;
    }

    //finally, set the new mode if a motor controller is truly defined.       
    if (motorController)
    {
        motorController->setOpState(opState);
        motorController->setSelectedGear(gear);
    }
 }

DeviceId PotGearSelector::getId() {
    return (POTGEAR);
}

uint32_t PotGearSelector::getTickInterval()
{
    return CFG_TICK_INTERVAL_DCDC;
}

DeviceType PotGearSelector::getType()
{
    return DEVICE_MISC;
}

void PotGearSelector::loadConfiguration()
{
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();

    if (!config) {
        config = new PotGearSelConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        Logger::info((char *)Constants::validChecksum);
        prefsHandler->read(EEGEARSEL_ADC, &config->adcPin);
        prefsHandler->read(EEGEARSEL_PARK, &config->parkPosition);
        prefsHandler->read(EEGEARSEL_REVERSE, &config->reversePosition);
        prefsHandler->read(EEGEARSEL_NEUTRAL, &config->neutralPosition);
        prefsHandler->read(EEGEARSEL_DRIVE, &config->drivePosition);
        prefsHandler->read(EEGEARSEL_HYST, &config->hysteresis);
    }
    else { //checksum invalid. Reinitialize values and store to EEPROM
        Logger::info((char *)Constants::invalidChecksum);
        config->adcPin = 2;
        config->parkPosition = 1830;
        config->reversePosition = 1630;
        config->neutralPosition = 1400;
        config->drivePosition = 1200;
        config->hysteresis = 80;
        saveConfiguration();
    }
}

void PotGearSelector::saveConfiguration()
{
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();

    Device::saveConfiguration();

    prefsHandler->write(EEGEARSEL_ADC, config->adcPin);
    prefsHandler->write(EEGEARSEL_PARK, config->parkPosition);
    prefsHandler->write(EEGEARSEL_REVERSE, config->reversePosition);
    prefsHandler->write(EEGEARSEL_NEUTRAL, config->neutralPosition);
    prefsHandler->write(EEGEARSEL_DRIVE, config->drivePosition);
    prefsHandler->write(EEGEARSEL_HYST, config->hysteresis);
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();

    Logger::debug("Wrote pot gear cfg");
}

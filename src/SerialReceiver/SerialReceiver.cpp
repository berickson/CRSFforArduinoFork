/**
 * @file SerialReceiver.cpp
 * @author Cassandra "ZZ Cat" Robinson (nicad.heli.flier@gmail.com)
 * @brief The Serial Receiver layer for the CRSF for Arduino library.
 * @version 1.0.0
 * @date 2024-2-14
 *
 * @copyright Copyright (c) 2024, Cassandra "ZZ Cat" Robinson. All rights reserved.
 *
 * @section License GNU General Public License v3.0
 * This source file is a part of the CRSF for Arduino library.
 * CRSF for Arduino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CRSF for Arduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CRSF for Arduino.  If not, see <https://www.gnu.org/licenses/>.
 * 
 */

#include "SerialReceiver.hpp"
#include "../hal/CompatibilityTable/CompatibilityTable.hpp"
#include "Arduino.h"

using namespace crsfProtocol;
using namespace hal;

namespace serialReceiverLayer
{
    SerialReceiver::SerialReceiver()
    {
#if defined(ARDUINO_ARCH_STM32)
#if defined(HAVE_HWSERIAL1)
        _uart = &Serial1;
#elif defined(HAVE_HWSERIAL2)
        _uart = &Serial2;
#elif defined(HAVE_HWSERIAL3)
        _uart = &Serial3;
#endif
#else
        _uart = &Serial1;
#endif

#if CRSF_RC_ENABLED > 0
        _rcChannels = new rcChannels_t;
        _rcChannels->valid = false;
        _rcChannels->failsafe = false;
        memset(_rcChannels->value, 0, sizeof(_rcChannels->value));
#if CRSF_FLIGHTMODES_ENABLED > 0
        _flightModes = new flightMode_t[FLIGHT_MODE_COUNT];
#endif
#endif
    }

    SerialReceiver::SerialReceiver(HardwareSerial *hwUartPort)
    {
        _uart = hwUartPort;

#if CRSF_RC_ENABLED > 0
        _rcChannels = new rcChannels_t;
        _rcChannels->valid = false;
        _rcChannels->failsafe = false;
        memset(_rcChannels->value, 0, sizeof(_rcChannels->value));
#if CRSF_FLIGHTMODES_ENABLED > 0
        _flightModes = new flightMode_t[FLIGHT_MODE_COUNT];
#endif
#endif
    }

    SerialReceiver::~SerialReceiver()
    {
        _uart = nullptr;

#if CRSF_RC_ENABLED > 0
        delete _rcChannels;
        _rcChannels = nullptr;
#if CRSF_FLIGHTMODES_ENABLED > 0
        delete[] _flightModes;
#endif
#endif
    }

    bool SerialReceiver::begin()
    {
#if CRSF_DEBUG_ENABLED > 0
        // Debug.
        CRSF_DEBUG_SERIAL_PORT.print("[Serial Receiver | INFO]: Initialising... ");
#endif
        // _uart->enterCriticalSection();

#if CRSF_RC_ENABLED > 0 && CRSF_RC_INITIALISE_CHANNELS > 0
        // Initialize the RC Channels.
        // Arm is set to 178 (1000us) to prevent the FC from arming.
        // Throttle is set to 172 (988us) to prevent the ESCs from arming. All other channels are set to 992 (1500us).
        for (size_t i = 0; i < RC_CHANNEL_COUNT; i++)
        {
#if CRSF_RC_INITIALISE_ARMCHANNEL > 0 && CRSF_RC_INITIALISE_THROTTLECHANNEL > 0
            if (i == RC_CHANNEL_AUX1 || i == RC_CHANNEL_THROTTLE)
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_MIN;
            }
            else
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_CENTER;
            }

#elif CRSF_RC_INITIALISE_ARMCHANNEL > 0
            if (i == RC_CHANNEL_AUX1)
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_MIN;
            }
            else
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_CENTER;
            }

#elif CRSF_RC_INITIALISE_THROTTLECHANNEL > 0
            if (i == RC_CHANNEL_THROTTLE)
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_MIN;
            }
            else
            {
                _rcChannels->value[i] = CRSF_RC_CHANNEL_CENTER;
            }
#else
            _rcChannels->value[i] = CRSF_RC_CHANNEL_CENTER;
#endif
        }
#endif

        CompatibilityTable *ct = new CompatibilityTable();
        if (!ct->isDevboardCompatible(ct->getDevboardName()))
        {
            delete ct;
            ct = nullptr;
            // _uart->exitCriticalSection();

#if CRSF_DEBUG_ENABLED > 0
            // Debug.
            CRSF_DEBUG_SERIAL_PORT.println("\r\n[Serial Receiver | FATAL ERROR]: Devboard is not compatible with CRSF Protocol.");
#endif
            return false;
        }

        delete ct;
        ct = nullptr;

        // Initialize the CRSF Protocol.
        crsf = new CRSF();

        // Check that the CRSF Protocol was initialized successfully.
        if (crsf == nullptr)
        {
            // _uart->exitCriticalSection();

#if CRSF_DEBUG_ENABLED > 0
            // Debug.
            CRSF_DEBUG_SERIAL_PORT.println("\n[Serial Receiver | FATAL ERROR]: CRSF Protocol could not be initialized.");
#endif
            return false;
        }

        crsf->begin();
        crsf->setFrameTime(BAUD_RATE, 10);
        _uart->begin(BAUD_RATE);

#if CRSF_TELEMETRY_ENABLED > 0
        // Initialise telemetry.
        telemetry = new Telemetry();

        // Check that the telemetry was initialised successfully.
        if (telemetry == nullptr)
        {
            // _uart->exitCriticalSection();

#if CRSF_DEBUG_ENABLED > 0
            // Debug.
            CRSF_DEBUG_SERIAL_PORT.println("\n[Serial Receiver | FATAL ERROR]: Telemetry could not be initialized.");
#endif
            return false;
        }

        telemetry->begin();
#endif

        // _uart->exitCriticalSection();

        // Clear the UART buffer.
        _uart->flush();
        while (_uart->available() > 0)
        {
            _uart->read();
        }

#if CRSF_DEBUG_ENABLED > 0
        // Debug.
        CRSF_DEBUG_SERIAL_PORT.println("Done.");
#endif
        return true;
    }

    void SerialReceiver::end()
    {
        _uart->flush();
        while (_uart->available() > 0)
        {
            _uart->read();
        }

        // _uart->enterCriticalSection();
        _uart->end();
        // _uart->clearUART();

        // Check if the CRSF Protocol was initialized.
        if (crsf != nullptr)
        {
            crsf->end();
            delete crsf;
        }

#if CRSF_TELEMETRY_ENABLED > 0
        // Check if telemetry was initialized.
        if (telemetry != nullptr)
        {
            telemetry->end();
            delete telemetry;
        }
#endif
        // _uart->exitCriticalSection();
    }

#if CRSF_RC_ENABLED > 0 || CRSF_TELEMETRY_ENABLED > 0 || CRSF_LINK_STATISTICS_ENABLED > 0
    void SerialReceiver::processFrames()
    {
        while (_uart->available() > 0)
        {
            if (crsf->receiveFrames((uint8_t)_uart->read()))
            {
                flushRemainingFrames();

#if CRSF_LINK_STATISTICS_ENABLED > 0
                // Handle link statistics.
                crsf->getLinkStatistics(&_linkStatistics);
                if (_linkStatisticsCallback != nullptr)
                {
                    _linkStatisticsCallback(_linkStatistics);
                }
#endif

#if CRSF_TELEMETRY_ENABLED > 0
                // Check if it is time to send telemetry.
                if (telemetry->update())
                {
                    telemetry->sendTelemetryData(_uart);
                }
#endif
            }
        }

#if CRSF_RC_ENABLED > 0
        // Update the RC Channels.
        crsf->getFailSafe(&_rcChannels->failsafe);
        crsf->getRcChannels(_rcChannels->value);
        if (_rcChannelsCallback != nullptr)
        {
            _rcChannelsCallback(_rcChannels);
        }
#endif
    }
#endif

#if CRSF_LINK_STATISTICS_ENABLED > 0
    void SerialReceiver::setLinkStatisticsCallback(linkStatisticsCallback_t callback)
    {
        _linkStatisticsCallback = callback;
    }
#endif

#if CRSF_RC_ENABLED > 0 || CRSF_TELEMETRY_ENABLED > 0 || CRSF_LINK_STATISTICS_ENABLED > 0
    void SerialReceiver::flushRemainingFrames()
    {
        _uart->flush();
        while (_uart->available() > 0)
        {
            _uart->read();
        }
    }
#endif

#if CRSF_RC_ENABLED > 0
    void SerialReceiver::setRcChannelsCallback(rcChannelsCallback_t callback)
    {
        _rcChannelsCallback = callback;
    }

    uint16_t SerialReceiver::readRcChannel(uint8_t channel, bool raw)
    {
        if (channel <= 15)
        {
            if (raw == true)
            {
                return _rcChannels->value[channel];
            }
            else
            {
                /* Convert RC value from raw to microseconds.
                - Mininum: 172 (988us)
                - Middle: 992 (1500us)
                - Maximum: 1811 (2012us)
                - Scale factor = (2012 - 988) / (1811 - 172) = 0.62477120195241
                - Offset = 988 - 172 * 0.62477120195241 = 880.53935326418548
                */
                return (uint16_t)((_rcChannels->value[channel] * 0.62477120195241F) + 881);
            }
        }
        else
        {
            return 0;
        }
    }

    uint16_t SerialReceiver::getChannel(uint8_t channel)
    {
        return readRcChannel(channel, true);
    }

    uint16_t SerialReceiver::rcToUs(uint16_t rc)
    {
        return (uint16_t)((rc * 0.62477120195241F) + 881);
    }

    uint16_t SerialReceiver::usToRc(uint16_t us)
    {
        return (uint16_t)((us - 881) / 0.62477120195241F);
    }

#if CRSF_FLIGHTMODES_ENABLED > 0
    bool SerialReceiver::setFlightMode(flightModeId_t flightMode, uint8_t channel, uint16_t min, uint16_t max)
    {
        if (flightMode < FLIGHT_MODE_COUNT && channel <= 15)
        {
            _flightModes[flightMode].channel = channel;
            _flightModes[flightMode].min = min;
            _flightModes[flightMode].max = max;
            return true;
        }
        else
        {
            return false;
        }
    }

    void SerialReceiver::setFlightModeCallback(flightModeCallback_t callback)
    {
        _flightModeCallback = callback;
    }

    void SerialReceiver::handleFlightMode()
    {
        if (_flightModeCallback != nullptr)
        {
            for (size_t i = 0; i < (size_t)FLIGHT_MODE_COUNT; i++)
            {
                if (_rcChannels[_flightModes[i].channel] >= _flightModes[i].min && _rcChannels[_flightModes[i].channel] <= _flightModes[i].max)
                {
                    _flightModeCallback((flightModeId_t)i);
                    break;
                }
            }
        }
    }
#endif
#endif

#if CRSF_TELEMETRY_ENABLED > 0
#if CRSF_TELEMETRY_ATTITUDE_ENABLED > 0
    void SerialReceiver::telemetryWriteAttitude(int16_t roll, int16_t pitch, int16_t yaw)
    {
        telemetry->setAttitudeData(roll, pitch, yaw);
    }
#endif

#if CRSF_TELEMETRY_BAROALTITUDE_ENABLED > 0
    void SerialReceiver::telemetryWriteBaroAltitude(uint16_t altitude, int16_t vario)
    {
        telemetry->setBaroAltitudeData(altitude, vario);
    }
#endif

#if CRSF_TELEMETRY_BATTERY_ENABLED > 0
    void SerialReceiver::telemetryWriteBattery(float voltage, float current, uint32_t fuel, uint8_t percent)
    {
        telemetry->setBatteryData(voltage, current, fuel, percent);
    }
#endif

#if CRSF_TELEMETRY_FLIGHTMODE_ENABLED > 0
    void SerialReceiver::telemetryWriteFlightMode(flightModeId_t flightModeId)
    {
        switch (flightModeId)
        {
            case FLIGHT_MODE_FAILSAFE:
                flightModeStr = "!FS!";
                break;
            case FLIGHT_MODE_GPS_RESCUE:
                flightModeStr = "RTH";
                break;
            case FLIGHT_MODE_PASSTHROUGH:
                flightModeStr = "MANU";
                break;
            case FLIGHT_MODE_ANGLE:
                flightModeStr = "STAB";
                break;
            case FLIGHT_MODE_HORIZON:
                flightModeStr = "HOR";
                break;
            case FLIGHT_MODE_AIRMODE:
                flightModeStr = "AIR";
                break;
            default:
                flightModeStr = "ACRO";
                break;
        }

        // Serial.println(flightModeStr);
        telemetry->setFlightModeData(flightModeStr, (bool)(flightModeId == FLIGHT_MODE_DISARMED ? true : false));
    }
#endif

#if CRSF_TELEMETRY_FLIGHTMODE_ENABLED > 0
    void SerialReceiver::telemetryWriteCustomFlightMode(const char *flightModeStr, bool armed = true)
    {
        telemetry->setFlightModeData(flightModeStr, armed);
    }
#endif

#if CRSF_TELEMETRY_GPS_ENABLED > 0
    void SerialReceiver::telemetryWriteGPS(float latitude, float longitude, float altitude, float speed, float groundCourse, uint8_t satellites)
    {
        telemetry->setGPSData(latitude, longitude, altitude, speed, groundCourse, satellites);
    }
#endif
#endif
} // namespace serialReceiverLayer

#include <algorithm>
#include <fstream>
#include <iostream>

#include "hlaudio/internal/Controller.h"
#include "hlaudio/internal/HulaAudioError.h"
#include "hlaudio/internal/HulaAudioSettings.h"

#if _WIN32
    #include "WindowsAudio.h"
#elif __unix__
    #include "LinuxAudio.h"
#elif __APPLE__
    #include "OSXAudio.h"
#endif

using namespace hula;

/**
 * Construct an instance of Controller class.
 * Acts as a bridge between the higher levels and OS level functions
 */
Controller::Controller()
{
    // Initialize OSAudio based on host OS
    #if defined(__unix__)
    audio = new LinuxAudio();
    #elif defined(__APPLE__)
    audio = new OSXAudio();
    #elif _WIN32
    audio = new WindowsAudio();
    #endif

    if (audio == nullptr)
    {
        hlDebug() << HL_OS_INIT_MSG << std::endl;
        throw AudioException(HL_OS_INIT_CODE, HL_OS_INIT_MSG);
    }

}

/**
 * Add an initialized buffer to the list of buffers that receive audio data.
 * As soon as the buffer is added, it should begin receiving data.
 *
 * If already present, the ring buffer will not be duplicated.
 *
 * This is a publicly exposed wrapper for the OSAudio method.
 *
 * @param rb HulaLoop ring buffer to add to the list
*/
void Controller::addBuffer(HulaRingBuffer *rb)
{
    audio->addBuffer(rb);
}

/**
 * @ingroup memory_management
 *
 * Allocate and initialize a HulaRingBuffer that can be added to
 * the OSAudio ring buffer list via Controller::addBuffer.
 *
 * @return Newly allocated ring buffer
 */
HulaRingBuffer *Controller::createBuffer(float duration)
{
    try
    {
        return new HulaRingBuffer(duration);
    }
    catch(const AudioException &ae)
    {
        throw;
    }
}

/**
 * @ingroup memory_management
 *
 * Allocate and initialize a HulaRingBuffer and automatically
 * add it to the OSAudio ring buffer list.
 */
HulaRingBuffer *Controller::createAndAddBuffer(float duration)
{
    HulaRingBuffer *rb = nullptr;
    try
    {
        rb = new HulaRingBuffer(duration);
    }
    catch(const AudioException &ae)
    {
        throw;
    }
    addBuffer(rb);
    return rb;
}

/**
 * @ingroup memory_management
 *
 * Remove a buffer from the list of buffers that receive audio data.
 * The removed buffer is not deleted and must be deleted by the user.
 *
 * This enables pausing retrieval when audio is not needed and
 * re-adding the same buffer when audio data is again needed.
 *
 * This is a publicly exposed wrapper for the OSAudio method.
 *
 * @param rb HulaLoop ring buffer to remove from the list.
*/
void Controller::removeBuffer(HulaRingBuffer *rb)
{
    audio->removeBuffer(rb);
}

/**
 * @ingroup memory_management
 *
 * Fetch a list of devices for the given DeviceType.
 *
 * For requests of more than one type, bitwise OR the types together
 * and cast back to a DeviceType.
 * \code
 * getDevices((DeviceType)(DeviceType::RECORD | DeviceType::LOOPBACK))
 * \endcode
 *
 * @param type DeviceType that is combination from the DeviceType enum
 *
 * @return std::vector<Device*> A list of Device instances that carry the necessary device information
 */
std::vector<Device *> Controller::getDevices(DeviceType type) const
{
    try
    {
        return audio->getDevices(type);
    }
    catch(const AudioException &ae)
    {
        throw;
    }
}

/**
 * @ingroup memory_management
 *
 * Set the device from which audio should be captured.
 *
 * This method will make a copy of the passed Device
 * so that subsequent (and necessary) calls to Device::deleteDevices()
 * can be used without issue.
 *
 * @param device Desired input device
 * @return Success of device switch
 */
bool Controller::setActiveInputDevice(Device *device) const
{
    try
    {
        return audio->setActiveInputDevice(device);
    }
    catch(const AudioException &ae)
    {
        throw;
    }
}

/**
 * @ingroup memory_management
 *
 * Set the device to which audio should be played back.
 *
 * This method will make a copy of the passed Device
 * so that subsequent (and necessary) calls to Device::deleteDevices()
 * can be used without issue.
 *
 * @param device - Device instance that is to be set as the active output device
 * @return Success of device switch
 */
bool Controller::setActiveOutputDevice(Device *device) const
{
    return audio->setActiveOutputDevice(device);
}

/**
 * Deconstructs the current instance of the Controller class
 */
Controller::~Controller()
{
    hlDebug() << "Controller destructor called" << std::endl;

    // Don't do this until mem management is fixed
    if (audio)
    {
        delete audio;
    }
}

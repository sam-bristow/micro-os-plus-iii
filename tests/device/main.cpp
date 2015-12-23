/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2015 Liviu Ionescu.
 *
 * µOS++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * µOS++ is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "posix-io/FileDescriptorsManager.h"
#include "posix-io/IO.h"
#include "posix-io/CharDevice.h"
#include "posix-io/CharDevicesRegistry.h"
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#if defined(OS_INCLUDE_TRACE_PRINTF)
#include "diag/Trace.h"
#endif

#if defined(__ARM_EABI__)
#include "posix-io/redefinitions.h"
#endif

// ----------------------------------------------------------------------------

// Test class, all methods return ENOSYS, as not implemented, except open().

class TestDevice : public os::posix::CharDevice
{
public:

  TestDevice (const char* deviceName, uint32_t deviceNumber);

  virtual int
  do_open (const char* path, int oflag, va_list args);

  int
  getMode (void);

private:

  uint32_t fDeviceNumber;
  uint32_t fMode;

};

TestDevice::TestDevice (const char* deviceName, uint32_t deviceNumber) :
    CharDevice (deviceName)
{
  fDeviceNumber = deviceNumber;
  fMode = 0;
}

int
TestDevice::getMode (void)
{
  return fMode;
}

#if defined ( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

int
TestDevice::do_open (const char* path, int oflag, va_list args)
{
  fMode = va_arg(args, int);

  return 0;
}

#if defined ( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#if !defined(OS_BOOL_PREFIX_POSIX_SYSCALLS)
#define __posix_close close
#define __posix_open open
#endif

// ----------------------------------------------------------------------------

#define DESCRIPTORS_ARRAY_SIZE (5)
os::posix::FileDescriptorsManager descriptorsManager
  { DESCRIPTORS_ARRAY_SIZE };

#define DEVICES_ARRAY_SIZE (3)
os::posix::CharDevicesRegistry devicesRegistry
  { DEVICES_ARRAY_SIZE };

// This device will be mapped as "/dev/test"
TestDevice test
  { "test", 1 };

// ----------------------------------------------------------------------------

extern "C"
{
  int
  __posix_open (const char* path, int oflag, ...);

  int
  __posix_close (int fildes);
}

// ----------------------------------------------------------------------------

int
main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
  std::size_t sz = os::posix::CharDevicesRegistry::getSize ();
  assert(sz == DEVICES_ARRAY_SIZE);

  // Check if initial status is empty.
  for (std::size_t i = 0; i < sz; ++i)
    {
      assert(os::posix::CharDevicesRegistry::getDevice (i) == nullptr);
    }

  // Register device
  os::posix::CharDevicesRegistry::add (&test);

  // Check if first device is registered.
  assert(os::posix::CharDevicesRegistry::getDevice (0) == &test);

    {
      // Test C++ API

      os::posix::IO* io;
      io = os::posix::open ("/dev/test", 0, 123);
      assert((io != nullptr) && (errno == 0));

      int fd;
      fd = io->getFileDescriptor ();

      // Get it back; is it the same?
      assert(os::posix::FileDescriptorsManager::getIo (fd) == &test);

      // Check passing variadic mode.
      assert(test.getMode () == 123);

      // Close and free descriptor.
      int ret = io->close ();
      assert((ret == 0) && (errno == 0));

      // Check if descriptor freed.
      assert(os::posix::FileDescriptorsManager::getIo (fd) == nullptr);
      assert(test.getFileDescriptor () == os::posix::noFileDescriptor);
    }

    {
      // Test C API

      int fd = __posix_open ("/dev/test", 0, 234);
      assert((fd >= 3) && (errno == 0));

      // Get it back; is it the same?
      assert(os::posix::FileDescriptorsManager::getIo (fd) == &test);
      assert(test.getFileDescriptor () == fd);

      // Check passing variadic mode.
      assert(test.getMode () == 234);

      // Close and free descriptor
      int ret = __posix_close (fd);
      assert((ret == 0) && (errno == 0));

      // Check if descriptor freed.
      assert(os::posix::FileDescriptorsManager::getIo (fd) == nullptr);
      assert(test.getFileDescriptor () == os::posix::noFileDescriptor);
    }

  const char* msg = "'test-device-debug' succeeded.\n";
#if defined(OS_INCLUDE_TRACE_PRINTF)
  trace_puts (msg);
#else
  std::puts (msg);
#endif

  // Success!
  return 0;
}

// ----------------------------------------------------------------------------


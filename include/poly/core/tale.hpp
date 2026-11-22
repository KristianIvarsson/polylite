//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#define POLY_VERSION_MAJOR 6
#define POLY_VERSION_MINOR 0
#define POLY_VERSION_PATCH 2

#define POLY_PASTE( major, minor, patch) v##major##_##minor##_##patch
#define POLY_SPACE( major, minor, patch) POLY_PASTE( major, minor, patch)

#define poly_version poly::inline POLY_SPACE( POLY_VERSION_MAJOR, POLY_VERSION_MINOR, POLY_VERSION_PATCH)

namespace poly_version
{
   namespace core::inline tale
   {
      constexpr struct
      {
         int major = POLY_VERSION_MAJOR;
         int minor = POLY_VERSION_MINOR;
         int patch = POLY_VERSION_PATCH;
         int whole = major * 10000 + minor * 100 + patch;
      } version{};
   } // core::tale
} // poly_version

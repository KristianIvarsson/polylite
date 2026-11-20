//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include <limits>
#include <cstdint>
#include <concepts>

#if __has_include(<stdfloat>)
#include <stdfloat>
#endif

namespace poly_version
{
   namespace core::type
   {
      using i08 = std::int8_t;
      using i16 = std::int16_t;
      using i32 = std::int32_t;
      using i64 = std::int64_t;

      using u08 = std::uint8_t;
      using u16 = std::uint16_t;
      using u32 = std::uint32_t;
      using u64 = std::uint64_t;

#if defined(__STDCPP_FLOAT32_T__)
      using f32 = std::float32_t;
#else
      static_assert( std::numeric_limits< float>::is_iec559 && sizeof( float) == sizeof( u32));
      using f32 = float;
#endif

#if defined(__STDCPP_FLOAT64_T__)
      using f64 = std::float64_t;
#else
      static_assert( std::numeric_limits< double>::is_iec559 && sizeof( double) == sizeof( u64));
      using f64 = double;
#endif

      template< typename unit>
      concept byte = sizeof( unit) == 1;

      //! prevents copying (and moving) of types referring to their own members
      struct pinned
      {
         pinned() = default;
         pinned( const pinned&) = delete;
         pinned& operator=( const pinned&) = delete;
      };

      template< typename type, typename... types>
      constexpr bool some = ( std::same_as< type, types> || ...);
   } // core::type
} // poly_version

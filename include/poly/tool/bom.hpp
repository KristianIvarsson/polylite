//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "../core/tale.hpp"

#include <array>
#include <ranges>
#include <istream>
#include <algorithm>
#include <string_view>


namespace poly_version
{
   namespace tool::bom
   {
      namespace detail
      {
         constexpr std::string_view utf8 = "\xEF\xBB\xBF";
      } // detail

      //! skips possible UTF8-BOM
      //! @{
      inline auto skip( std::istream& value) -> std::istream&
      {
         std::array< char, 3> data{};

         const auto count  = value.read( data.data(), data.size()).gcount();

         if( ! std::ranges::equal( data, detail::utf8))
            value.clear(), value.seekg( 0 - count, std::ios::cur);

         return value;
      }

      inline auto skip( std::string_view value)
      {
         if( value.starts_with( detail::utf8))
            value.remove_prefix( detail::utf8.size());

         return value;
      }
      //! @}
   } // tool::bom
} // poly_version

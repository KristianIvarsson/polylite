//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include <variant>
#include <string>
#include <vector>

#include "fifo.hpp"

namespace poly
{
   inline namespace v1_0_0
   {
      struct node;

      using data = std::variant< std::nullptr_t, bool, long, double, std::string, std::vector< node>, fifo< std::string, node>>;

      struct node : data
      {
         using data::data;
         using data::operator=;

         //! @{ member
         using nothing = std::variant_alternative_t< 0, data>;
         using boolean = std::variant_alternative_t< 1, data>;
         using integer = std::variant_alternative_t< 2, data>;
         using decimal = std::variant_alternative_t< 3, data>;
         using string = std::variant_alternative_t< 4, data>;
         using array = std::variant_alternative_t< 5, data>;
         using table = std::variant_alternative_t< 6, data>;
         //! @}

         //! @{ access
         template< typename type> auto& as( this auto&& self) { return std::get< type>( self); }

         auto& as_nothing( this auto&& self) { return self.template as< nothing>(); }
         auto& as_boolean( this auto&& self) { return self.template as< boolean>(); }
         auto& as_integer( this auto&& self) { return self.template as< integer>(); }
         auto& as_decimal( this auto&& self) { return self.template as< decimal>(); }
         auto& as_string( this auto&& self) { return self.template as< string>(); }
         auto& as_array( this auto&& self) { return self.template as< array>(); }
         auto& as_table( this auto&& self) { return self.template as< table>(); }

         auto& at( this auto& self, const auto& lookup) requires std::is_convertible_v< decltype( lookup), array::size_type>
         {
           return self.as_array().at( lookup);
         }

         auto& at( this auto& self, const auto& lookup) requires std::is_convertible_v< decltype( lookup), table::key_type>
         {
           return self.as_table().at( lookup);
         }
         //! @}

         //! @{ lookup (and access)
         template< typename type> auto is( this auto&& self) noexcept { return std::get_if< type>( &self); }

         auto is_nothing( this auto&& self) noexcept { return self.template is< nothing>(); }
         auto is_boolean( this auto&& self) noexcept { return self.template is< boolean>(); }
         auto is_integer( this auto&& self) noexcept { return self.template is< integer>(); }
         auto is_decimal( this auto&& self) noexcept { return self.template is< decimal>(); }
         auto is_string( this auto&& self) noexcept { return self.template is< string>(); }
         auto is_array( this auto&& self) noexcept { return self.template is< array>(); }
         auto is_table( this auto&& self) noexcept { return self.template is< table>(); }

         bool is_null() const noexcept { return is_nothing(); }
         bool is_true() const noexcept { return is_boolean() && as_boolean(); }
         bool is_false() const noexcept { return is_boolean() && not as_boolean(); }
         bool is_object() const noexcept { return is_array() or is_table(); }
         bool is_scalar() const noexcept { return not is_object(); }
         bool is_numeric() const noexcept { return is_integer() or is_decimal(); }
         //! @}
      };

   } // v1_0_0

} // poly

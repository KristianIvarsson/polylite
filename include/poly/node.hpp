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
   inline namespace v1_1_0
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
         template< typename type> auto to( this auto&& self) noexcept { return std::get_if< type>( &self); }

         auto to_nothing( this auto&& self) noexcept { return self.template to< nothing>(); }
         auto to_boolean( this auto&& self) noexcept { return self.template to< boolean>(); }
         auto to_integer( this auto&& self) noexcept { return self.template to< integer>(); }
         auto to_decimal( this auto&& self) noexcept { return self.template to< decimal>(); }
         auto to_string( this auto&& self) noexcept { return self.template to< string>(); }
         auto to_array( this auto&& self) noexcept { return self.template to< array>(); }
         auto to_table( this auto&& self) noexcept { return self.template to< table>(); }

         bool is_null() const noexcept { return to_nothing(); }
         bool is_true() const noexcept { return to_boolean() && as_boolean(); }
         bool is_false() const noexcept { return to_boolean() && not as_boolean(); }
         bool is_scalar() const noexcept { return to_boolean() or is_numeric() or to_string(); }
         bool is_numeric() const noexcept { return to_integer() or to_decimal(); }
         //! @}

         //! @{ mutate (and access)
         auto& operator []( const auto& lookup) requires std::is_convertible_v< decltype( lookup), array::size_type>
         {
            if( not to_array())
               *this = node::array{};
            
            while( as_array().size() <= lookup)
               as_array().emplace_back();

            return as_array()[ lookup];
         }

         auto& operator []( const auto& lookup) requires std::is_convertible_v< decltype( lookup), table::key_type>
         {
            if( not to_table())
               *this = node::table{};

            return as_table()[ lookup];
         }
         //! @}
      };

   } // v1_1_0

} // poly

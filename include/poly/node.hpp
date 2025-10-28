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

#include <cassert>

namespace poly
{
   inline namespace v1_2_0
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
         using object = std::variant_alternative_t< 6, data>;
         //! @}

         //! @{ access
         template< typename type> constexpr auto& as( this auto&& self) { return std::get< type>( self); }

         constexpr auto& as_nothing( this auto&& self) { return self.template as< nothing>(); }
         constexpr auto& as_boolean( this auto&& self) { return self.template as< boolean>(); }
         constexpr auto& as_integer( this auto&& self) { return self.template as< integer>(); }
         constexpr auto& as_decimal( this auto&& self) { return self.template as< decimal>(); }
         constexpr auto& as_string( this auto&& self) { return self.template as< string>(); }
         constexpr auto& as_array( this auto&& self) { return self.template as< array>(); }
         constexpr auto& as_table( this auto&& self) { return self.template as< table>(); }
         constexpr auto& as_object( this auto&& self) { return self.template as< object>(); }

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
         template< typename type> constexpr auto to( this auto&& self) noexcept { return std::get_if< type>( &self); }

         constexpr auto to_nothing( this auto&& self) noexcept { return self.template to< nothing>(); }
         constexpr auto to_boolean( this auto&& self) noexcept { return self.template to< boolean>(); }
         constexpr auto to_integer( this auto&& self) noexcept { return self.template to< integer>(); }
         constexpr auto to_decimal( this auto&& self) noexcept { return self.template to< decimal>(); }
         constexpr auto to_string( this auto&& self) noexcept { return self.template to< string>(); }
         constexpr auto to_array( this auto&& self) noexcept { return self.template to< array>(); }
         constexpr auto to_table( this auto&& self) noexcept { return self.template to< table>(); }
         constexpr auto to_object( this auto&& self) noexcept { return self.template to< object>(); }

         template< typename type>
         struct proxy
         {
            explicit operator bool () const noexcept { return pointer; }
            auto operator ->() const noexcept { assert( pointer); return pointer; }
            auto& operator *() const noexcept { assert( pointer); return *pointer; }

            auto operator ()( const auto& lookup) const noexcept requires std::is_convertible_v< decltype( lookup), array::size_type>
            {
               if( pointer and pointer->to_array() and lookup < pointer->as_array().size())
                  return proxy{ &pointer->as_array().at( lookup)};

               return proxy{ nullptr};
            }

            auto operator ()( const auto& lookup) const noexcept requires std::is_convertible_v< decltype( lookup), table::key_type>
            {
               if( pointer and pointer->to_table() and pointer->as_table().contains( lookup))
                  return proxy{ &pointer->as_table().at( lookup)};

               return proxy{ nullptr};
            }

         private:

            friend struct node;

            explicit proxy( type pointer) : pointer{ pointer} {}

            type pointer = nullptr;
         };

         auto operator ()( const auto& lookup) const & noexcept
         {
            return proxy{ this}( lookup);
         }

         auto operator ()() const & noexcept
         {
            return proxy{ this};
         }
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

         //! @{ lookup
         constexpr bool is_null() const noexcept { return to_nothing(); }
         constexpr bool is_true() const noexcept { return to_boolean() && as_boolean(); }
         constexpr bool is_false() const noexcept { return to_boolean() && not as_boolean(); }
         constexpr bool is_scalar() const noexcept { return to_boolean() or is_numeric() or to_string(); }
         constexpr bool is_numeric() const noexcept { return to_integer() or to_decimal(); }
         //! @]}
      };

   } // v1_2_0

} // poly

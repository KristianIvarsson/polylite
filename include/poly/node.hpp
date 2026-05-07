//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "fifo.hpp"

#include <chrono>
#include <string>
#include <vector>
#include <cstddef>
#include <variant>

#include <cassert>

namespace poly
{
   inline namespace version
   {
      struct node;

      using data = std::variant< 
         std::nullptr_t, 
         bool, 
         long, 
         double, 
         std::variant
         < 
            std::chrono::local_time< std::chrono::days>, 
            std::chrono::hh_mm_ss< std::chrono::system_clock::duration>, 
            std::chrono::local_time< std::chrono::system_clock::duration>, 
            std::chrono::zoned_time< std::chrono::system_clock::duration>
         >,
         std::string, 
         std::vector< std::byte>, 
         std::vector< node>, 
         fifo< std::string, node>>;

      struct node : data
      {
         using data::data;
         using data::operator=;

         //! @{ member
         using nothing = std::variant_alternative_t< 0, data>;
         using boolean = std::variant_alternative_t< 1, data>;
         using integer = std::variant_alternative_t< 2, data>;
         using decimal = std::variant_alternative_t< 3, data>;
         using instant = std::variant_alternative_t< 4, data>;
         using string = std::variant_alternative_t< 5, data>;
         using binary = std::variant_alternative_t< 6, data>;
         using array = std::variant_alternative_t< 7, data>;
         using object = std::variant_alternative_t< 8, data>;

         using local_date = std::variant_alternative_t< 0, instant>;
         using local_time = std::variant_alternative_t< 1, instant>;
         using local_datetime = std::variant_alternative_t< 2, instant>;
         using zoned_datetime = std::variant_alternative_t< 3, instant>;
         //! @}

         //! @{ access
         constexpr auto& as_nothing( this auto&& self) { return std::get< nothing>( self); }
         constexpr auto& as_boolean( this auto&& self) { return std::get< boolean>( self); }
         constexpr auto& as_integer( this auto&& self) { return std::get< integer>( self); }
         constexpr auto& as_decimal( this auto&& self) { return std::get< decimal>( self); }
         constexpr auto& as_instant( this auto&& self) { return std::get< instant>( self); }
         constexpr auto& as_string( this auto&& self) { return std::get< string>( self); }
         constexpr auto& as_binary( this auto&& self) { return std::get< binary>( self); }
         constexpr auto& as_array( this auto&& self) { return std::get< array>( self); }
         constexpr auto& as_object( this auto&& self) { return std::get< object>( self); }

         constexpr auto& as_local_date( this auto&& self) { return std::get< local_date>( self.as_instant()); }
         constexpr auto& as_local_time( this auto&& self) { return std::get< local_time>( self.as_instant()); }
         constexpr auto& as_local_datetime( this auto&& self) { return std::get< local_datetime>( self.as_instant()); }
         constexpr auto& as_zoned_datetime( this auto&& self) { return std::get< zoned_datetime>( self.as_instant()); }

         auto& at( this auto& self, const auto& lookup) requires std::is_convertible_v< decltype( lookup), array::size_type>
         {
            return self.as_array().at( lookup);
         }

         auto& at( this auto& self, const auto& lookup) requires std::is_convertible_v< decltype( lookup), object::key_type>
         {
            return self.as_object().at( lookup);
         }
         //! @}

         //! @{ lookup (and access)
         constexpr auto to_nothing( this auto&& self) noexcept { return std::get_if< nothing>( &self); }
         constexpr auto to_boolean( this auto&& self) noexcept { return std::get_if< boolean>( &self); }
         constexpr auto to_integer( this auto&& self) noexcept { return std::get_if< integer>( &self); }
         constexpr auto to_decimal( this auto&& self) noexcept { return std::get_if< decimal>( &self); }
         constexpr auto to_instant( this auto&& self) noexcept { return std::get_if< instant>( &self); }
         constexpr auto to_string( this auto&& self) noexcept { return std::get_if< string>( &self); }
         constexpr auto to_binary( this auto&& self) noexcept { return std::get_if< binary>( &self); }
         constexpr auto to_array( this auto&& self) noexcept { return std::get_if< array>( &self); }
         constexpr auto to_object( this auto&& self) noexcept { return std::get_if< object>( &self); }

         constexpr auto to_local_date( this auto&& self) { return std::get_if< local_date>( self.to_instant()); }
         constexpr auto to_local_time( this auto&& self) { return std::get_if< local_time>( self.to_instant()); }
         constexpr auto to_local_datetime( this auto&& self) { return std::get_if< local_datetime>( self.to_instant()); }
         constexpr auto to_zoned_datetime( this auto&& self) { return std::get_if< zoned_datetime>( self.to_instant()); }

         template< typename type>
         struct proxy
         {
            explicit operator bool () const noexcept { return pointer; }
            auto operator ->() const noexcept { assert( pointer); return pointer; }
            auto& operator *() const noexcept { assert( pointer); return *pointer; }

            auto operator ()( const auto& lookup) const noexcept requires std::is_convertible_v< decltype( lookup), array::size_type>
            {
               if( pointer and pointer->to_array() and std::cmp_less( lookup, pointer->as_array().size()))
                  return proxy{ &pointer->as_array().at( lookup)};

               return proxy{ nullptr};
            }

            auto operator ()( const auto& lookup) const noexcept requires std::is_convertible_v< decltype( lookup), object::key_type>
            {
               if( pointer and pointer->to_object() and pointer->as_object().contains( lookup))
                  return proxy{ &pointer->as_object().at( lookup)};

               return proxy{ nullptr};
            }

         private:

            friend struct node;

            explicit proxy( type pointer) : pointer{ pointer} {}

            type pointer = nullptr;
         };

         auto operator ()( this auto& self, const auto& lookup) noexcept
         {
            return proxy{ &self}( lookup);
         }

         auto operator ()( this auto& self) noexcept
         {
            return proxy{ &self};
         }
         //! @}

         //! @{ mutate (and access)
         auto& operator []( const auto& lookup) requires std::is_convertible_v< decltype( lookup), array::size_type>
         {
            if( not to_array())
               *this = node::array{};
            
            while( std::cmp_less_equal( as_array().size(), lookup))
               as_array().emplace_back();

            return as_array()[ lookup];
         }

         auto& operator []( const auto& lookup) requires std::is_convertible_v< decltype( lookup), object::key_type>
         {
            if( not to_object())
               *this = node::object{};

            return as_object()[ lookup];
         }
         //! @}

         //! @{ lookup
         constexpr bool is_nothing() const noexcept { return to_nothing(); }
         constexpr bool is_boolean() const noexcept { return to_boolean(); }
         constexpr bool is_integer() const noexcept { return to_integer(); }
         constexpr bool is_decimal() const noexcept { return to_decimal(); }
         constexpr bool is_instant() const noexcept { return to_instant(); }
         constexpr bool is_string() const noexcept { return to_string(); }
         constexpr bool is_binary() const noexcept { return to_binary(); }
         constexpr bool is_array() const noexcept { return to_array(); }
         constexpr bool is_object() const noexcept { return to_object(); }

         constexpr bool is_local_time() const noexcept { return to_local_time(); }
         constexpr bool is_local_date() const noexcept { return to_local_date(); }
         constexpr bool is_local_datetime() const noexcept { return to_local_datetime(); }
         constexpr bool is_zoned_datetime() const noexcept { return to_zoned_datetime(); }

         constexpr bool is_null() const noexcept { return is_nothing(); }
         constexpr bool is_true() const noexcept { return is_boolean() and as_boolean(); }
         constexpr bool is_false() const noexcept { return is_boolean() and not as_boolean(); }
         constexpr bool is_scalar() const noexcept { return is_boolean() or is_numeric() or is_instant() or is_string() or is_binary(); }
         constexpr bool is_numeric() const noexcept { return is_integer() or is_decimal(); }
         constexpr bool is_trivial() const noexcept { return is_null() or is_scalar(); }
         //! @}
      };

   } // version

} // poly

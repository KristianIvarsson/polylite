//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "core/tale.hpp"

#include <tuple>
#include <format>
#include <ranges>
#include <vector>
#include <utility>
#include <concepts>
#include <iterator>
#include <algorithm>
#include <stdexcept>

namespace poly_version
{
   template< typename Key, typename T>
   struct fifo : std::vector< std::pair< Key, T>>
   {
      using base_type = std::vector< std::pair< Key, T>>;

      using base_type::base_type;

      using value_type = base_type::value_type;
      using key_type = Key;
      using mapped_type = T;
      using size_type = base_type::size_type;

      using base_type::erase;


      //! @{ plain-map-interface
      auto find( this auto&& self, const auto& key)
      {
         return std::ranges::find( self, key, &value_type::first);
      }

      bool contains( const auto& key) const
      {
         return find( key) != base_type::end();
      }

      auto& at( this auto&& self, const auto& key)
      {
         if( auto result = self.find( key); result != self.end())
            return result->second;
         throw std::out_of_range{ std::format( "{} '{}'", __func__, key)};
      }

      auto& operator []( auto&& key)
      {
         if( auto result = find( key); result != base_type::end())
            return result->second;
         return base_type::emplace_back( std::forward< decltype( key)>( key), mapped_type{}).second;
      }

      auto try_emplace( auto&& key, auto&&... parameters)
      {
         if( auto result = find( key); result != base_type::end())
            return std::make_pair( result, false);
         return base_type::emplace_back( std::piecewise_construct, std::forward_as_tuple( std::forward< decltype( key)>( key)), std::forward_as_tuple( std::forward< decltype( parameters)>( parameters)...)), std::make_pair( std::prev( base_type::end()), true);
      }
      //! @}

      //! @{ multi-map-interface
      auto count( const auto& key) const
      {
         return std::ranges::count( *this, key, &value_type::first);
      }

      auto emplace( auto&&... parameters)
      {
         return base_type::emplace_back( std::forward< decltype( parameters)>( parameters)...), std::prev( base_type::end());
      }

      auto equal_range( this auto&& self, const auto& key)
      {
         using iterator = decltype( std::ranges::begin( self));
         return std::ranges::subrange{ twin< iterator>{ self.find( key), std::ranges::end( self)}, std::default_sentinel};
      }

      auto erase( const auto& key)
         requires( ! std::convertible_to< decltype( key), typename base_type::const_iterator>)
      {
         return std::erase_if( *this, [ &]( const auto& item){ return item.first == key; });
      }

      auto erase( auto lead, std::default_sentinel_t)
      {
         if( lead == std::default_sentinel)
            return base_type::end();

         const auto last = std::ranges::find_last( lead.base(), base_type::end(), lead->first, &value_type::first).begin();
         auto [ head, tail] = std::ranges::remove( lead.base(), last, last->first, &value_type::first);
         return base_type::erase( head, std::next( last));
      }
      //! @}

   private:

      template< typename iterator>
      struct twin
      {
         using value_type = fifo::value_type;
         using difference_type = std::iter_difference_t< iterator>;

         twin() = default;
         twin( iterator mark, iterator last) : mark{ mark}, last{ last} {}

         auto base() const { return mark; }

         decltype( auto) operator *() const { return *mark; }
         auto operator ->() const { return mark; }

         auto& operator ++() { mark = std::ranges::find( std::next( mark), last, mark->first, &value_type::first); return *this; }
         auto operator ++( int) { auto copy = *this; ++*this; return copy; }

         bool operator ==( const twin&) const = default;
         bool operator ==( std::default_sentinel_t) const { return mark == last; }

      private:

         iterator mark{};
         iterator last{};
      };   

   private:
   
#if __cpp_lib_containers_ranges >= 202202L
      using base_type::append_range;
      using base_type::assign_range;
      using base_type::insert_range;
#endif
      using base_type::assign;
      using base_type::insert;
      using base_type::resize;
      using base_type::push_back;
      using base_type::emplace_back;

   };

} // poly_version

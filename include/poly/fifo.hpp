//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include <vector>
#include <format>
#include <utility>
#include <stdexcept>

namespace poly
{
   inline namespace version
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

         auto find( this auto&& self, const auto& key)
         {
            return std::ranges::find( self, key, &value_type::first);
         }

         auto contains( const auto& key) const
         {
            return find( key) != base_type::end();
         }

         auto erase( const auto& key)
         {
            if( auto result = find( key); result != base_type::end())
               return base_type::erase( result), size_type{ 1};
            else
               return size_type{ 0};
         }

         auto& at( this auto&& self, const auto& key)
         {
            if(auto result = self.find( key); result != self.end())
               return result->second;
            else
               throw std::out_of_range{ std::format( "{} '{}'", __func__, key)};
         }

         auto& operator []( auto&& key)
         {
            if(auto result = find( key); result != base_type::end())
               return result->second;
            else
               return base_type::emplace_back( std::forward< decltype( key)>( key), mapped_type{}).second;
         }

         auto insert( auto&& value)
         {
            if(auto result = find( value.first); result != base_type::end())
               return std::make_pair( result, false);
            else
               return base_type::emplace_back( std::move( value)), std::make_pair( std::prev( base_type::end()), true);
         }

         auto insert_or_assign( auto&& key, auto&& object)
         {
            if(auto result = find( key); result != base_type::end())
               return result->second = std::move( object), std::make_pair( result, false);
            else
               return base_type::emplace_back( std::move( key), std::move( object)), std::make_pair( std::prev( base_type::end()), true);
         }

         auto emplace( auto&&... parameters)
         {
            if(auto result = find( std::get< 0>( std::tie( parameters...))); result != base_type::end())
               return std::make_pair( result, false);
            else
               return base_type::emplace_back( std::forward< decltype( parameters)>( parameters)...), std::make_pair( std::prev( base_type::end()), true);
         }

      private:
      
         //using base_type::append_range;
         //using base_type::assign_range;
         //using base_type::insert_range;

         using base_type::assign;
         using base_type::insert;
         using base_type::emplace;
         using base_type::emplace_back;
         using base_type::push_back;
         using base_type::erase;
         using base_type::resize;

      };

   } // version

} // poly

//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "../core/tale.hpp"
#include "../node.hpp"

#include <algorithm>

namespace poly_version
{
   namespace tool::same
   {
      template< typename... parameters>
      bool test( const fifo< parameters...>& data)
      {
         for( auto item = data.begin(); item != data.end(); ++item)
            if( std::ranges::find( std::next( item), data.end(), item->first, &fifo< parameters...>::value_type::first) != data.end())
               return false;
         return true;
      }

      //! test whether the node tree have only unique keys
      inline bool test( const node& data)
      {
         if( auto array = data.to_array())
            return std::ranges::all_of( *array, []( const auto& item){ return test( item);} );

         if( auto object = data.to_object())
            return test( *object) && std::ranges::all_of( *object, []( const auto& item){ return test( item.second); });

         return true;
      }

      //! make sure the node tree have only unique keys
      inline node& cull( node& data)
      {
         if( auto array = data.to_array())
            for( auto& item : *array)
               cull( item);

         if( auto object = data.to_object())
            for( auto index = 0uz; index < object->size(); ++index)
            {
               if( auto [ head, tail] = object->equal_range( object->begin()[ index].first); head != tail)
                  object->erase( std::next( head), tail);
               cull( object->begin()[ index].second);
            }

         return data;
      }

      inline node cull( node&& data)
      {
         return std::move( cull( data));
      }
   } // tool::same
} // poly_version

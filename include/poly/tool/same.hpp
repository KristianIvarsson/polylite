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
      //! test whether the node tree have only unique keys
      inline bool test( const node& data)
      {
         if( auto array = data.to_array())
            return std::ranges::all_of( *array, test);

         if( auto object = data.to_object())
            for( auto item = object->begin(); item != object->end(); ++item)
               if( std::ranges::find( std::next( item), object->end(), item->first, &node::object::value_type::first) != object->end() || ! test( item->second))
                  return false;

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

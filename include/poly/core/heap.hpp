//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include <vector>
#include <utility>
#include <iterator>

namespace poly_version
{
   namespace core
   {
      template< typename into>
      class heap
      {
      private:

         std::vector< typename into::value_type> data;

      public:

         //! the current base, to seal from later
         [[nodiscard]] auto base() const
         {
            return data.size();
         }

         //! pushes item
         void push( auto&&... parameters)
         {
            data.emplace_back( std::forward< decltype( parameters)>( parameters)...);
         }

         //! moves everything since from into an into of exact size
         [[nodiscard]] auto seal( const typename into::size_type from) -> into
         {
            into nrv( std::make_move_iterator( data.begin() + from), std::make_move_iterator( data.end()));
            data.erase( data.begin() + from, data.end());
            return nrv;
         }
      };
   } // core
} // poly_version

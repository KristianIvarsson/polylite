//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "../node.hpp"
#include "../tool/same.hpp"

#include <stdexcept>

namespace poly_version
{
   namespace core::lint
   {
      struct rule
      {
         template< typename type>
         static constexpr bool block = false;
         static constexpr bool multi = false;
         static constexpr bool table = false;
         static constexpr auto valid = [](const auto&) { return true; };
      };

      namespace detail
      {
         [[noreturn]] inline void halt( )
         {
            throw std::invalid_argument{ "unsupported type"};
         }

         template< typename rules>
         struct visit
         {
            void operator() ( const node::object& data) const
            {
               if constexpr( ! rules::multi)
                  if( ! tool::same::test( data))
                     throw std::invalid_argument{ "duplicated key"};
               
               for( const auto& [ name, item] : data)
                  if( rules::valid( name))
                     std::visit( *this, item);
                  else
                     throw std::invalid_argument{ "invalid key"};

            }

            void operator() ( const node::array& data) const
            {
               if constexpr( rules::template block< node::array>)
                  halt();
               else
                  for( const auto& item : data)
                     std::visit( *this, item);
            }

            void operator() ( const node::instant& data) const
            {
               if constexpr( rules::template block< node::instant>)
                  halt();
               else
                  std::visit( *this, data);
            }

            template< typename type>
            void operator() ( const type&) const
            {
               if constexpr( rules::template block< type>)
                  halt();
            }
         };
      } // detail

      template< std::derived_from< rule> plan>
      const node& test( const node& data)
      {
         if constexpr( plan::table)
            if( ! data.is_object())
               throw std::invalid_argument{ "not an object"};

         std::visit( detail::visit< plan>{}, data);
         return data;
      }

      template< std::derived_from< rule> plan>
      node test( node&& data)
      {
         test< plan>( data);
         return std::move( data);
      }
   } // core::lint
} // poly_version

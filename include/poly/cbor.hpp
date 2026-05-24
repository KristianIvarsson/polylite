//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <limits>
#include <vector>
#include <cstddef>


namespace poly_version
{
   namespace cbor
   {
      namespace detail
      {
         enum class major : std::uint8_t
         {
            positive = 0,  // unsigned integer
            negative = 1,  // negative integer
            bytes    = 2,  // byte string
            text     = 3,  // text string
            array    = 4,  // array
            map      = 5,  // map
            tag      = 6,  // semantic tag
            simple   = 7,  // float / simple / break / floating point
         };

         enum class simple : std::uint8_t
         {
            no       = 20, // false
            yes      = 21, // true
            null     = 22, // null
            none     = 23,
            byte     = 24, // uint8
            half     = 25, // uint16
            real     = 26, // uint32
            full     = 27, // uint64
            stop     = 31, // break
         };

         enum class chrono : std::uint64_t
         {
            when = 0,      // standard date/time string  (RFC 8949)
            tick = 1,      // epoch-based date/time      (RFC 8949)
            days = 100,    // days since 1970-01-01      (RFC 8943)
            date = 1004,   // RFC 3339 full-date string  (RFC 8943)
         };         


      } // detail

      namespace detail
      {
         inline constexpr auto head( major kind, simple info) -> std::uint8_t
         {
            constexpr std::uint8_t mask{ 0x1F};
            constexpr std::uint8_t bits{ 5};
            return ( std::to_underlying( kind) << bits) | ( std::to_underlying( info) & mask);
         }

         template< help::sign type, help::target_iterator< type> iterator, bool packed, bool strict>
         struct writer : help::target< type, iterator>
         {
            using base = help::target< type, iterator>;
            using base::halt;
            using base::emit;
            using base::push;

            void operator() ( const node::nothing&)
            {
               push( head( major::simple, simple::null));
            }

            void operator() ( const node::boolean& node)
            {
               push( head( major::simple, node ? simple::yes : simple::no));
            }

            void operator() ( const node::integer& node)
            {
               if( node < 0)
                  pack( major::negative, ~static_cast< std::uint64_t>( node));
               else
                  pack( major::positive,  static_cast< std::uint64_t>( node));
            }

            void operator() ( const node::decimal& node)
            {
               full( major::simple, std::bit_cast< std::uint64_t>( node));
            }

            void operator() ( const node::instant& node)
            {
               std::visit( *this, node);
            }

            void operator() ( const node::string& node)
            {
               pack( major::text, node.size());
               emit( node);
            }

            void operator() ( const node::binary& node)
            {
               pack( major::bytes, node.size());
               emit( node);
            }

            void operator() ( const node::object& node)
            {
               pack( major::map, node.size());

               for( const auto& [ name, data] : node)
                  (*this)( name), std::visit( *this, data);
            }

            void operator() ( const node::array& node)
            {
               pack( major::array, node.size());

               for( const auto& data : node)
                  std::visit( *this, data);
            }

            void operator() ( const node::local_date& time)
            {
               if constexpr( packed)
               {
                  pack( major::tag, std::to_underlying( chrono::days));
                  (*this)( static_cast< node::integer>( time.time_since_epoch().count()));
               }
               else
               {
                  pack( major::tag, std::to_underlying( chrono::date));
                  text( time);
               }
            }

            void operator() ( const node::local_time& time)
            {
               if constexpr( strict) 
                  [[unlikely]] halt( "node::local_time");
               text( time);
            }

            void operator() ( const node::zoned_datetime& time)
            {
               if constexpr( packed)
               {
                  pack( major::tag, std::to_underlying( chrono::tick));
                  const auto elapsed = time.get_sys_time().time_since_epoch();
                  const auto rounded = std::chrono::floor< std::chrono::seconds>( elapsed);
                  if( elapsed == rounded)
                     (*this)( std::chrono::duration< node::integer>( rounded).count());
                  else
                     (*this)( std::chrono::duration< node::decimal>( elapsed).count());
               }
               else
               {
                  pack( major::tag, std::to_underlying( chrono::when));
                  text( time);
               }
            }

            void operator() ( const node::local_datetime& time)
            {
               if constexpr( strict) 
                  [[unlikely]] halt( "node::local_datetime");
               text( time);
            }


         private:

            void text( const auto& time)
            {
               std::string result;
               help::writer< char, std::back_insert_iterator< std::string>>{ std::back_inserter( result)}.time( time);
               (*this)( result);
            }

            void full( major kind, std::uint64_t data)
            {
               push( head( kind, simple::full));

               push( static_cast< std::uint8_t>( data >> 56));
               push( static_cast< std::uint8_t>( data >> 48));
               push( static_cast< std::uint8_t>( data >> 40));
               push( static_cast< std::uint8_t>( data >> 32));
               push( static_cast< std::uint8_t>( data >> 24));
               push( static_cast< std::uint8_t>( data >> 16));
               push( static_cast< std::uint8_t>( data >>  8));
               push( static_cast< std::uint8_t>( data));
            }

            void pack( major kind, std::uint64_t data)
            {
               constexpr auto edge = std::to_underlying( simple::byte);

               if( data < edge)
               {
                  push( head( kind, static_cast< simple>( data)));
               }
               else if( data <= std::numeric_limits< std::uint8_t>::max())
               {
                  push( head( kind, simple::byte));
                  push( static_cast< std::uint8_t>( data));
               }
               else if( data <= std::numeric_limits< std::uint16_t>::max())
               {
                  push( head( kind, simple::half));
                  push( static_cast< std::uint8_t>( data >> 8));
                  push( static_cast< std::uint8_t>( data));
               }
               else if( data <= std::numeric_limits< std::uint32_t>::max())
               {
                  push( head( kind, simple::real));
                  push( static_cast< std::uint8_t>( data >> 24));
                  push( static_cast< std::uint8_t>( data >> 16));
                  push( static_cast< std::uint8_t>( data >>  8));
                  push( static_cast< std::uint8_t>( data));
               }
               else
               {
                  full( kind, data);
               }
            }
         };

         template< bool packed, bool strict>
         auto write( const node& root, auto&& target)
         {
            std::visit( help::make::target< writer, packed, strict>( target), root);
         }

         // the default write function
         template< bool packed, bool strict>
         auto write( const node& root)
         {
            std::vector< std::byte> target;
            write< packed, strict>( root, target);
            return target;
         }

      } // detail

      inline namespace compact
      {
         inline namespace strict
         {
            auto write( auto&&... parameters)
            {
               return detail::write< true, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // strict

         namespace gentle
         {
            auto write( auto&&... parameters)
            {
               return detail::write< true, false>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // gentle
      } // verbose

      namespace verbose
      {
         inline namespace strict
         {
            auto write( auto&&... parameters)
            {
               return detail::write< false, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // strict

         namespace gentle
         {
            auto write( auto&&... parameters)
            {
               return detail::write< false, false>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // gentle
      } // compact

   } // cbor
} // poly_version
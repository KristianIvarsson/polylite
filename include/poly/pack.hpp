//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include <bit>
#include <cmath>
#include <limits>
#include <vector>
#include <cstddef>
#include <cstdint> 
#include <utility>
#include <algorithm>

#include "help.hpp"
#include "node.hpp"

namespace poly_version
{
   namespace pack
   {
      namespace detail
      {
         namespace extension
         {
            constexpr std::uint8_t timestamp{0xFF};
         } // extension


         enum class format : std::uint8_t
         {
            nil             = 0xC0,
            no              = 0xC2,
            yes             = 0xC3,

            bin8            = 0xC4,
            bin16           = 0xC5,
            bin32           = 0xC6,

            ext8            = 0xC7,
            ext16           = 0xC8,
            ext32           = 0xC9,

            float32         = 0xCA,
            float64         = 0xCB,

            uint8           = 0xCC,
            uint16          = 0xCD,
            uint32          = 0xCE,
            uint64          = 0xCF,

            sint8           = 0xD0,
            sint16          = 0xD1,
            sint32          = 0xD2,
            sint64          = 0xD3,

            fixext4         = 0xD6,
            fixext8         = 0xD7,

            str8            = 0xD9,
            str16           = 0xDA,
            str32           = 0xDB,

            array16         = 0xDC,
            array32         = 0xDD,

            map16           = 0xDE,
            map32           = 0xDF,

            // base values: the low bits hold the length or value
            fixmap          = 0x80,
            fixarray        = 0x90,
            fixstr          = 0xA0,
            negative_fixint = 0xE0,
         };

         template< help::sign type, help::target_iterator< type> iterator, bool strict>
         struct writer : help::target< type, iterator>
         {
            using base = help::target< type, iterator>;
            using base::emit;
            using base::halt;
            using base::push;

            void operator() ( const node::nothing)
            {
               push( format::nil);
            }

            void operator() ( const node::boolean node)
            {
               push( node ? format::yes : format::no);
            }

            void operator() ( const node::integer node)
            {
               if(node < 0)
                  negative( static_cast< std::uint64_t>( node));
               else
                  positive( static_cast< std::uint64_t>( node));
            }

            void operator() ( const node::decimal node)
            {
               using tiny = float;
               using huge = double;

               static_assert( sizeof( node::decimal) <= sizeof( huge));

               // a trick that finally will be float32/float64
               constexpr format float8{0xC8};

               if( std::fabs( node) > std::numeric_limits< tiny>::max() or static_cast< tiny>( node) != static_cast< huge>( node))
                  feed( float8, std::bit_cast< std::uint64_t>( static_cast< huge>( node)), sizeof( std::uint64_t));
               else
                  feed( float8, std::bit_cast< std::uint32_t>( static_cast< tiny>( node)), sizeof( std::uint32_t));
            }

            void operator() ( const node::instant& node)
            {
               std::visit( *this, node);
            }

            void operator() ( const node::string& node)
            {
               const auto size = node.size();
               const auto bits = std::bit_width( size);

               if( bits > 32) [[unlikely]] halt( "string length");

               if( bits > 5)
                  feed( format::str8, size, need( bits));
               else
                  push( static_cast< std::uint8_t>( std::to_underlying( format::fixstr) | size));

               emit( node);
            }

            void operator() ( const node::binary& node)
            {
               const auto size = node.size();
               const auto bits = std::bit_width( size);

               if( bits > 32) [[unlikely]] halt( "binary length");

               feed( format::bin8, size, need( bits));

               emit( node);
            }

            void operator() ( const node::object& node)
            {
               const auto size = node.size();
               const auto bits = std::bit_width( size);

               if( bits > 32) [[unlikely]] halt( "object length");

               // a trick that finally will be map16/map32
               constexpr format map8{0xDD};

               if( bits > 4)
                  feed( map8, size, need< 2>( bits));
               else
                  push( static_cast< std::uint8_t>( std::to_underlying( format::fixmap) | size));

               for( const auto& [name, data] : node)
               {
                  (*this)( name);
                  std::visit( *this, data);
               }
            }

            void operator() ( const node::array& node)
            {
               const auto size = node.size();
               const auto bits = std::bit_width( size);

               if( bits > 32) [[unlikely]] halt( "array length");

               // a trick that finally will be array16/array32
               constexpr format array8{0xDB};

               if( bits > 4)
                  feed( array8, size, need< 2>( bits));
               else
                  push( static_cast< std::uint8_t>( std::to_underlying( format::fixarray) | size));

               for( const auto& item : node)
                  std::visit( *this, item);
            }

            void operator() ( const node::local_date data)
            {
               if constexpr( strict) [[unlikely]] halt( "node::local_date");
               time( data);
            }

            void operator() ( const node::local_time data)
            {
               if constexpr( strict) [[unlikely]] halt( "node::local_time");
               time( data);
            }

            void operator() ( const node::local_datetime data)
            {
               if constexpr( strict) [[unlikely]] halt( "node::local_datetime");
               time( data);
            }
         
            void operator() ( const node::zoned_datetime data)
            {
               const auto elapsed = data.get_sys_time().time_since_epoch();
               const auto rounded = std::chrono::floor< std::chrono::seconds>( elapsed);
               const auto details = static_cast< std::uint64_t>( std::chrono::duration_cast< std::chrono::nanoseconds>( elapsed - rounded).count());
               const auto seconds = static_cast< std::uint64_t>( rounded.count());

               const auto stuffed = ( details << 34) | seconds;

               if( std::bit_width( stuffed) <= 32)
               {
                  //
                  // Timestamp32: nanoseconds is zero, seconds is non-negative and fits in 32 bits

                  push( format::fixext4);
                  push( extension::timestamp);
                  feed( seconds, 4);
               }
               else 
               if( std::bit_width( seconds) <= 34)
               {
                  //
                  // Timestamp64: nanoseconds always fits in 30 bits, seconds is non-negative and fits in 34 bits

                  push( format::fixext8);
                  push( extension::timestamp);
                  feed( stuffed, 8);
               }
               else
               {
                  //
                  // Timestamp96: 4-byte nanoseconds, then 8-byte seconds as two's complement

                  push( format::ext8);
                  push( std::uint8_t{12});
                  push( extension::timestamp);
                  feed( details, 4);
                  feed( seconds, 8);
               }
            }

         private:

            void time( const auto& data)
            {
               std::string string;
               help::writer< char, std::back_insert_iterator< std::string>>{ std::back_inserter( string)}.time( data);
               (*this)( string);
            }

            //! @return the smallest size (1, 2, 4 or 8 bytes) for a number of significant bits
            template< std::size_t min = 1, std::size_t max = 8>
            static constexpr auto need( const auto bits) noexcept
            {
               return std::clamp( std::bit_ceil( static_cast< std::size_t>( ( bits + 7) / 8)), min, max);
            }

            void feed( const std::uint64_t data, std::size_t size)
            {
               while( size--) push( static_cast< std::uint8_t>( data >> ( size * 8)));
            }            

            //! @param root is the base tag
            void feed( const format root, const std::uint64_t data, const std::size_t size)
            {
               push( static_cast< std::uint8_t>( std::to_underlying( root) + std::countr_zero( size)));
               feed( data, size);
            }            

            void negative( const std::uint64_t data)
            {
               const auto bits = std::bit_width( ~data) + 1;
               if( bits > 6)
                  feed( format::sint8, data, need( bits));
               else
                  push( static_cast< std::uint8_t>( data));
            }

            void positive( const std::uint64_t data)
            {
               const auto bits = std::bit_width( data);
               if( bits > 7)
                  feed( format::uint8, data, need( bits));
               else
                  push( static_cast< std::uint8_t>( data));
            }
         };

         template< bool strict>
         auto write( const node& root, auto&& target)
         {
            std::visit( help::make::target< writer, strict>( target), root);
         }

         // the default write function
         template< bool strict>
         auto write( const node& root)
         {
            std::vector< std::byte> target;
            write< strict>( root, target);
            return target;
         }

      } // detail

      inline namespace strict
      {
         auto write( auto&&... parameters)
         {
            return detail::write< true>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // strict

      namespace gentle
      {
         auto write( auto&&... parameters)
         {
            return detail::write< false>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // gentle

   } // pack
} // poly_version

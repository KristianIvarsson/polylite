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
#include <stdfloat>
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
            namespace timestamp
            {
               constexpr std::uint8_t type{ 0xFF};
               constexpr auto split{ 34};
            } // timestamp
         } // extension


         enum class format : std::uint8_t
         {
            // base values: the low bits hold the length or value
            fixmap          = 0x80,
            fixarray        = 0x90,
            fixstr          = 0xA0,

            nil             = 0xC0,
            no              = 0xC2,
            yes             = 0xC3,

            bin8            = 0xC4,
            bin16           = 0xC5,
            bin32           = 0xC6,

            ext8            = 0xC7,
            ext16           = 0xC8,
            ext32           = 0xC9,

            float8fake      = 0xC8,
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

            array8fake      = 0xDB,
            array16         = 0xDC,
            array32         = 0xDD,

            map8fake        = 0xDD,
            map16           = 0xDE,
            map32           = 0xDF,

            negative_fixint = 0xE0,
         };

         template< help::sign type, help::source_iterator< type> iterator>
         struct parser : help::source< type, iterator>
         {
            using base = help::source< type, iterator>;
            using base::deny;
            using base::done;
            using base::halt;
            using base::pull;

            auto operator()() -> node
            {
               auto nrv = spot();
               done();
               return nrv;
            }

         private:

            template< typename into>
            auto read( const auto size)
            {
               return this->template grab< into>( size);
            }

            auto take( std::size_t size)
            {
               std::uint64_t data{};
               while( size--) data = ( data << 8) | static_cast< std::uint8_t>( pull());
               return data;
            }

            auto take( const format root, const format code)
            {
               return take( std::size_t{ 1} << ( std::to_underlying( code) - std::to_underlying( root)));
            }

            auto spot( const format code) -> node
            {
               switch( code)
               {
               case format::nil:
                  return nullptr;
               case format::no:
                  return false;
               case format::yes:
                  return true;
               case format::uint8: case format::uint16: case format::uint32: case format::uint64:
                  return natural( format::uint8, code);
               case format::sint8: case format::sint16: case format::sint32: case format::sint64:
                  return integer( format::sint8, code);
               case format::float32:
                  return real( format::float8fake, code);
               case format::float64:
                  return full( format::float8fake, code);
               case format::fixext4: 
                  return timestamp( sizeof( std::uint32_t));
               case format::fixext8:
                  return timestamp( sizeof( std::uint64_t));
               case format::ext8:
                  return timestamp( static_cast< std::uint8_t>( pull()));
               case format::bin8: case format::bin16: case format::bin32:
                  return binary( natural( format::bin8, code));
               case format::str8: case format::str16: case format::str32:
                  return string( natural( format::str8, code));
               case format::array16: case format::array32:
                  return array( natural( format::array8fake, code));
               case format::map16: case format::map32:
                  return object( natural( format::map8fake, code));
               default:
                  break;
               }

               [[unlikely]] deny( "encoding");

            }

            auto spot() -> node
            {
               const auto byte = static_cast< std::uint8_t>( pull());

               if( byte >= std::to_underlying( format::negative_fixint))
                  return static_cast< std::int8_t>( byte);
               if( byte >= std::to_underlying( format::nil))
                  return spot( static_cast< format>( byte));
               if( byte >= std::to_underlying( format::fixstr))
                  return string( byte & 0x1F);
               if( byte >= std::to_underlying( format::fixarray))
                  return array( byte & 0x0F);
               if( byte >= std::to_underlying( format::fixmap))
                  return object( byte & 0x0F);

               return static_cast< std::int8_t>( byte);
            }

            auto natural( const format root, const format code)
            {
               const auto data = take( root, code);

               if( data > static_cast< std::uint64_t>( std::numeric_limits< node::integer>::max()))
                  [[unlikely]] halt( "integer overflow");
               
               return static_cast< node::integer>( data);
            }

            auto integer( const format root, const format code)
            {
               const auto data = take( root, code);
               const auto shift = 64 - ( 8u << ( std::to_underlying( code) - std::to_underlying( root)));
               return static_cast< node::integer>( data << shift) >> shift;
            }

            auto timestamp( const std::size_t size)
            {
               if( static_cast< decltype( extension::timestamp::type)>( pull()) != extension::timestamp::type)
                  [[unlikely]] halt( "unexpected extension");

               auto moment = []( const std::uint64_t seconds, const std::uint64_t details) -> node::zoned_datetime
               {
                  const auto time = 
                     std::chrono::sys_seconds{ std::chrono::seconds{ static_cast< std::int64_t>( seconds)}} +
                     std::chrono::nanoseconds{ details};
                  return { std::chrono::time_point_cast< std::chrono::system_clock::duration>( time)};
               };

               switch( size)
               {
                  case sizeof( std::uint32_t):
                  {
                     const auto seconds = take( sizeof( std::uint32_t));
                     return moment( seconds, {});
                  }
                  case sizeof( std::uint64_t):
                  {
                     const auto stuffed = take( sizeof( std::uint64_t));
                     const auto seconds = stuffed & ( ( std::uint64_t{ 1} << extension::timestamp::split) - 1);
                     const auto details = stuffed >> extension::timestamp::split;
                     return moment( seconds, details);
                  }
                  case sizeof( std::uint32_t) + sizeof( std::uint64_t):
                  {
                     const auto details = take( sizeof( std::uint32_t));
                     const auto seconds = take( sizeof( std::uint64_t));
                     return moment( seconds, details);
                  }
                  default:
                     [[unlikely]] deny( "timestamp length");
               }

            }

            auto real( const format root, const format code)
            {
               return static_cast< node::decimal>( std::bit_cast< std::float32_t>( static_cast< std::uint32_t>( take( root, code))));
            }

            auto full( const format root, const format code)
            {
               return static_cast< node::decimal>( std::bit_cast< std::float64_t>( static_cast< std::uint64_t>( take( root, code))));
            }

            auto binary( const std::size_t size) -> node::binary
            {
               return read< node::binary>( size);
            }

            auto string( const std::size_t size) -> node::string
            {
               return read< node::string>( size);
            }

            auto array( std::size_t size) -> node::array
            {
               node::array nrv;
               nrv.reserve( help::size::clip( size));
               while( size--)
                  nrv.push_back( spot());
               return nrv;
            }

            auto object( std::size_t size) -> node::object
            {
               node::object nrv;
               nrv.reserve( help::size::clip( size));
               while( size--)
               {
                  auto data = spot();
                  if( auto name = std::get_if< node::string>( &data))
                     nrv.emplace( std::move( *name), spot());
                  else
                     [[unlikely]] halt( "expected key");
               }

               return nrv;
            }
         };

      } // detail

      auto parse( auto&& source)
      {
         return help::make::source< detail::parser>( source)();
      }

      namespace detail
      {

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
               using real = std::float32_t;
               using full = std::float64_t;

               if( std::fabs( node) > std::numeric_limits< real>::max() || static_cast< real>( node) != static_cast< full>( node))
                  feed( format::float8fake, std::bit_cast< std::uint64_t>( static_cast< full>( node)), sizeof( std::uint64_t));
               else
                  feed( format::float8fake, std::bit_cast< std::uint32_t>( static_cast< real>( node)), sizeof( std::uint32_t));
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

               if( bits > 4)
                  feed( format::map8fake, size, need< 2>( bits));
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

               if( bits > 4)
                  feed( format::array8fake, size, need< 2>( bits));
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

               const auto stuffed = details << extension::timestamp::split | seconds;

               if( std::bit_width( stuffed) <= std::numeric_limits< std::uint32_t>::digits)
               {
                  //
                  // Timestamp32: nanoseconds is zero, seconds is non-negative and fits in 32 bits

                  push( format::fixext4);
                  push( extension::timestamp::type);
                  feed( seconds, sizeof( std::uint32_t));
               }
               else 
               if( std::bit_width( seconds) <= extension::timestamp::split)
               {
                  //
                  // Timestamp64: nanoseconds always fits in 30 bits, seconds is non-negative and fits in 34 bits

                  push( format::fixext8);
                  push( extension::timestamp::type);
                  feed( stuffed, sizeof( std::uint64_t));
               }
               else
               {
                  //
                  // Timestamp96: 4-byte nanoseconds, then 8-byte seconds as two's complement

                  push( format::ext8);
                  push( std::uint8_t{ sizeof( std::uint32_t) + sizeof( std::uint64_t)});
                  push( extension::timestamp::type);
                  feed( details, sizeof( std::uint32_t));
                  feed( seconds, sizeof( std::uint64_t));
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

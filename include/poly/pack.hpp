//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "core/help.hpp"
#include "core/lint.hpp"
#include "core/load.hpp"
#include "core/save.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <vector>
#include <cstddef>
#include <utility>
#include <algorithm>

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
               constexpr core::type::u08 type{ 0xFF};
               constexpr auto split{ 34};
            } // timestamp
         } // extension


         enum class format : core::type::u08
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

         template< core::type::byte unit, core::load::pipe::source origin>
         struct parser : core::load::source< unit, origin>
         {
            using base = core::load::source< unit, origin>;
            using base::base;
            using base::clip;
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
               core::type::u64 data{};
               while( size--) data = ( data << 8) | static_cast< core::type::u08>( pull());
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
                  return timestamp( sizeof( core::type::u32));
               case format::fixext8:
                  return timestamp( sizeof( core::type::u64));
               case format::ext8:
                  return timestamp( static_cast< core::type::u08>( pull()));
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
               const auto byte = static_cast< core::type::u08>( pull());

               if( byte >= std::to_underlying( format::negative_fixint))
                  return static_cast< core::type::i08>( byte);
               if( byte >= std::to_underlying( format::nil))
                  return spot( static_cast< format>( byte));
               if( byte >= std::to_underlying( format::fixstr))
                  return string( byte & 0x1F);
               if( byte >= std::to_underlying( format::fixarray))
                  return array( byte & 0x0F);
               if( byte >= std::to_underlying( format::fixmap))
                  return object( byte & 0x0F);

               return static_cast< core::type::i08>( byte);
            }

            auto natural( const format root, const format code)
            {
               const auto data = take( root, code);

               if( data > static_cast< core::type::u64>( std::numeric_limits< node::integer>::max()))
                  [[unlikely]] halt( "integer overflow");
               
               return static_cast< node::integer>( data);
            }

            auto integer( const format root, const format code)
            {
               const auto data = take( root, code);
               const auto move = 64 - ( 8u << ( std::to_underlying( code) - std::to_underlying( root)));
               return static_cast< core::type::i64>( data << move) >> move;
            }

            auto timestamp( const std::size_t size)
            {
               if( static_cast< decltype( extension::timestamp::type)>( pull()) != extension::timestamp::type)
                  [[unlikely]] halt( "unexpected extension");

               auto moment = []( const core::type::u64 seconds, const core::type::u64 details) -> node::zoned_datetime
               {
                  const auto time = 
                     std::chrono::sys_seconds{ std::chrono::seconds{ static_cast< core::type::i64>( seconds)}} +
                     std::chrono::nanoseconds{ details};
                  return { std::chrono::time_point_cast< std::chrono::system_clock::duration>( time)};
               };

               switch( size)
               {
                  case sizeof( core::type::u32):
                  {
                     const auto seconds = take( sizeof( core::type::u32));
                     return moment( seconds, {});
                  }
                  case sizeof( core::type::u64):
                  {
                     const auto stuffed = take( sizeof( core::type::u64));
                     const auto seconds = stuffed & ( ( core::type::u64{ 1} << extension::timestamp::split) - 1);
                     const auto details = stuffed >> extension::timestamp::split;
                     return moment( seconds, details);
                  }
                  case sizeof( core::type::u32) + sizeof( core::type::u64):
                  {
                     const auto details = take( sizeof( core::type::u32));
                     const auto seconds = take( sizeof( core::type::u64));
                     return moment( seconds, details);
                  }
                  default:
                     [[unlikely]] deny( "timestamp length");
               }

            }

            auto real( const format root, const format code)
            {
               return static_cast< node::decimal>( std::bit_cast< core::type::f32>( static_cast< core::type::u32>( take( root, code))));
            }

            auto full( const format root, const format code)
            {
               return static_cast< node::decimal>( std::bit_cast< core::type::f64>( static_cast< core::type::u64>( take( root, code))));
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
               nrv.reserve( clip( size));
               while( size--)
                  nrv.push_back( spot());
               return nrv;
            }

            auto object( std::size_t size) -> node::object
            {
               node::object nrv;
               nrv.reserve( clip( size));
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
         return core::load::make::source< detail::parser>( source)();
      }

      namespace detail
      {

         template< core::type::byte unit, core::save::pipe::target output>
         struct writer : core::save::target< unit, output>
         {
            using base = core::save::target< unit, output>;
            using base::base;
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
               if( node < 0)
                  negative( static_cast< core::type::u64>( node));
               else
                  positive( static_cast< core::type::u64>( node));
            }

            void operator() ( const node::decimal node)
            {
               using real = core::type::f32;
               using full = core::type::f64;

               if( std::fabs( node) > std::numeric_limits< real>::max() || static_cast< real>( node) != static_cast< full>( node))
                  feed( format::float8fake, std::bit_cast< core::type::u64>( static_cast< full>( node)), sizeof( core::type::u64));
               else
                  feed( format::float8fake, std::bit_cast< core::type::u32>( static_cast< real>( node)), sizeof( core::type::u32));
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
                  push( static_cast< core::type::u08>( std::to_underlying( format::fixstr) | size));

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
                  push( static_cast< core::type::u08>( std::to_underlying( format::fixmap) | size));

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
                  push( static_cast< core::type::u08>( std::to_underlying( format::fixarray) | size));

               for( const auto& item : node)
                  std::visit( *this, item);
            }

            void operator() ( const node::local_date data)
            {
               time( data);
            }

            void operator() ( const node::local_time data)
            {
               time( data);
            }

            void operator() ( const node::local_datetime data)
            {
               time( data);
            }
         
            void operator() ( const node::zoned_datetime data)
            {
               const auto elapsed = data.get_sys_time().time_since_epoch();
               const auto rounded = std::chrono::floor< std::chrono::seconds>( elapsed);
               const auto details = static_cast< core::type::u64>( std::chrono::duration_cast< std::chrono::nanoseconds>( elapsed - rounded).count());
               const auto seconds = static_cast< core::type::u64>( rounded.count());

               const auto stuffed = details << extension::timestamp::split | seconds;

               if( std::bit_width( stuffed) <= std::numeric_limits< core::type::u32>::digits)
               {
                  //
                  // Timestamp32: nanoseconds is zero, seconds is non-negative and fits in 32 bits

                  push( format::fixext4);
                  push( extension::timestamp::type);
                  feed( seconds, sizeof( core::type::u32));
               }
               else 
               if( std::bit_width( seconds) <= extension::timestamp::split)
               {
                  //
                  // Timestamp64: nanoseconds always fits in 30 bits, seconds is non-negative and fits in 34 bits

                  push( format::fixext8);
                  push( extension::timestamp::type);
                  feed( stuffed, sizeof( core::type::u64));
               }
               else
               {
                  //
                  // Timestamp96: 4-byte nanoseconds, then 8-byte seconds as two's complement

                  push( format::ext8);
                  push( core::type::u08{ sizeof( core::type::u32) + sizeof( core::type::u64)});
                  push( extension::timestamp::type);
                  feed( details, sizeof( core::type::u32));
                  feed( seconds, sizeof( core::type::u64));
               }
            }

         private:

            void time( const auto& data)
            {
               std::string string;
               core::save::writer< char, core::save::pipe::memory< char, std::string>>( string).time( data);
               (*this)( string);
            }

            //! @return the smallest size (1, 2, 4 or 8 bytes) for a number of significant bits
            template< std::size_t min = 1, std::size_t max = 8>
            static constexpr auto need( const auto bits) noexcept
            {
               return std::clamp( std::bit_ceil( static_cast< std::size_t>( ( bits + 7) / 8)), min, max);
            }

            void feed( const core::type::u64 data, std::size_t size)
            {
               while( size--) push( static_cast< core::type::u08>( data >> ( size * 8)));
            }            

            //! @param root is the base tag
            void feed( const format root, const core::type::u64 data, const std::size_t size)
            {
               push( static_cast< core::type::u08>( std::to_underlying( root) + std::countr_zero( size)));
               feed( data, size);
            }            

            void negative( const core::type::u64 data)
            {
               const auto bits = std::bit_width( ~data) + 1;
               if( bits > 6)
                  feed( format::sint8, data, need( bits));
               else
                  push( static_cast< core::type::u08>( data));
            }

            void positive( const core::type::u64 data)
            {
               const auto bits = std::bit_width( data);
               if( bits > 7)
                  feed( format::uint8, data, need( bits));
               else
                  push( static_cast< core::type::u08>( data));
            }
         };

         inline auto write( const node& root, auto&& target)
         {
            std::visit( core::save::make::target< writer>( target), root);
         }

         // the default write function
         inline auto write( const node& root)
         {
            std::vector< std::byte> target;
            write( root, target);
            return target;
         }

      } // detail

      auto write( auto&&... parameters)
      {
         return detail::write( std::forward< decltype( parameters)>( parameters)...);
      }

      namespace detail
      {
         struct rule : core::lint::rule
         {
            template< typename type>
            static constexpr bool block = core::type::some< type, node::local_date, node::local_time, node::local_datetime>;
            static constexpr bool multi = true;
         };
      } // detail

      decltype( auto) check( auto&& data)
      {
         return core::lint::test< detail::rule>( std::forward< decltype( data)>( data));
      }
   } // pack
} // poly_version

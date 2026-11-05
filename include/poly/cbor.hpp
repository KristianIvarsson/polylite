//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <bit>
#include <cmath>
#include <tuple>
#include <chrono>
#include <limits>
#include <ranges>
#include <string>
#include <vector>
#include <cstddef>

namespace poly_version
{
   namespace cbor
   {
      namespace detail
      {
         enum class major : help::type::u08
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

         enum class simple : help::type::u08
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

         enum class chrono : help::type::u64
         {
            when = 0,      // standard date/time string  (RFC 8949)
            tick = 1,      // epoch-based date/time      (RFC 8949)
            days = 100,    // days since 1970-01-01      (RFC 8943)
            date = 1004,   // RFC 3339 full-date string  (RFC 8943)
         };         

         constexpr auto mask = help::type::u08{ 0x1F};

         constexpr auto half( const help::type::u16 data) -> node::decimal
         {
            struct layout
            {
               help::type::u16 mantissa : 10;
               help::type::u16 exponent :  5;
               help::type::u16 sign     :  1;
            };

            const auto bits = std::bit_cast< layout>( data);            

            auto result = [&bits] -> node::decimal
            {
               if( bits.exponent == 0) 
                  return std::ldexp( bits.mantissa, -24);

               if( bits.exponent == 31) 
                  return bits.mantissa ? 
                     std::numeric_limits< node::decimal>::quiet_NaN() : 
                     std::numeric_limits< node::decimal>::infinity();

               return std::ldexp( bits.mantissa + 1024, bits.exponent - 25);
            }();

            return bits.sign ? -result : +result;
         }

         constexpr auto real( const help::type::u32 data) -> node::decimal
         {
            return std::bit_cast< help::type::f32>( data);
         }

         constexpr auto full( const help::type::u64 data) -> node::decimal
         {
            return std::bit_cast< help::type::f64>( data);
         }

         template< help::byte unit, help::source_iterator< unit> iterator>
         struct parser : help::source< unit, iterator>
         {
            using base = help::source< unit, iterator>;
            using base::mark;
            using base::last;
            using base::deny;
            using base::done;
            using base::drop;
            using base::halt;
            using base::peek;
            using base::pull;

            auto operator()() -> node
            {
               auto nrv = spot();
               done();
               return nrv;
            }

         private:

            bool more()
            {
               if( static_cast< std::byte>( peek()) != std::byte{ 0xFF}) 
                  return true;

               return drop(), false;
            }

            auto array( auto size) -> node::array
            {
               node::array nrv;
               nrv.reserve( help::clip::size( size));
               while( size--) nrv.push_back( spot());
               return nrv;
            }

            auto array() -> node::array
            {
               node::array nrv;

               while( more()) nrv.push_back( spot());
               return nrv;
            }

            auto pair()
            {
               auto data = spot();
               if( auto name = std::get_if< node::string>( &data))
                  return std::make_pair( std::move( *name), spot());
               [[unlikely]] halt( "expected key");
            }

            auto object( auto size) -> node::object
            {
               node::object nrv;
               nrv.reserve( help::clip::size( size));
               while( size--)
                  nrv.insert( pair());
               return nrv;
            }

            auto object() -> node::object
            {
               node::object nrv;
               while( more())
                  nrv.insert( pair());
               return nrv;
            }

            auto when( const node& data) -> node::instant
            {
               return help::transform::instant( std::get< node::string>( data)).value();
            }

            auto tick( const node& data) -> node::instant
            {
               const auto elapsed = data.is_integer()
                  ? std::chrono::duration_cast< std::chrono::system_clock::duration>( std::chrono::seconds{ data.as_integer()})
                  : std::chrono::duration_cast< std::chrono::system_clock::duration>( std::chrono::duration< node::decimal>{ data.as_decimal()});

               return node::zoned_datetime{ std::chrono::sys_time< std::chrono::system_clock::duration>{ elapsed}};
            }

            auto days( const node& data) -> node::instant
            {
               return node::local_date{ std::chrono::days{ data.as_integer()}};
            }

            auto tagged( const auto data) -> node
            {
               switch( static_cast< chrono>( data))
               {
               case chrono::when:
               case chrono::date: return when( spot());
               case chrono::tick: return tick( spot());
               case chrono::days: return days( spot());
               }

               deny( "tag");
            }

            auto integer( const help::type::u64 data)
            {
               if( data > static_cast< help::type::u64>( std::numeric_limits< node::integer>::max()))
                  [[unlikely]] halt( "integer overflow");
               return static_cast< node::integer>( data);
            }

            template< typename into>
            auto read( const auto size)
            {
               return this->template grab< into>( size);
            }

            template< typename into, major want>
            auto read()
            {
               into nrv;
               while( more())
               {
                  const auto [ kind, info] = next();
                  if( kind != want || info == simple::stop) halt( "malformed chunk");
                  std::ranges::move( read< into>( load( info)), std::back_inserter( nrv));
               }
               return nrv;
            }

            auto binary( const auto size) -> node::binary
            {
               return read< node::binary>( size);
            }

            auto binary() -> node::binary
            {
               return read< node::binary, major::bytes>();
            }

            auto string( const auto size) -> node::string
            {
               return read< node::string>( size);
            }

            auto string() -> node::string
            {
               return read< node::string, major::text>();
            }

            template< std::size_t size>
            auto take() -> help::type::u64
            {
               help::type::u64 data{};
               for( std::size_t item{}; item < size; ++item)
                  data = ( data << 8) | static_cast< help::type::u08>( pull());
               return data;
            }

            auto next() -> std::tuple< major, simple>
            {
               const auto byte = static_cast< help::type::u08>( pull());
               
               return { static_cast< major>( byte >> 5), static_cast< simple>( byte & mask)};
            }

            auto load( const simple info) -> help::type::u64
            {
               if( info < simple::byte) 
                  return std::to_underlying( info); // inline

               switch( info)
               {
               case simple::byte: return take< sizeof( help::type::u08)>();
               case simple::half: return take< sizeof( help::type::u16)>();
               case simple::real: return take< sizeof( help::type::u32)>();
               case simple::full: return take< sizeof( help::type::u64)>();
               default: break; // -Wswitch
               }

               halt( "invalid info");
            }

            auto spot() -> node
            {
               const auto [ kind, info] = next();

               if( info == simple::stop && kind != major::simple)
               {
                  switch( kind)
                  {
                  case major::bytes: return binary();
                  case major::text:  return string();
                  case major::array: return array();
                  case major::map:   return object();
                  default: break; // -Wswitch
                  }
               }

               const auto data = load( info);

               switch( kind)
               {
               case major::positive: return  integer( data);
               case major::negative: return ~integer( data);

               case major::bytes:    return binary(   data);
               case major::text:     return string(   data);
               case major::array:    return array(    data);
               case major::map:      return object(   data);
               case major::tag:      return tagged(   data);
               case major::simple:
               {
                  switch( info)
                  {
                  case simple::no:   return false;
                  case simple::yes:  return true;
                  case simple::null:
                  case simple::none: return nullptr;
                  case simple::half: return half( static_cast< help::type::u16>( data));
                  case simple::real: return real( static_cast< help::type::u32>( data));
                  case simple::full: return full( static_cast< help::type::u64>( data));
                  default: break; // -Wswitch
                  }
               }
               }

               deny( "encoding");
            }

         };

         constexpr auto head( major kind, simple info) -> help::type::u08
         {
            constexpr help::type::u08 bits{ 5};
            return ( std::to_underlying( kind) << bits) | ( std::to_underlying( info) & mask);
         }

         template< help::byte unit, help::target_iterator< unit> iterator, bool packed, bool strict>
         struct writer : help::target< unit, iterator>
         {
            using base = help::target< unit, iterator>;
            using base::emit;
            using base::halt;
            using base::push;

            void operator() ( const node::nothing)
            {
               push( head( major::simple, simple::null));
            }

            void operator() ( const node::boolean node)
            {
               push( head( major::simple, node ? simple::yes : simple::no));
            }

            void operator() ( const node::integer node)
            {
               if( node < 0)
                  pack( major::negative, ~static_cast< help::type::u64>( node));
               else
                  pack( major::positive,  static_cast< help::type::u64>( node));
            }

            void operator() ( const node::decimal node)
            {
               full( major::simple, std::bit_cast< help::type::u64>( node));
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

            void operator() ( const node::local_date data)
            {
               if constexpr( packed)
               {
                  pack( major::tag, std::to_underlying( chrono::days));
                  (*this)( static_cast< node::integer>( data.time_since_epoch().count()));
               }
               else
               {
                  pack( major::tag, std::to_underlying( chrono::date));
                  time( data);
               }
            }

            void operator() ( const node::local_time data)
            {
               if constexpr( strict) halt( "node::local_time");
               else time( data);
            }

            void operator() ( const node::local_datetime data)
            {
               if constexpr( strict) halt( "node::local_datetime");
               else time( data);
            }

            void operator() ( const node::zoned_datetime data)
            {
               if constexpr( packed)
               {
                  pack( major::tag, std::to_underlying( chrono::tick));
                  const auto elapsed = data.get_sys_time().time_since_epoch();
                  const auto rounded = std::chrono::floor< std::chrono::seconds>( elapsed);
                  if( elapsed == rounded)
                     (*this)( std::chrono::duration< node::integer>( rounded).count());
                  else
                     (*this)( std::chrono::duration< node::decimal>( elapsed).count());
               }
               else
               {
                  pack( major::tag, std::to_underlying( chrono::when));
                  time( data);
               }
            }

         private:

            void time( const auto data)
            {
               std::string string;
               help::writer< char, std::back_insert_iterator< std::string>>{ std::back_inserter( string)}.time( data);
               (*this)( string);
            }

            void full( major kind, help::type::u64 data)
            {
               push( head( kind, simple::full));

               push( static_cast< help::type::u08>( data >> 56));
               push( static_cast< help::type::u08>( data >> 48));
               push( static_cast< help::type::u08>( data >> 40));
               push( static_cast< help::type::u08>( data >> 32));
               push( static_cast< help::type::u08>( data >> 24));
               push( static_cast< help::type::u08>( data >> 16));
               push( static_cast< help::type::u08>( data >>  8));
               push( static_cast< help::type::u08>( data));
            }

            void pack( major kind, help::type::u64 data)
            {
               constexpr auto edge = std::to_underlying( simple::byte);

               if( data < edge)
               {
                  push( head( kind, static_cast< simple>( data)));
               }
               else if( data <= std::numeric_limits< help::type::u08>::max())
               {
                  push( head( kind, simple::byte));
                  push( static_cast< help::type::u08>( data));
               }
               else if( data <= std::numeric_limits< help::type::u16>::max())
               {
                  push( head( kind, simple::half));
                  push( static_cast< help::type::u08>( data >> 8));
                  push( static_cast< help::type::u08>( data));
               }
               else if( data <= std::numeric_limits< help::type::u32>::max())
               {
                  push( head( kind, simple::real));
                  push( static_cast< help::type::u08>( data >> 24));
                  push( static_cast< help::type::u08>( data >> 16));
                  push( static_cast< help::type::u08>( data >>  8));
                  push( static_cast< help::type::u08>( data));
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

      auto parse( auto&& source)
      {
         return help::make::source< detail::parser>( source)();
      }

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
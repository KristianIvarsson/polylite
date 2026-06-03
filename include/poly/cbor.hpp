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
#include <cstdint> 
#include <stdfloat>

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

         constexpr auto mask = std::uint8_t{ 0x1F};

         constexpr auto half( const std::uint16_t data) -> node::decimal
         {
            struct layout
            {
               std::uint16_t mantissa : 10;
               std::uint16_t exponent :  5;
               std::uint16_t sign     :  1;
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

         constexpr auto real( const std::uint32_t data) -> node::decimal
         {
            return std::bit_cast< std::float32_t>( data);
         }

         constexpr auto full( const std::uint64_t data) -> node::decimal
         {
            return std::bit_cast< std::float64_t>( data);
         }

         template< help::sign type, help::source_iterator< type> iterator>
         struct parser : help::source< type, iterator>
         {
            using base = help::source< type, iterator>;
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
               nrv.reserve( size);
               while( size--) nrv.push_back( spot());
               return nrv;
            }

            auto array() -> node::array
            {
               node::array nrv;

               while( more()) nrv.push_back( spot());
               return nrv;
            }

            auto object( auto size) -> node::object
            {
               node::object nrv;
               if constexpr( requires { nrv.reserve( size); }) nrv.reserve( size);
               while( size--)
               {
                  auto name = spot();
                  nrv.emplace( std::get< node::string>( std::move( name)), spot());
               }
               return nrv;
            }

            auto object() -> node::object
            {
               node::object nrv;
               while( more())
               {
                  auto name = spot();
                  nrv.emplace( std::get< node::string>( std::move( name)), spot());
               }
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

               return node::zoned_datetime{ "UTC", std::chrono::sys_time{ elapsed}};
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

            auto integer( const std::uint64_t data)
            {
               if( data > static_cast< std::uint64_t>( std::numeric_limits< node::integer>::max()))
                  halt( "integer overflow");
               return static_cast< node::integer>( data);
            }

            template< typename into>
            auto read( const auto size)
            {
               if constexpr( std::contiguous_iterator< iterator>)
               {
                  auto save = mark;
                  if( std::ranges::advance( mark, size, last))
                     pull(); // provoke an error

                  return std::ranges::subrange( save, mark)
                     | std::views::transform( []( auto byte) { return static_cast< into::value_type>( byte); })
                     | std::ranges::to< into>();
               }
               else
               {
                  //
                  // fallback for non-contiguous iterators
                  // - correctness for single-pass iterators
                  // - performance for non-contiguous iterators

                  into data( size, {});
                  for( std::size_t item{}; item < size; ++item)
                     data[ item] = static_cast< into::value_type>( pull());
                  return data;
               } 
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
            auto take() -> std::uint64_t
            {
               std::uint64_t data{};
               for( std::size_t item{}; item < size; ++item)
                  data = ( data << 8) | static_cast< std::uint8_t>( pull());
               return data;
            }

            auto next() -> std::tuple< major, simple>
            {
               const auto byte = static_cast< std::uint8_t>( pull());
               
               return { static_cast< major>( byte >> 5), static_cast< simple>( byte & mask)};
            }

            auto load( const simple info) -> std::uint64_t
            {
               if( info < simple::byte) 
                  return std::to_underlying( info); // inline

               switch( info)
               {
               case simple::byte: return take< sizeof( std::uint8_t )>();
               case simple::half: return take< sizeof( std::uint16_t)>();
               case simple::real: return take< sizeof( std::uint32_t)>();
               case simple::full: return take< sizeof( std::uint64_t)>();
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
                  case simple::half: return half( data);
                  case simple::real: return real( data);
                  case simple::full: return full( data);
                  default: break; // -Wswitch
                  }
               }
               }

               deny( "encoding");
            }

         };

         constexpr auto head( major kind, simple info) -> std::uint8_t
         {
            constexpr std::uint8_t bits{ 5};
            return ( std::to_underlying( kind) << bits) | ( std::to_underlying( info) & mask);
         }

         template< help::sign type, help::target_iterator< type> iterator, bool packed, bool strict>
         struct writer : help::target< type, iterator>
         {
            using base = help::target< type, iterator>;
            using base::emit;
            using base::halt;
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
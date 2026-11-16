//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "type.hpp"
#include "../node.hpp"

#include <chrono>
#include <limits>
#include <ranges>
#include <string>
#include <charconv>
#include <iterator>
#include <optional>
#include <algorithm>
#include <spanstream>
#include <string_view>

namespace poly_version
{
   namespace core::help
   {
      template< typename... types>
      constexpr bool hold( const auto& data)
      {
         return ( std::holds_alternative< types>( data) || ... );
      }

      //! for performance (only)
      namespace is
      {
         namespace in
         {
            template< auto lower, auto upper>
            bool range( const auto sign)
            {
               return sign >= lower && sign <= upper;
            }
         } // in

         //! sign is one of the signs
         template< auto... signs>
         bool some( const auto sign)
         {
            return ( ( sign == signs) || ...);
         }

         //! sign is none of the signs
         template< auto... signs>
         bool none( const auto sign)
         {
            return ( ( sign != signs) && ...);
         }

         bool white( const auto sign)
         {
            return is::some< ' ', '\n', '\r', '\t'>( sign);
         }         

         bool inert( const auto sign)
         {
            return in::range< '\0', ' '>( sign);
         }


         //! @{ somehow as the standard
         bool space( const auto sign)
         {
            return sign == ' ' || in::range< '\t', '\r'>( sign);
         }

         bool digit( const auto sign)
         {
            return in::range< '0', '9'>( sign);
         }

         bool lower( const auto sign)
         {
            return in::range< 'a', 'z'>( sign);
         }

         bool upper( const auto sign)
         {
            return in::range< 'A', 'Z'>( sign);
         }

         bool alpha( const auto sign)
         {
            return lower( sign) || upper( sign);
         }

         bool alnum( const auto sign)
         {
            return alpha( sign) || digit( sign);
         }

         bool xdigit( const auto sign)
         {
            return digit( sign) || in::range< 'a', 'f'>( sign) || in::range< 'A', 'F'>( sign);
         }

         bool cntrl( const auto sign)
         {
            return in::range< 0x0, 0x1F>( sign) || sign == 0x7F;
         }
         //! @}
      } // is

      namespace to
      {
         auto lower( const auto sign)
         {
            return is::upper( sign) ? sign + ( 'a' - 'A') : sign;
         }

         auto upper( const auto sign)
         {
            return is::lower( sign) ? sign - ( 'a' - 'A') : sign;
         }
      } // to
      
      namespace till
      {
         constexpr auto quot = []( const auto sign) { return is::none< '"', '\\'>( sign); };
         constexpr auto apos = []( const auto sign) { return is::none< '\''>( sign); };
         constexpr auto line = []( const auto sign) { return is::none< '\n'>( sign); };         
      } // till

      auto trim( auto data)
      {
         auto white = [] ( const auto sign) { return is::white( sign); };
         data.erase( std::ranges::find_if_not( data | std::views::reverse, white).base(), data.end());
         data.erase( data.begin(), std::ranges::find_if_not( data, white));
         return data;
      }

      namespace transform
      {
         inline auto simple( std::string_view data) -> std::optional< node>
         {
            auto compare = [&data] ( std::string_view what)
            {
               return std::ranges::equal( data, what, [] ( const auto lhs, const auto rhs) { return to::lower( lhs) == rhs; });
            };

            if( compare( "null"))
               return nullptr;
            if( compare( "true"))
               return true;
            if( compare( "false"))
               return false;
            
            return {};
         }

         inline auto number( std::string_view data) -> std::optional< node>
         {
            const auto positive = data.starts_with( '+');
            const auto negative = data.starts_with( '-');
            
            auto start = data.data() + positive + negative;
            const auto cease = data.data() + data.size();

            // integer
            {
               const auto base = [&]
               {
                  if( std::distance( start, cease) > 2 && *start == '0')
                  {
                     switch( *(start + 1))
                     {
                     case 'b': std::advance( start, 2); return 2;
                     case 'o': std::advance( start, 2); return 8;
                     case 'x': std::advance( start, 2); return 16;
                     }
                  }
                  return 10;
               }();

               std::make_unsigned_t< node::integer> value;
               if( const auto [ptr, ec] = std::from_chars( start, cease, value, base); ptr == cease && ec == std::errc{})
               {
                  constexpr auto max = static_cast< std::make_unsigned_t< node::integer>>( std::numeric_limits< node::integer>::max());
                  if( negative)
                  {
                     if( value <= max + 1u)
                        return static_cast< node::integer>( 0u - value);
                  }
                  else
                  {
                     if( value <= max + 0u)
                        return static_cast< node::integer>( 0u + value);
                  }
                  return {};
               }
            }

            // decimal
            {
               node::decimal value;
               const auto result = std::from_chars( start, cease, value);
               if( result.ec == std::errc{} && result.ptr == cease)
                  return negative ? - value : + value;
            }

            return {};
         }

         inline auto instant( std::string_view data) -> std::optional< node::instant>
         {
            auto parse = [&data] < typename unit>( const auto& format) -> std::optional< unit>
            {
               // parse with nanosecond precision and truncate to the precision of type
               decltype( std::chrono::floor< std::chrono::nanoseconds>( unit{})) result{};
               std::ispanstream stream{ data};
               if( stream >> std::chrono::parse( format, result) && stream.peek() == std::ispanstream::traits_type::eof())
                  return std::chrono::floor< decltype( unit{} - unit{})>( result);

               return {};
            };

            if( data.ends_with( 'Z'))
               if( auto result = parse.template operator()< std::chrono::system_clock::time_point>( "%FT%TZ"))
                  return *result;

            if( auto result = parse.template operator()< node::local_date>( "%F"))
               return *result;

            if( auto result = parse.template operator()< node::local_time::precision>( "%T"))
               return node::local_time( *result);

            if( auto result = parse.template operator()< node::local_datetime>( "%FT%T"))
               return *result;

            if( auto result = parse.template operator()< std::chrono::system_clock::time_point>( "%FT%T%Ez"))
               return *result;

            return {};
         }

         inline auto binary( std::string_view data) -> std::optional< node::binary>
         {
            node::binary result;
            result.reserve( data.size() * 3 / 4);

            core::type::u32 pack{};
            int bits{};

            for( const auto sign : data)
            {
               if( ! is::white( sign) && sign != '=')
               {
                  const auto spot = [sign]
                  {
                     if( is::upper( sign)) return sign - 'A';
                     if( is::lower( sign)) return sign - 'a' + 26;
                     if( is::digit( sign)) return sign - '0' + 52;
                     if( sign == '+') return 62;
                     if( sign == '/') return 63;
                     return -1;
                  }();

                  if( spot < 0)
                     return {};

                  pack = ( pack << 6) | spot;
                  bits += 6;

                  if( bits >= 8)
                  {
                     bits -= 8;
                     result.push_back( static_cast< std::byte>( (pack >> bits) & 0xFF));
                  }
               }
            }

            return result;
         }            

         auto point( const std::same_as< core::type::i32> auto rune)
         {
            std::string nrv;

            using unit = std::string::value_type;

            if( rune < 0x80)
            {
               nrv.push_back( static_cast< unit>( rune));
            }
            else if( rune < 0x800)
            {
               nrv.push_back( static_cast< unit>( 0xC0 | (( rune >> 6) & 0x1F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }
            else if( rune < 0x10000)
            {
               nrv.push_back( static_cast< unit>( 0xE0 | (( rune >> 12) & 0x0F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 6) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }
            else
            {
               nrv.push_back( static_cast< unit>( 0xF0 | (( rune >> 18) & 0x07)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 12) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 6) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }

            return nrv;
         }

      } // transform
   } // core::help
} // poly_version

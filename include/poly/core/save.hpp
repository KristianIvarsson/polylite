//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "type.hpp"
#include "help.hpp"
#include "../node.hpp"

#include <span>
#include <chrono>
#include <format>
#include <ranges>
#include <ostream>
#include <concepts>
#include <stdexcept>
#include <string_view>

namespace poly_version
{
   namespace core::save
   {
      namespace pipe
      {
         //! something the writers can write to
         template< typename type>
         concept target = requires( type& output) { output.push( std::byte{}); output.copy( std::span< std::byte>{}); };

         //! container target, i.e. appends directly to the container
         template< type::byte unit, typename container>
         class memory : type::pinned
         {
         private:

            container& into;

         public:

            explicit memory( container& target) : into{ target} {}

            void push( const type::byte auto sign) { into.push_back( static_cast< unit>( sign)); }

            void copy( const std::ranges::contiguous_range auto& data)
               requires type::byte< std::ranges::range_value_t< decltype( data)>>
            {
               const auto lead = reinterpret_cast< const unit*>( std::ranges::data( data));
               into.insert( into.end(), lead, lead + std::ranges::size( data));
            }
         };

         template< std::ranges::range container>
         memory( container&) -> memory< std::ranges::range_value_t< container>, container>;

         //! writes to an ostream (that has a buffer of its own)
         template< type::byte unit>
         class stream : type::pinned
         {
         private:

            using traits = std::basic_ostream< unit>::traits_type;

            std::basic_ostream< unit>& into;

         public:

            explicit stream( std::basic_ostream< unit>& target) : into{ target} {}

            void push( const type::byte auto sign)
            {
               if( traits::eq_int_type( into.rdbuf()->sputc( static_cast< unit>( sign)), traits::eof()))
                  into.setstate( std::ios::badbit);
            }

            void copy( const std::ranges::contiguous_range auto& data)
               requires type::byte< std::ranges::range_value_t< decltype( data)>>
            {
               const auto size = static_cast< std::streamsize>( std::ranges::size( data));
               const auto sent = into.rdbuf()->sputn( reinterpret_cast< const unit*>( std::ranges::data( data)), size);

               if( sent != size)
                  into.setstate( std::ios::badbit);
            }
         };
      } // pipe

      template< type::byte unit, pipe::target output>
      struct target
      {
         output sink;

         explicit target( auto& origin) : sink{ origin} {}

         void push( const type::byte auto data)
         {
            sink.push( data);
         }

         void emit( const std::ranges::contiguous_range auto& data)
         {
            sink.copy( data);
         }

         [[noreturn]] static void halt( const auto& what)
         {
            throw std::invalid_argument{ std::format( "{} is not valid", what)};
         }
      };

      template< type::byte unit, pipe::target output>
      struct writer : target< unit, output>
      {
         using base = target< unit, output>;
         using base::base;
         using base::push;
         using base::emit;

         void copy( const std::string_view data)
         {
            emit( data);
         }

         void fill( std::size_t size)
         {
            static constexpr std::string_view hole{ "                                "};
            while( size > hole.size()) emit( hole), size -= hole.size();
            emit( hole.substr( 0, size));
         }

         void fold( const std::size_t size)
         {
            push( '\n'); fill( size);
         }

         void cast( const std::string_view data)
         {
            auto stop = std::ranges::find_if( data, [] ( const auto sign) { return help::is::some< '\\', '\"'>( sign) || help::is::cntrl( sign); });

            if( stop == std::end( data)) [[likely]]
               return emit( data);

            emit( std::string_view{ std::begin( data), stop});

            push( '\\');

            if( const auto sign = *stop++; help::is::cntrl( sign))
               if( help::is::some< '\b', '\t', '\n', '\f', '\r'>( sign))
                  push( "btnvfr"[ sign - '\b']);
               else
                  emit( std::format( "u{:04x}", static_cast< unsigned char>( sign)));
            else
               push( sign);

            cast( std::string_view{ stop, std::end( data)});
         }

         void time( const node::local_time data)
         {
            const auto duration = data.to_duration();

            auto sink = [&] ( const auto floored)
            {
               if( duration != floored) return false;
               emit( std::format( "{:%T}", floored));
               return true;
            };

            sink( std::chrono::floor< std::chrono::seconds>( duration)) ||
            sink( std::chrono::floor< std::chrono::milliseconds>( duration)) ||
            sink( std::chrono::floor< std::chrono::microseconds>( duration)) ||
            sink( duration);
         }

         void time( const node::local_date data)
         {
            emit( std::format( "{:%F}", data));
         }

         void time( const node::local_datetime data)
         {
            time( std::chrono::floor< std::chrono::days>( data));
            push( 'T');
            time( std::chrono::hh_mm_ss{ data - std::chrono::floor< std::chrono::days>( data)});
         }

         void time( const node::zoned_datetime data)
         {
            time( data.get_local_time());
            emit( std::format( "{:%Ez}", data));
         }

         template< std::size_t size = 76>
         auto wrap( const auto& blob, const std::size_t dent = 0)
         {
            constexpr std::string_view alphabet{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

            auto chunks = blob | std::views::chunk( 3);

            std::size_t list = size - 1;

            auto feed = [&]( const char sign)
            {
               if constexpr( size)
                  if( ++list == size) { fold( dent); list = 0; }

               push( sign);
            };

            for( auto chunk : chunks)
            {
               if( chunk.size() == 3)
               {
                  const auto b1 = std::to_integer< type::u32>( chunk[0]);
                  const auto b2 = std::to_integer< type::u32>( chunk[1]);
                  const auto b3 = std::to_integer< type::u32>( chunk[2]);

                  const type::u32 triple = ( b1 << 16) | ( b2 << 8) | b3;

                  feed( alphabet[ (triple >> 18) & 0x3F]);
                  feed( alphabet[ (triple >> 12) & 0x3F]);
                  feed( alphabet[ (triple >> 6) & 0x3F]);
                  feed( alphabet[ triple & 0x3F]);
               }
               else
               {
                  type::u32 triple = std::to_integer< type::u32>( chunk[0]) << 16;
                  if( chunk.size() == 2) triple |= std::to_integer< type::u32>( chunk[1]) << 8;

                  feed( alphabet[ (triple >> 18) & 0x3F]);
                  feed( alphabet[ (triple >> 12) & 0x3F]);
                  feed( ( chunk.size() == 2) ? alphabet[ (triple >> 6) & 0x3F] : '=');
                  feed( '=');
               }
            }

            if constexpr( size)
               push( '\n');
         }

         auto flat( const auto& blob)
         {
            wrap< 0>( blob);
         }
      };

      namespace make
      {
         template< template< typename, typename, auto...> class writer, auto... flags, typename unit>
         auto target( std::basic_ostream< unit>& value)
         {
            return writer< unit, pipe::stream< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class writer, auto... flags>
         auto target( std::ranges::range auto& value)
         {
            using into = std::remove_cvref_t< decltype( value)>;
            return writer< std::ranges::range_value_t< into>, pipe::memory< std::ranges::range_value_t< into>, into>, flags...>( value);
         }
      } // make
   } // core::save
} // poly_version

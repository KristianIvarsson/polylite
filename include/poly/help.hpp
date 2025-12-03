//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#define version v1_3_0

#include <array>
#include <format>
#include <ranges>
#include <string>
#include <istream>
#include <iterator>
#include <stdexcept>

namespace poly::help 
{
   inline namespace version
   {
      namespace stream
      {
         namespace ignore
         {
            //! ignores possible UTF8-BOM
            std::istream& bom( std::istream& stream)
            {
               std::array< char, 3> data{};

               const auto count  = stream.read( data.data(), data.size()).gcount();

               if( ! std::ranges::equal( data, std::string_view{ "\xEF\xBB\xBF"}))
                  stream.clear(), stream.seekg( 0 - count, std::ios::cur);
               
               return stream;
            }
         } // bom
         
         namespace buffer
         {
            namespace iterator
            {
               struct parser
               {
                  std::istreambuf_iterator< std::istream::char_type> mark;

                  parser( std::istream& stream) : mark{ stream} {}

                  auto read( auto&& want)
                  {
                     return 
                        std::ranges::subrange( mark, decltype( mark){}) | 
                        std::views::take_while( want) | 
                        std::ranges::to< std::string>();
                  }

                  bool good() const
                  {
                     return mark != decltype( mark){};
                  }

                  char pull()
                  {
                     if( good()) [[likely]]
                        return *mark++;
                     [[unlikely]] halt( "unexpected end of stream");
                  }

                  char peek() const
                  {
                     if( good()) [[likely]]
                        return *mark;
                     return std::char_traits< std::istream::char_type>::eof();
                  }

                  void test( const char want, const char pick) const
                  {
                     if( want != pick) [[unlikely]] halt( "unexpected character");
                  }

                  [[noreturn]] void halt( const std::string_view message) const
                  {
                     throw std::runtime_error{ std::format( "{} with just {} bytes left to parse", message, std::distance( mark, decltype( mark){}))};
                  }

               };

               struct writer
               {
                  std::ostreambuf_iterator< std::istream::char_type> mark;

                  writer( std::ostream& stream) : mark{ stream} {}

                  void push( const auto sign)
                  {
                     *mark++ = sign;
                  }

                  void copy( const std::string_view data)
                  {
                     std::ranges::copy( data, mark);
                  }
               };

            } // iterator
         } // buffer
      } // stream
   } // version
} // poly::help

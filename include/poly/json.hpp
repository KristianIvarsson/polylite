//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "node.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <iomanip>
#include <spanstream>
#include <sstream>

#include <stdexcept>

namespace poly
{

   inline namespace v1_1_0
   {

      namespace json
      {

         namespace detail
         {
            void validate( const char wanted, const char picked)
            {
               if( wanted != picked) [[unlikely]] throw std::runtime_error{ std::format( "unexpected character (wanted '{}' [0x{:X}] picked '{}' [0x{:X}])", wanted, wanted, picked, picked)};
            }

            struct parser
            {
               parser( std::istream& stream) : stream{ stream} {}

               void operator() ( node::table& node)
               {
                  validate( '{', pull());

                  if( peep() == '}')
                     return skip();

                  while( true)
                  {
                     std::string name;

                     //stream >> std::quoted( name);
                     std::getline( stream, name, '"');
                     std::getline( stream, name, '"');

                     validate( ':', pick());

                     std::visit( *this, node.emplace( std::move( name), detect()).first->second);

                     if( const auto sign = pick(); sign != ',')
                        return validate( '}', sign);
                  }
               }

               void operator() ( node::array& node)
               {
                  validate( '[', pull());

                  if( peep() == ']')
                     return skip();

                  while( true)
                  {
                     std::visit( *this, node.emplace_back( detect()));

                     if( const auto sign = pick(); sign != ',')
                        return validate( ']', sign);
                  }
               }

               void operator() ( auto& node)
               {
                  // scalars are already handled
               }

               auto operator()()
               {
                  auto result = detect();
                  std::visit( *this, result);
                  validate( std::char_traits< decltype( pick())>::eof(), pick());
                  return result;
               }

            private:

               void leap()
               {
                  while( std::isspace( stream.peek())) stream.get();
               }

               void skip()
               {
                  stream.get();
               }

               char peek()
               {
                  return stream.peek();
               }

               char peep()
               {
                  return leap(), peek();
               }

               char pull()
               {
                  return stream.get();
               }

               char pick()
               {
                  return leap(), pull();
               }

               void back()
               {
                  stream.unget();
               }

               auto unit()
               {
                  std::array< char, 4> data{};
                  const auto count = stream.read( data.data(), data.size()).gcount();

                  std::int32_t code;
                  const auto result = std::from_chars( data.data(), data.data() + data.size(), code, 16);

                  if( count != data.size() || result.ec != std::errc{} || result.ptr != (data.data() + data.size()))
                     throw std::runtime_error{ std::format( "invalid code point [{}]", std::string_view{ data})};

                  return code;
               }

               auto code()
               {
                  const auto lead = unit();

                  if( lead < 0xD800 || lead > 0xDFFF)
                     return lead;

                  if( lead > 0xDBFF)
                     throw std::runtime_error{ std::format( "invalid 1st surrogate [0x{:X}]", lead)};

                  validate( '\\', pull()); validate( 'u', pull());

                  const auto tail = unit();

                  if( tail < 0xDC00 || tail > 0xDFFF)
                     throw std::runtime_error{ std::format( "invalid 2nd surrogate [0x{:X}]", tail)};

                  return 0x10000 + ( ( lead - 0xD800) << 10) + ( tail - 0xDC00);
               }

               auto detect() -> node
               {
                  switch( peep())
                  {
                  case '{':
                     return { node::table{}};
                  case '[':
                     return { node::array{}};
                  case '"':
                     return string();
                  default:
                     return simple();
                  }
               }

               auto string() -> node
               {
                  validate( '"', pull());

                  std::string value;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"')
                        return value;

                     if( sign == '\\')
                     {
                        switch( const auto sign = pull())
                        {
                        break; case '\\': value.push_back( '\\');
                        break; case '"': value.push_back( '\"');
                        break; case 'b': value.push_back( '\b');
                        break; case 'f': value.push_back( '\f');
                        break; case 'n': value.push_back( '\n');
                        break; case 'r': value.push_back( '\r');
                        break; case 't': value.push_back( '\t');
                        break; case '/': value.push_back( '/');
                        break; case 'u':
                        {
                           using type = std::string::value_type;
                           const auto cp = code();
                           if( cp < 0x80)
                              value.insert( value.end(), { static_cast< type>( cp)});
                           else if( cp < 0x800)
                              value.insert( value.end(), { static_cast< type>( 0xC0 | (( cp >> 6) & 0x1F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                           else if( cp < 0x10000)
                              value.insert( value.end(), { static_cast< type>( 0xE0 | (( cp >> 12) & 0x0F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                           else
                              value.insert( value.end(), { static_cast< type>( 0xF0 | (( cp >> 18) & 0x07)), static_cast< type>( 0x80 | (( cp >> 12) & 0x3F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                        }
                        break; default:
                           throw std::runtime_error{ std::format( "invalid escape character '{}' [0x{:X}]", sign, sign)};
                        }
                     }
                     else [[likely]]
                     {
                        if( sign != std::char_traits< decltype( sign)>::eof()) [[likely]]
                           value.push_back( sign);
                        else
                           throw std::runtime_error{ "unexpected end of stream"};
                     }
                  }
               }

               auto simple() -> node
               {
                  std::string data;

                  auto good = []( const auto sign)
                  {
                     switch( sign)
                     case '.': case '-': case '+': return sign;
                     return std::isalnum( sign) ? sign : decltype( sign){};
                  };

                  for( ; const auto sign = good( pull()); )
                     data.push_back( sign);
                  
                  back();


                  if( data == "null")
                     return node::nothing{};

                  if( data == "true")
                     return true;

                  if( data == "false")
                     return false;

                  {
                     node::integer value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  {
                     node::decimal value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  throw std::runtime_error{ std::format( "unexpected data [{}]", data)};
               }

            private:

               std::istream& stream;

            };

         } // detail

         auto parse( std::istream& stream)
         {
            return detail::parser{ stream}();
         }

         auto parse( std::string_view json)
         {
            std::ispanstream stream{ json};
            return parse( stream);
         }

         namespace bom
         {
            //! ignores possible UTF8-BOM
            auto parse( std::istream& stream)
            {
               std::array< char, 3> data{};

               const auto count  = stream.read( data.data(), data.size()).gcount();

               if( ! std::ranges::equal( data, std::string_view{ "\xEF\xBB\xBF"}))
                  stream.clear(), stream.seekg( 0 - count, std::ios::cur);
               
               return detail::parser{ stream}();
            }

            auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // bom

         namespace detail
         {
            template< std::size_t spaces = 3>
            struct writer
            {
               std::ostream& stream;

               writer( std::ostream& stream) : stream{ stream} {}

               void operator() ( const node::table& node)
               {
                  start( '{');

                  for( const auto& [ name, data] : node)
                  {
                     insert( name);
                     if constexpr( spaces) indent = false;
                     std::visit( *this, data);
                     if constexpr( spaces) indent = true;
                     stream << ',';
                  }

                  if( !node.empty()) regret();

                  close( '}');
               }

               void operator() ( const node::array& node)
               {
                  start( '[');

                  for( const auto& data : node)
                  {
                     insert();
                     std::visit( *this, data);
                     stream << ',';
                  }

                  if( !node.empty()) regret();

                  close( ']');
               }

               void operator() ( const node::nothing& node)
               {
                  stream << "null";
               }

               void operator() ( const node::boolean& node)
               {
                  stream << ( node ? "true" : "false");
               }

               void operator() ( const node::integer& node)
               {
                  stream << node;
               }

               void operator() ( const node::decimal& node)
               {
                  if( std::isnan( node) || std::isinf( node))
                     throw std::invalid_argument{ std::format( "invalid decimal node [{}]", node)};
                  stream << node;
               }

               void operator() ( const node::string& node)
               {
                  stream << '"';

                  for( const auto data : node)
                  {
                     if( std::iscntrl( data))
                     {
                        stream << R"(\u)" << std::format( "{:04x}", data);
                     }
                     else [[likely]]
                     {
                        switch( data)
                        {
                        case '\\':
                        case '\"':
                           stream << "\\";
                        }

                        stream << data;
                     }
                  }

                  stream << '"';
               }

            private:

               void insert()
               {
                  if constexpr( spaces)
                     if( indent)
                        stream << '\n' << std::setw( column * spaces) << "";
                     else
                        indent = true;
               }

               void insert( const std::string::value_type sign)
               {
                  insert();
                  stream << sign;
               }

               void insert( const std::string& name)
               {
                  insert();
                  //stream << std::quoted( name) << ':';
                  stream << '"' << name << '"' << ':';
                  
                  if constexpr( spaces) stream << ' ';
               }

               void regret()
               {
                  stream.seekp( -1, std::ios_base::cur);
               }

               void start( const auto sign)
               {
                  insert( sign);
                  if constexpr( spaces) ++column;
               }

               void close( const auto sign)
               {
                  if constexpr( spaces) --column;
                  insert( sign);
               }

            private:

               char column{};
               bool indent{};

            };

         } // detail


         auto write( const node& node, std::ostream& stream)
         {
            std::visit( detail::writer{ stream}, node);
         }

         auto write( const node& node)
         {
            std::ostringstream stream;
            write( node, stream);
            return std::move( stream).str();
         }

         namespace compact
         {
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( detail::writer< 0>{ stream}, node);
            }

            auto write( const node& node)
            {
               std::ostringstream stream;
               write( node, stream);
               return std::move( stream).str();
            }
         } // compact

      } // json

   } // v1_1_0

} // poly

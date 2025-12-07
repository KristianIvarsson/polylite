//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "node.hpp"
#include "help.hpp"

#include <array>
#include <cmath>
#include <cuchar>
#include <format>
#include <sstream>
#include <charconv>
#include <stdexcept>
#include <spanstream>


namespace poly
{

   inline namespace version
   {
      namespace json
      {
         namespace detail
         {
            struct parser : help::stream::buffer::iterator::parser
            {
               using help::stream::buffer::iterator::parser::parser;
               
               auto operator()()
               {
                  auto nrv = spot();
                  test( std::char_traits< std::istream::char_type>::eof(), peep());
                  return nrv;
               }

            private:

               auto spot() -> node
               {
                  switch( peep())
                  {
                  case '{':
                     return object();
                  case '[':
                     return array();
                  case '"':
                     return string();
                  case 'n': case 't': case 'f':
                     return simple();
                  default:
                     return number();
                  }
               }

               void skip()
               {
                  leap( [] ( const auto sign) { return std::isspace( sign); });
               }

               char pick()
               {
                  while( good() && std::isspace( *mark)) ++mark;
                  return pull();
               }

               char peep()
               {
                  while( good() && std::isspace( *mark)) ++mark;
                  return peek();
               }

               auto object() -> node::object
               {
                  ++mark; // '{'

                  node::object nrv;

                  if( peep() != '}')
                  {
                     while( true)
                     {
                        test( '"', peep());

                        auto name = string();

                        test( ':', pick());

                        nrv.emplace( std::move( name), spot());

                        if( const auto sign = pick(); sign != ',')
                        {
                           test( '}', sign);
                           break;
                        }
                     }
                  }
                  else
                  {
                     ++mark; // '}'
                  }

                  return nrv;
               }

               auto array() -> node::array
               {
                  ++mark; // '['

                  node::array nrv;

                  if( peep() != ']')
                  {
                     while( true)
                     {
                        nrv.emplace_back( spot());

                        if( const auto sign = pick(); sign != ',')
                        {
                           test( ']', sign);
                           break;
                        }
                     }
                  }
                  else
                  {
                     ++mark; // ']'
                  }

                  return nrv;
               }

               // c-style escape sequences
               auto cast() -> std::int32_t
               {
                  switch( pull())
                  {
                  case '\\':return '\\';
                  case '"': return '\"';
                  case 'b': return '\b';
                  case 'f': return '\f';
                  case 'n': return '\n';
                  case 'r': return '\r';
                  case 't': return '\t';
                  case '/': return '/';
                  case 'u': return code();
                  default: [[unlikely]] halt( "invalid escape character");
                  }
               }

               auto string() -> node::string
               {
                  ++mark; // '"'

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"')
                        return nrv;

                     if( sign != '\\') [[likely]]
                     {
                        nrv.push_back( sign);
                     }
                     else
                     {
                        using type = std::string::value_type;
                        if( const auto cp = cast(); cp < 0x80) [[likely]]
                        {
                           nrv.push_back( static_cast< type>( cp));
                        }
                        else if( cp < 0x800)
                        {
                           nrv.push_back( static_cast< type>( 0xC0 | (( cp >> 6) & 0x1F)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                        }
                        else if( cp < 0x10000)
                        {
                           nrv.push_back( static_cast< type>( 0xE0 | (( cp >> 12) & 0x0F)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                        }
                        else
                        {
                           nrv.push_back( static_cast< type>( 0xF0 | (( cp >> 18) & 0x07)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp >> 12) & 0x3F)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)));
                           nrv.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                        }
                     }
                  }
               }

               auto simple() -> node
               {
                  ++mark; // 'n'+, 't', 'f

                  const auto data = read( []( const auto sign) 
                     { 
                        return std::islower( sign); 
                     });

                  if( data == "rue")
                     return true;

                  if( data == "alse")
                     return false;

                  if( data == "ull")
                     return nullptr;

                  [[unlikely]] halt( "unexpected data");
               }

               auto number() -> node
               {
                  bool decimal = false;
                  const auto data = read( [ &decimal]( const auto sign)
                     {
                        switch( sign)
                        case '.': case 'e': case 'E': return decimal = true;
                        return std::isdigit( sign) != 0 || sign == '-';
                     });

                  if( decimal)
                  {
                     node::decimal value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }
                  else
                  {
                     node::integer value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  [[unlikely]] halt( "unexpected data");
               }
            };

         } // detail

         inline auto parse( std::istream& stream)
         {
            return detail::parser{ stream}();
         }

         inline auto parse( std::string_view json)
         {
            std::ispanstream stream{ json};
            return parse( stream);
         }

         namespace bom
         {
            inline auto parse( std::istream& stream)
            {
               return detail::parser{ help::stream::ignore::bom( stream)}();
            }

            inline auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // bom

         namespace detail
         {
            constexpr std::size_t spaces = 3;

            template< std::size_t spaces>
            struct writer : help::stream::buffer::iterator::writer
            {
               using help::stream::buffer::iterator::writer::writer;
               
               void operator() ( const node::object& node)
               {
                  open( '{');

                  auto comma = node.size();
                  for( const auto& [ name, data] : node)
                  {
                     fill( name);
                     if constexpr( spaces) indent = false;
                     std::visit( *this, data);
                     if constexpr( spaces) indent = true;
                     if( --comma) push( ',');
                  }

                  seal( '}');
               }

               void operator() ( const node::array& node)
               {
                  open( '[');

                  auto comma = node.size();
                  for( const auto& data : node)
                  {
                     std::visit( *this, data);
                     if( --comma) push( ',');
                  }

                  seal( ']');
               }

               void operator() ( const node::nothing& node)
               {
                  fill();
                  copy( "null");
               }

               void operator() ( const node::boolean& node)
               {
                  fill();
                  copy( node ? "true" : "false");
               }

               void operator() ( const node::integer& node)
               {
                  fill();
                  copy( std::format( "{}", node));
               }

               void operator() ( const node::decimal& node)
               {
                  if( std::isnan( node) || std::isinf( node))
                     [[unlikely]] throw std::invalid_argument{ std::format( "invalid decimal node [{}]", node)};

                  fill();
                  copy( std::format( "{}", node));
               }

               void operator() ( const node::string& node)
               {
                  fill();
                  push( '"');

                  for( const auto data : node)
                  {
                     if( std::iscntrl( data))
                     {
                        copy( std::format( R"(\u{:04x})", data));
                     }
                     else [[likely]]
                     {
                        switch( data)
                        case '\\': case '\"': push( '\\');
                        push( data);
                     }
                  }

                  push( '"');
               }

            private:

               void fill()
               {
                  if constexpr( spaces)
                     if( indent)
                        push( '\n'), std::fill_n( mark, column * spaces, ' ');
                     else
                        indent = true;
               }

               void fill( const std::string::value_type sign)
               {
                  fill();
                  push( sign);
               }

               void fill( const std::string& name)
               {
                  fill();
                  //copy( '"' + name + '"' + ':');
                  push( '"'); 
                  copy( name);
                  push( '"');
                  push( ':');
                  if constexpr( spaces) push( ' ');
               }

               void open( const auto sign)
               {
                  fill( sign);
                  if constexpr( spaces) ++column;
               }

               void seal( const auto sign)
               {
                  if constexpr( spaces) --column;
                  fill( sign);
               }

            private:

               char column{};
               bool indent{};

            };

         } // detail


         inline namespace elegant
         {
            template< std::size_t spaces = detail::spaces>
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( detail::writer< spaces>{ stream}, node);
            }

            template< std::size_t spaces = detail::spaces>
            auto write( const node& node)
            {
               std::ostringstream stream;
               write< spaces>( node, stream);
               return std::move( stream).str();
            }
         } // elegant

         namespace compact
         {
            inline auto write( const node& node, std::ostream& stream)
            {
               std::visit( detail::writer< 0>{ stream}, node);
            }

            inline auto write( const node& node)
            {
               std::ostringstream stream;
               write( node, stream);
               return std::move( stream).str();
            }
         } // compact

      } // json

   } // version

} // poly

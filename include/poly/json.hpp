//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <cmath>
#include <string>
#include <format>
#include <sstream>
#include <charconv>
#include <stdexcept>
#include <spanstream>
#include <string_view>


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
               using base::base;

               auto operator()()
               {
                  auto nrv = spot();
                  test( std::char_traits< std::istream::char_type>::eof(), peep());
                  return nrv;
               }

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

            private:

               void skip()
               {
                  while( good() && help::is::space( *mark)) ++mark;
               }

               char pick()
               {
                  while( good() && help::is::space( *mark)) ++mark;
                  return pull();
               }

               char peep()
               {
                  while( good() && help::is::space( *mark)) ++mark;
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
                        nrv.push_back( sign);
                     else
                        nrv.append( help::transform::point( cast( pull())));
                  }
               }

               auto simple() -> node
               {
                  ++mark; // 'n', 't', 'f

                  const auto data = read( []( const auto sign) 
                     { 
                        return help::is::lower( sign); 
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
                        return help::is::digit( sign) || sign == '-';
                     });

                  const auto start = data.data();
                  const auto cease = data.data() + data.size();

                  if( decimal)
                  {
                     node::decimal value;
                     const auto result = std::from_chars( start, cease, value);
                     if( result.ec == std::errc{} && result.ptr == cease)
                        return value;
                  }
                  else
                  {
                     node::integer value;
                     const auto result = std::from_chars( start, cease, value);
                     if( result.ec == std::errc{} && result.ptr == cease)
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

         inline auto parse( std::string_view data)
         {
            std::ispanstream stream{ data};
            return parse( stream);
         }


         namespace detail
         {
            constexpr std::size_t spaces = 3;

            template< std::size_t spaces, bool strict>
            struct writer : help::stream::buffer::iterator::writer
            {
               using base::base;
               
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

               void operator() ( const node::instant& node)
               {
                  if constexpr( strict)
                  {
                     halt( "node::instant");
                  }
                  else
                  {
                     fill();
                     push( '"');
                     std::visit( [ this]( const auto& data) { time( data); }, node);
                     push( '"');
                  }
               }

               void operator() ( const node::string& node)
               {
                  fill();
                  push( '"');
                  cast( node);
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

            template< std::size_t spaces, bool strict>
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( writer< spaces, strict>{ stream}, node);
            }

            template< std::size_t spaces, bool strict>
            auto write( const node& node)
            {
               std::ostringstream stream;
               write< spaces, strict>( node, stream);
               return std::move( stream).str();
            }

         } // detail


         inline namespace elegant
         {
            inline namespace strict
            {
               template< std::size_t spaces = detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               template< std::size_t spaces = detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, false>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // elegant

         namespace compact
         {
            inline namespace strict
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, false>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // compact

      } // json

   } // version

} // poly

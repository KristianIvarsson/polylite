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
#include <charconv>


namespace poly_version
{
   namespace json
   {
      namespace detail
      {
         template< help::sign type, help::source_iterator< type> iterator>
         struct parser : help::parser< type, iterator>
         {
            using base = help::parser< type, iterator>;
            using base::mark;
            using base::cast;
            using base::done;
            using base::halt;
            using base::peek;
            using base::pull;
            using base::read;
            using base::skip;
            using base::test;

            auto operator()()
            {
               auto nrv = spot();
               skip(); 
               done();
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

            char pick()
            {
               return skip(), pull();
            }

            char peep()
            {
               return skip(), peek();
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
               const auto data = read( []( const auto sign) 
                  { 
                     return help::is::lower( sign); 
                  });

               if( data == "true")
                  return true;

               if( data == "false")
                  return false;

               if( data == "null")
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
                     return help::is::digit( sign) || sign == '-' || sign == '+';
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

      auto parse( auto&& source)
      {
         return help::make::source< detail::parser>( source)();
      }


      namespace detail
      {
         constexpr std::size_t spaces = 3;

         template< help::sign type, help::target_iterator< type> iterator, std::size_t spaces, bool strict>
         struct writer : help::writer< type, iterator>
         {
            using base = help::writer< type, iterator>;
            using base::push;
            using base::copy;
            using base::cast;
            using base::time;
            using base::flat;
            using base::halt;

            char column{};
            bool indent{};
            
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

            void operator() ( const node::nothing& )
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
                  [[unlikely]] halt( "node::decimal");

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

            void operator() ( const node::binary& node)
            {
               if constexpr( strict)
                  halt( "node::binary");

               fill();
               push( '"');
               flat( node);
               push( '"');
            }

         private:

            void fill()
            {
               if constexpr( spaces)
               {
                  if( indent)
                     base::fold( column * spaces);
                  else
                     indent = true;
               }
            }

            void fill( const std::string::value_type sign)
            {
               fill();
               push( sign);
            }

            void fill( const std::string& name)
            {
               fill();
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

         };

         template< std::size_t spaces, bool strict>
         auto write( const node& root, auto&& target)
         {
            auto sink = help::make::target< writer, spaces, strict>( target);
            std::visit( sink, root);
         }

         // the default write function
         template< std::size_t spaces, bool strict>
         auto write( const node& root)
         {
            std::string target;
            write< spaces, strict>( root, target);
            return target;
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

} // poly_version

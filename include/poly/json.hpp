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
         template< help::byte unit, pipe::source origin>
         struct parser : help::parser< unit, origin>
         {
            using base = help::parser< unit, origin>;
            using base::base;
            using base::mark;
            using base::cast;
            using base::done;
            using base::feed;
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
                  read( help::till::quot, nrv);

                  if( pull() == '"')
                     return nrv;

                  nrv.append( help::transform::point( cast( pull())));
               }
            }

            auto simple() -> node
            {
               switch( pull())
               {
               case 't': test( "rue");  return true;
               case 'f': test( "alse"); return false;
               case 'n': test( "ull");  return nullptr;
               }

               [[unlikely]] halt( "unexpected data");
            }

            auto number() -> node
            {
               auto [ head, tail] = feed.data( 128);

               const auto first = reinterpret_cast< const char*>( head);
               const auto final = reinterpret_cast< const char*>( tail);

               node::integer integer{};

               if( const auto [ stop, code] = std::from_chars( first, final, integer); code == std::errc{})
                  if( stop == final || help::is::none< '.', 'e', 'E'>( *stop))
                     return head += stop - first, integer;

               node::decimal decimal{};

               if( const auto [ stop, code] = std::from_chars( first, final, decimal); code == std::errc{})
                  if( std::isfinite( decimal))
                     return head += stop - first, decimal;

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

         template< help::byte unit, pipe::target output, std::size_t spaces, bool strict>
         struct writer : help::writer< unit, output>
         {
            using base = help::writer< unit, output>;
            using base::base;
            using base::cast;
            using base::copy;
            using base::flat;
            using base::fold;
            using base::halt;
            using base::push;
            using base::time;

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

            void operator() ( const node::nothing)
            {
               fill();
               copy( "null");
            }

            void operator() ( const node::boolean node)
            {
               fill();
               copy( node ? "true" : "false");
            }

            void operator() ( const node::integer node)
            {
               fill();
               copy( std::format( "{}", node));
            }

            void operator() ( const node::decimal node)
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
                  std::visit( [ this]( const auto& data) { this->time( data); }, node);
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
               {
                  halt( "node::binary");
               }
               else
               {
                  fill();
                  push( '"');
                  flat( node);
                  push( '"');
               }
            }

         private:

            void fill()
            {
               if constexpr( spaces)
               {
                  if( indent)
                     fold( column * spaces);
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

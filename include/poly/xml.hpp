//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

namespace poly_version
{
   namespace xml
   {
      namespace detail
      {
         constexpr std::size_t spaces = 3;

         template< help::byte unit, help::target_iterator< unit> iterator, std::size_t spaces>
         struct writer : help::writer< unit, iterator>
         {
            using base = help::writer< unit, iterator>;
            using base::copy;
            using base::emit;
            using base::halt;
            using base::fold;
            using base::push;

            char column{};
            bool indent{};

            void open( const std::string_view name)
            {
               begin( name);
               if constexpr( spaces) ++column;
            }

            void seal( const std::string_view name)
            {
               if constexpr( spaces) --column;
               fill();
               cease( name);
            }

            void begin( const std::string_view name)
            {
               fill();
               push( '<');
               emit( name);
               push( '>');
            }

            void cease( const std::string_view name)
            {
               push( '<');
               push( '/');
               emit( name);
               push( '>');
            }

            void bare( const std::string_view name)
            {
               fill();
               push( '<');
               emit( name);
               push( '/');
               push( '>');
            }

            void cast( const std::string& data)
            {
               for( const auto sign : data)
               {
                  if( help::is::in::range< 0x0, 0x1F>( sign))
                     switch( sign)
                     {
                     case '\t': case '\n': case '\r':
                        break;
                     default:
                        [[unlikely]] halt( "character");
                     }
                  
                  switch( sign)
                  {
                     case '\r':  copy( "&#13;"); break;
                     case '&':   copy( "&amp;"); break;
                     case '<':   copy( "&lt;"); break;
                     case '>':   copy( "&gt;"); break;
                     default:    push( sign); break;
                  }
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

         };

      } // detail

      namespace plist
      {
         namespace detail
         {
            namespace types
            {
               constexpr std::string_view integer  { "integer"};
               constexpr std::string_view decimal  { "real"};
               constexpr std::string_view instant  { "date"};
               constexpr std::string_view string   { "string"};
               constexpr std::string_view binary   { "data"};
               constexpr std::string_view object   { "dict"};
               constexpr std::string_view array    { "array"};

               constexpr std::string_view yes      { "true"};
               constexpr std::string_view no       { "false"};

               constexpr std::string_view name     { "key"};
            }// types


            template< help::byte unit, help::target_iterator< unit> iterator, std::size_t spaces, bool strict>
            struct writer : xml::detail::writer< unit, iterator, spaces>
            {
               using base = xml::detail::writer< unit, iterator, spaces>;
               using base::cast;
               using base::copy;
               using base::emit;
               using base::halt;
               using base::flat;
               using base::open;
               using base::push;
               using base::seal;
               using base::time;
               using base::begin;
               using base::cease;
               using base::bare;

               void operator() ( const node::object& node)
               {
                  if( node.empty())
                  {
                     bare( types::object);
                  }
                  else
                  {
                     open( types::object);

                     for( const auto& [ name, data] : node)
                     {
                        begin( types::name);
                        cast( name);
                        cease( types::name);
                        std::visit( *this, data);
                     }

                     seal( types::object);
                  }
               }

               void operator() ( const node::array& node)
               {
                  if( node.empty())
                  {
                     bare( types::array);
                  }
                  else
                  {
                     open( types::array);

                     for( const auto& data : node)
                        std::visit( *this, data);

                     seal( types::array);
                  }
               }

               void operator() ( const node::nothing)
               {
                  if constexpr( strict)
                  {
                      halt( "node::nothing");
                  }
                  else 
                  {
                     begin( types::string);
                     copy( "null");
                     cease( types::string);
                  }
               }

               void operator() ( const node::boolean node)
               {
                  if( node)
                     bare( types::yes);
                  else
                     bare( types::no);
               }

               void operator() ( const node::integer node)
               {
                  begin( types::integer);
                  emit( std::format( "{}", node));
                  cease( types::integer);
               }

               void operator() ( const node::decimal node)
               {
                  begin( types::decimal);
                  emit( std::format( "{}", node));
                  cease( types::decimal);
               }

               void operator() ( const node::instant& node)
               {
                  std::visit( *this, node);
               }

               void operator() ( const node::string& node)
               {
                  begin( types::string);
                  cast( node);
                  cease( types::string);
               }

               void operator() ( const node::binary& node)
               {
                  begin( types::binary);
                  flat( node);
                  cease( types::binary);
               }


               void operator() ( const node::local_date data)
               {
                  if constexpr( strict) halt( "node::local_date");
                  else date( data);
               }

               void operator() ( const node::local_time data)
               {
                  if constexpr( strict) halt( "node::local_time");
                  else date( data);
               }

               void operator() ( const node::local_datetime data)
               {
                  if constexpr( strict) halt( "node::local_datetime");
                  else date( data);
               }
            
               void operator() ( const node::zoned_datetime data)
               {
                  begin( types::instant);
                  emit( std::format( "{:%FT%TZ}", std::chrono::floor< std::chrono::seconds>( data.get_sys_time())));
                  cease( types::instant);
               }

            private:

               void date( const auto& data)
               {
                  begin( types::string);
                  time( data);
                  cease( types::string);
               }
            };


            template< std::size_t spaces, bool strict, bool complete>
            void write( const node& root, auto&& target)
            {
               auto sink = help::make::target< writer, spaces, strict>( target);

               if constexpr( complete)
               {
                  sink.copy( R"(<?xml version="1.0" encoding="UTF-8"?>)");
                  if constexpr( spaces) sink.push( '\n');
                  sink.copy( R"(<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)");
                  if constexpr( spaces) sink.push( '\n');
                  sink.copy( R"(<plist version="1.0">)");
                  if constexpr( spaces) sink.push( '\n');
               }

               std::visit( sink, root);

               if constexpr( complete)
               {
                  if constexpr( spaces) sink.push( '\n');
                  sink.copy( "</plist>");
               }
            }

            // the default write function
            template< std::size_t spaces, bool strict, bool complete>
            auto write( const node& root)
            {
               std::string target;
               write< spaces, strict, complete>( root, target);
               return target;
            }

         } // detail

         inline namespace elegant
         {
            inline namespace strict
            {
               template< std::size_t spaces = xml::detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, true, false>( std::forward< decltype( parameters)>( parameters)...);
               }

               template< std::size_t spaces = xml::detail::spaces>
               auto whole( auto&&... parameters)
               {
                  return detail::write< spaces, true, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               template< std::size_t spaces = xml::detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, false, false>( std::forward< decltype( parameters)>( parameters)...);
               }

               template< std::size_t spaces = xml::detail::spaces>
               auto whole( auto&&... parameters)
               {
                  return detail::write< spaces, false, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // elegant

         namespace compact
         {
            inline namespace strict
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, true, false>( std::forward< decltype( parameters)>( parameters)...);
               }

               auto whole( auto&&... parameters)
               {
                  return detail::write< 0, true, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, false, false>( std::forward< decltype( parameters)>( parameters)...);
               }

               auto whole( auto&&... parameters)
               {
                  return detail::write< 0, false, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // compact

      } // plist

   } // xml
} // poly_version
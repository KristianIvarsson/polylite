//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <tuple>
#include <string>
#include <optional>

namespace poly_version
{
   namespace xml
   {
      namespace detail
      {
         template< help::byte unit, help::source_iterator< unit> iterator>
         struct parser : help::parser< unit, iterator>
         {
            using base = help::parser< unit, iterator>;
            using base::deny;
            using base::drop;
            using base::good;
            using base::halt;
            using base::look;
            using base::peek;
            using base::pull;
            using base::read;
            using base::skip;
            using base::take;
            using base::test;

            enum class form
            {
               hint,
               kind,
               note,
               lump,
               open,
               seal,
               bare,
               done,
            };

            auto till( const std::string_view stop)
            {
               std::string nrv;
               do
               {
                  read( [ back = stop.back()]( const auto sign) { return sign != back; }, nrv);
                  nrv.push_back( pull());
               }
               while( ! nrv.ends_with( stop));
               nrv.resize( nrv.size() - stop.size());
               return nrv;
            }

            auto till( const help::byte auto stop)
            {
               auto nrv = read( [ stop]( const auto sign) { return sign != stop; });
               pull(); // stop
               return nrv;
            }

            auto next() -> std::tuple< form, std::string>
            {
               skip();

               if( ! good())
                  return { form::done, {}};

               test( '<', take());

               const auto sign = pull();

               if( sign == '?')
                  return { form::hint, help::trim( till( "?>"))};
               
               if( sign == '!')
               {
                  switch( pull())
                  {
                  case '-': test( '-');      return { form::note, till( "-->")};
                  case '[': test( "CDATA["); return { form::lump, till( "]]>")};
                  case 'D': test( "OCTYPE"); return { form::kind, help::trim( till( '>'))};
                  default: [[unlikely]] halt( "unexpected data");
                  }
               }

               if( sign == '/')
                  return { form::seal, help::trim( till( '>'))};

               std::string data = sign + till( '>');

               if( data.ends_with( '/'))
                  return data.pop_back(), std::tuple{ form::bare, help::trim( std::move( data))};

               return { form::open, help::trim( std::move( data))};
            }

            auto sift()
            {
               while( true)
               {
                  auto item = next();

                  const auto type = std::get< form>( item);

                  if( type != form::hint && type != form::note)
                  {
                     if( type == form::lump)
                        [[unlikely]] deny( "CDATA");

                     return item;
                  }
               }
            }

            static auto word( const std::string_view data) -> std::string_view
            {
               const auto last = std::ranges::find_if( data, []( const auto sign){ return help::is::space( sign);});
               return data.substr( 0, last - std::begin( data));
            }

            auto want( const form wish)
            {
               auto [type, data] = sift();

               if( type != wish) [[unlikely]] halt( "unexpected type");

               return std::move( data);
            }

            void want( const form wish, const auto name)
            {
               if( word( want( wish)) != name) [[unlikely]] halt( "unexpected name");
            }

            auto cast( std::string_view data) -> std::string
            {
               if( data == "amp")  return "&";
               if( data == "lt")   return "<";
               if( data == "gt")   return ">";
               if( data == "quot") return "\"";
               if( data == "apos") return "'";

               if( data.starts_with( '#'))
               {
                  data = data.substr( 1);
                  const bool hexa = data.starts_with( 'x');
                  data = data.substr( hexa);

                  help::type::i32 code{};
                  const auto [ ptr, ec] = std::from_chars( data.data(), data.data() + data.size(), code, hexa ? 16 : 10);

                  auto scan = [] ( const help::type::i32 rune)
                     {
                        return help::is::some< 0x9, 0xA, 0xD>( rune)
                           || help::is::in::range< 0x20, 0xD7FF>( rune)
                           || help::is::in::range< 0xE000, 0xFFFD>( rune)
                           || help::is::in::range< 0x10000, 0x10FFFF>( rune);
                     };

                  if( ptr == data.data() + data.size() && ec == std::errc{} && scan( code))
                     return help::transform::point( code);
               }

               [[unlikely]] halt( "invalid entity");
            }

            auto text()
            {
               std::string nrv;

               while( good() && look() != '<')
               {
                  switch( const auto sign = take())
                  {
                  case '&':
                     nrv.append( cast( till( ';')));
                     break;

                  case '\r':
                     if( peek() != '\n')
                        nrv.push_back( '\n');
                     break;

                  case '\t':
                  case '\n':
                     nrv.push_back( sign);
                     break;

                  default:
                     if( help::is::in::range< 0x0, 0x1F>( sign))
                        [[unlikely]] halt( "invalid character");
                     nrv.push_back( sign);
                  }
               }

               return nrv;
            }

            //! @return the raw value of the attribute, or nullopt if absent or malformed
            static auto slot( std::string_view data, const std::string_view name) -> std::optional< std::string_view>
            {
               auto part = [ &data]( const std::string_view signs) -> std::optional< std::string_view>
               {
                  const auto spot = data.find_first_of( signs);
                  if( spot == std::string_view::npos)
                     return std::nullopt;

                  const auto nrv = data.substr( 0, spot);
                  data.remove_prefix( spot + 1);
                  return nrv;
               };

               data.remove_prefix( word( data).size());

               while( true)
               {
                  auto view = part( "=");
                  if( ! view)
                     return std::nullopt;

                  while( ! view->empty() && help::is::space( view->front())) view->remove_prefix( 1);
                  while( ! view->empty() && help::is::space( view->back())) view->remove_suffix( 1);

                  if( ! part( "\"'"))
                     return std::nullopt;

                  // value ends at the same kind of sign that opened it
                  const auto crop = part( { data.data() - 1, 1});
                  if( ! crop || view == name)
                     return crop;
               }
            }

            auto head()
            {
               auto [ type, data] = next();

               if( type == form::hint && word( data) == "xml")
               {
                  auto equal = [] ( const std::string_view lhs, const std::string_view rhs)
                  {
                     return std::ranges::equal( lhs, rhs, [] ( const auto first, const auto other) 
                     { 
                        return help::to::lower( first) == help::to::lower( other); 
                     });
                  };

                  if( const auto value = slot( data, "version"); value && ! equal( *value, "1.0"))
                     [[unlikely]] halt( "unsupported xml version");

                  if( const auto value = slot( data, "encoding"); value && ! equal( *value, "UTF-8"))
                     [[unlikely]] halt( "unsupported xml encoding");
               }

               if( type == form::hint || type == form::note)
                  std::tie( type, data) = sift();               

               return std::make_tuple( type, std::move( data));
            }

         };

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

               constexpr std::string_view root     { "plist"};
            }// types

         } // detail


         namespace detail
         {
            template< help::byte unit, help::source_iterator< unit> iterator>
            struct parser : xml::detail::parser< unit, iterator>
            {
               using base = xml::detail::parser< unit, iterator>;
               using base::halt;
               using base::head;
               using base::word;
               using base::sift;
               using base::slot;
               using base::text;
               using base::want;

               using typename base::form;

               auto operator()()
               {
                  auto [ type, data] = head();

                  // skipping potential doctype
                  if( type == form::kind)
                     std::tie( type, data) = sift();

                  const auto root = type == form::open && word( data) == types::root;

                  if( root)
                     if( const auto value = slot( data, "version"); value && value != "1.0")
                        [[unlikely]] halt( "unsupported plist version");

                  if( root)
                     std::tie( type, data) = sift();

                  auto nrv = spot( type, data);

                  if( root)
                     want( form::seal, types::root);

                  want( form::done);

                  return nrv;
               }

            private:

               auto leaf( const std::string_view name, const bool bare) -> std::string
               {
                  if( bare)
                     return {};

                  auto data = text();
                  want( form::seal, name);
                  return data;
               }

               node spot( const form type, const std::string_view data)
               {
                  if( type != form::open && type != form::bare)
                     [[unlikely]] halt( "expected element");

                  const auto name = word( data);
                  const bool bare = type == form::bare;

                  if( name == types::object)  return bare ? node::object{} : object();
                  if( name == types::array)   return bare ? node::array{}  : array();
                  if( name == types::yes)     return leaf( name, bare), true;
                  if( name == types::no)      return leaf( name, bare), false;
                  if( name == types::string)  return leaf( name, bare);
                  if( name == types::integer) return integer( leaf( name, bare));
                  if( name == types::decimal) return decimal( leaf( name, bare));
                  if( name == types::instant) return instant( leaf( name, bare));
                  if( name == types::binary)  return binary( leaf( name, bare));

                  [[unlikely]] halt( "unknown element");
               }

               auto object()
               {
                  node::object nrv;

                  while( true)
                  {
                     auto [type, data] = sift();

                     if( type == form::seal && word( data) == types::object)
                        return nrv;

                     if( type != form::open || word( data) != types::name)
                        [[unlikely]] halt( "unexpected element");

                     auto name = text();
                     want( form::seal, types::name);
                     std::tie( type, data) = sift();
                     nrv.emplace( std::move( name), spot( type, data));
                  }
               }

               auto array()
               {
                  node::array nrv;

                  while( true)
                  {
                     auto [type, data] = sift();

                     if( type == form::seal && word( data) == types::array)
                        return nrv;

                     nrv.push_back( spot( type, data));
                  }
               }

               auto integer( const std::string_view data)
               {
                  if( auto result = help::transform::number( data); result && result->is_integer())
                     return std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto decimal( const std::string_view data)
               {
                  if( auto result = help::transform::number( data); result && result->is_numeric())
                     return result->is_integer() ? node{ static_cast< node::decimal>( result->as_integer())} : std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto instant( const std::string_view data)
               {
                  if( auto result = help::transform::instant( data))
                     if( std::holds_alternative< node::zoned_datetime>( *result))
                        return std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto binary( const std::string_view data)
               {
                  if( auto result = help::transform::binary( data); result)
                     return std::move( *result);

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
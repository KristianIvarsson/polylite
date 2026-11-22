//
// Copyright (c) 2026 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "core/heap.hpp"
#include "core/help.hpp"
#include "core/lint.hpp"
#include "core/load.hpp"
#include "core/save.hpp"

#include <tuple>
#include <string>
#include <optional>

namespace poly_version
{
   namespace xml
   {
      namespace detail
      {
         namespace basic
         {
            template< core::type::byte unit, core::load::pipe::source origin>
            struct parser : core::load::parser< unit, origin>
            {
               using base = core::load::parser< unit, origin>;
               using base::base;
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

               auto till( const core::type::byte auto stop)
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
                     return { form::hint, core::help::trim( till( "?>"))};
               
                  if( sign == '!')
                  {
                     switch( pull())
                     {
                     case '-': test( '-');      return { form::note, till( "-->")};
                     case '[': test( "CDATA["); return { form::lump, till( "]]>")};
                     case 'D': test( "OCTYPE"); return { form::kind, core::help::trim( till( '>'))};
                     default: [[unlikely]] halt( "unexpected data");
                     }
                  }

                  if( sign == '/')
                     return { form::seal, core::help::trim( till( '>'))};

                  std::string data = sign + till( '>');

                  if( data.ends_with( '/'))
                     return data.pop_back(), std::tuple{ form::bare, core::help::trim( std::move( data))};

                  return { form::open, core::help::trim( std::move( data))};
               }

               auto sift()
               {
                  while( true)
                  {
                     auto item = next();

                     switch( std::get< form>( item))
                     {
                     case form::hint:
                        scan( std::get< std::string>( item));
                     [[fallthrough]];
                     case form::note:
                     case form::kind:
                        break;
                     case form::lump:
                        [[unlikely]] deny( "CDATA");
                     default:
                        return item;
                     }
                  }
               }

               static auto word( const std::string_view data) -> std::string_view
               {
                  const auto last = std::ranges::find_if( data, []( const auto sign){ return core::help::is::white( sign);});
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

                     core::type::i32 code{};
                     const auto [ ptr, ec] = std::from_chars( data.data(), data.data() + data.size(), code, hexa ? 16 : 10);

                     auto scan = [] ( const core::type::i32 rune)
                        {
                           return core::help::is::some< '\t', '\n', '\r'>( rune)
                              || core::help::is::in::range< 0x20, 0xD7FF>( rune)
                              || core::help::is::in::range< 0xE000, 0xFFFD>( rune)
                              || core::help::is::in::range< 0x10000, 0x10FFFF>( rune);
                        };

                     if( ptr == data.data() + data.size() && ec == std::errc{} && scan( code))
                        return core::help::transform::point( code);
                  }

                  [[unlikely]] halt( "invalid entity");
               }

               auto text()
               {
                  std::string nrv;

                  while( true)
                  {
                     constexpr auto stop = [] ( const auto sign)
                     {
                        return core::help::is::in::range< 0x0, 0x1F>( sign) ?
                           core::help::is::some< '\t', '\n'>( sign) :
                           core::help::is::none< '<', '&', '\r'>( sign);
                     };

                     read( stop, nrv);

                     if( peek() == '<')
                        return nrv;

                     switch( pull())
                     {
                     case '&':
                        nrv.append( cast( till( ';')));
                        break;
                     case '\r':
                        if( peek() != '\n')
                           nrv.push_back( '\n');
                        break;
                     default:
                        [[unlikely]] halt( "invalid character");
                     }
                  }
               }
               
               //! @return the raw value of the attribute or nullopt if absent or malformed
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

                     while( ! view->empty() && core::help::is::white( view->front())) view->remove_prefix( 1);
                     while( ! view->empty() && core::help::is::white( view->back())) view->remove_suffix( 1);

                     if( ! part( "\"'"))
                        return std::nullopt;

                     // value ends at the same kind of sign that opened it
                     const auto crop = part( { data.data() - 1, 1});
                     if( ! crop || view == name)
                        return crop;
                  }
               }

               void scan( const std::string_view data) const
               {
                  if( word( data) == "xml")
                  {
                     auto equal = [] ( const std::string_view lhs, const std::string_view rhs)
                     {
                        return std::ranges::equal( lhs, rhs, [] ( const auto first, const auto other) 
                        { 
                           return core::help::to::lower( first) == core::help::to::lower( other); 
                        });
                     };

                     if( const auto value = slot( data, "version"); value && ! equal( *value, "1.0"))
                        [[unlikely]] halt( "unsupported xml version");

                     if( const auto value = slot( data, "encoding"); value && ! equal( *value, "UTF-8"))
                        [[unlikely]] halt( "unsupported xml encoding");
                  }
               }

               auto lead()
               {
                  auto [ type, data] = next();

                  switch( type)
                  {
                  case form::hint:
                     scan( data);
                  [[fallthrough]];
                  case form::note:
                     std::tie( type, data) = sift();
                  }

                  return std::make_tuple( type, std::move( data));
               }
            };
         } // basic

         template< core::type::byte unit, core::load::pipe::source origin>
         struct parser : basic::parser< unit, origin>
         {
            using base = basic::parser< unit, origin>;
            using base::base;
            using base::halt;
            using base::sift;
            using base::text;
            using base::word;

            using typename base::form;

            node operator()()
            {
               node::object nrv;

               auto [ type, data] = sift();
               while( type != form::done)
               {
                  nrv.emplace( word( data), spot( type, data));
                  std::tie( type, data) = sift();
               }

               return nrv;
            }

         private:

            core::heap< node::object> pair{};

            node spot( const form type, const std::string_view data)
            {
               switch( type)
               {
               case form::open:
                  return object( word( data));
               case form::bare:
                  return scalar( {});
               default:
                  [[unlikely]] halt( "expected element");
               }
            }

            node object( const std::string_view name)
            {
               const auto from = pair.base();
               
               while( true)
               {
                  auto item = text();

                  const auto [ type, data] = sift();

                  if( type == form::seal)
                  {
                     if( word( data) != name)
                        [[unlikely]] halt( "unexpected name");

                     if( pair.base() == from) // empty
                         return scalar( std::move( item));
                  }

                  if( std::ranges::all_of( item, [] ( const auto sign) { return core::help::is::white( sign); }))
                     if( type == form::seal)
                        return pair.seal( from);
                     else
                        pair.push( word( data), spot( type, data));
                  else
                     [[unlikely]] halt( "mixed content");
               }
            }

            static node scalar( std::string data)
            {
               // if( data.empty())
               //    return nullptr;
               // if( auto item = core::help::transform::simple( data))
               //    return std::move( *item);
               // if( auto item = core::help::transform::number( data))
               //    return std::move( *item);
               // if( auto item = core::help::transform::instant( data))
               //    return std::move( *item);

               return data;
            }
         };
      } // detail

      auto parse( auto&& source)
      {
         return core::load::make::source< detail::parser>( source)();
      }

      namespace detail
      {
         constexpr std::size_t spaces = 3;

         namespace basic
         {
            template< core::type::byte unit, core::save::pipe::target output, std::size_t spaces>
            struct writer : core::save::writer< unit, output>
            {
               using base = core::save::writer< unit, output>;
               using base::base;
               using base::copy;
               using base::emit;
               using base::flat;
               using base::halt;
               using base::fold;
               using base::push;
               using base::time;

               char column{};
               bool indent{};

               void open( const std::string_view name)
               {
                  head( name);
                  if constexpr( spaces) ++column;
               }

               void seal( const std::string_view name)
               {
                  if constexpr( spaces) --column;
                  fill();
                  tail( name);
               }

               void head( const std::string_view name)
               {
                  fill();
                  push( '<');
                  emit( name);
                  push( '>');
               }

               void tail( const std::string_view name)
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
                     if( core::help::is::in::range< 0x0, 0x1F>( sign))
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

            protected:

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
         } // basic

         template< core::type::byte unit, core::save::pipe::target output, std::size_t spaces>
         struct writer : basic::writer< unit, output, spaces>
         {
            using base = basic::writer< unit, output, spaces>;
            using base::bare;
            using base::base;
            using base::cast;
            using base::copy;
            using base::fill;
            using base::flat;
            using base::open;
            using base::push;
            using base::seal;
            using base::tail;
            using base::time;
            using base::column;

            void operator() ( const node& node)
            {
               if( node.is_object())
                  (*this)( node.as_object());
               else
                  // create an object
                  (*this)( node::object{ { { "root"}, node}});
            }

            static bool null( const node::nothing&)
            {
               return true;
            }

            static bool null( const auto& data)
            {
               if constexpr( requires { data.empty(); })
                  return data.empty();
               else
                  return false;
            }

            void operator() ( const node::object& node)
            {
               for( const auto& [ name, data] : node)
               {
                  if( std::visit( []( const auto& item) { return null( item); }, data))
                  {
                     bare( name);
                  }
                  else
                  {
                     open( name);

                     std::visit( *this, data);

                     if( data.is_scalar())
                     {
                        if constexpr( spaces) --column;
                        tail( name);
                     }
                     else
                     {
                        seal( name);
                     }
                  }
               }
            }

            void operator() ( const node::array& node)
            {
               if constexpr( spaces)
               {
                  for( const auto& item : node)
                  {
                     if( item.is_scalar())
                        fill();

                     std::visit( *this, item);
                  }
               }
               else
               {
                  bool first = true;

                  for( const auto& item : node)
                  {
                     if( first)
                        first = false;
                     else
                        push( ' ');

                     std::visit( *this, item);
                  }
               }
            }

            void operator() ( const node::nothing&)
            {

            }

            void operator() ( const node::instant& node)
            {
               std::visit( [ this]( const auto& data) { this->time( data); }, node);
            }

            void operator() ( const node::binary& node)
            {
               flat( node);
            }

            void operator() ( const node::string& node)
            {
               cast( node);
            }

            void operator() ( const auto& node)
            {
               copy( std::format( "{}", node));
            }
         };

         template< std::size_t spaces>
         auto write( const node& root, auto&& target)
         {
            core::save::make::target< writer, spaces>( target)( root);
         }

         // the default write function
         template< std::size_t spaces>
         auto write( const node& root)
         {
            std::string target;
            write< spaces>( root, target);
            return target;
         }
      } // detail

      inline namespace elegant
      {
         template< std::size_t spaces = detail::spaces>
         auto write( auto&&... parameters)
         {
            return detail::write< spaces>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // elegant

      namespace compact
      {
         auto write( auto&&... parameters)
         {
            return detail::write< 0>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // compact

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
            template< core::type::byte unit, core::load::pipe::source origin>
            struct parser : xml::detail::basic::parser< unit, origin>
            {
               using base = xml::detail::basic::parser< unit, origin>;
               using base::base;
               using base::halt;
               using base::lead;
               using base::sift;
               using base::slot;
               using base::text;
               using base::want;
               using base::word;

               using typename base::form;

               auto operator()()
               {
                  auto [ type, data] = sift();

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

               core::heap< node::object>  pair{};
               core::heap< node::array>   item{};


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
                  const auto from = pair.base();
                  
                  while( true)
                  {
                     auto [type, data] = sift();

                     if( type == form::seal && word( data) == types::object)
                        return pair.seal( from);

                     if( type != form::open || word( data) != types::name)
                        [[unlikely]] halt( "unexpected element");

                     auto name = text();
                     want( form::seal, types::name);
                     std::tie( type, data) = sift();
                     pair.push( std::move( name), spot( type, data));
                  }
               }

               auto array()
               {
                  const auto from = item.base();
                  
                  while( true)
                  {
                     auto [type, data] = sift();

                     if( type == form::seal && word( data) == types::array)
                        return item.seal( from);

                     item.push( spot( type, data));
                  }
               }

               auto integer( const std::string_view data)
               {
                  if( auto result = core::help::transform::number( data); result && result->is_integer())
                     return std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto decimal( const std::string_view data)
               {
                  if( auto result = core::help::transform::number( data); result && result->is_numeric())
                     return result->is_integer() ? node{ static_cast< node::decimal>( result->as_integer())} : std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto instant( const std::string_view data)
               {
                  if( auto result = core::help::transform::instant( data))
                     if( std::holds_alternative< node::zoned_datetime>( *result))
                        return std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }

               auto binary( const std::string_view data)
               {
                  if( auto result = core::help::transform::binary( data); result)
                     return std::move( *result);

                  [[unlikely]] halt( "unexpected data");
               }
               
            };

         } // detail

         auto parse( auto&& source)
         {
            return core::load::make::source< detail::parser>( source)();
         }

         namespace detail
         {

            template< core::type::byte unit, core::save::pipe::target output, std::size_t spaces>
            struct writer : xml::detail::basic::writer< unit, output, spaces>
            {
               using base = xml::detail::basic::writer< unit, output, spaces>;
               using base::bare;
               using base::base;
               using base::cast;
               using base::copy;
               using base::emit;
               using base::halt;
               using base::head;
               using base::flat;
               using base::open;
               using base::push;
               using base::seal;
               using base::tail;
               using base::time;

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
                        head( types::name);
                        cast( name);
                        tail( types::name);
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
                  head( types::string);
                  copy( "null");
                  tail( types::string);
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
                  head( types::integer);
                  emit( std::format( "{}", node));
                  tail( types::integer);
               }

               void operator() ( const node::decimal node)
               {
                  head( types::decimal);
                  emit( std::format( "{}", node));
                  tail( types::decimal);
               }

               void operator() ( const node::instant& node)
               {
                  std::visit( *this, node);
               }

               void operator() ( const node::string& node)
               {
                  head( types::string);
                  cast( node);
                  tail( types::string);
               }

               void operator() ( const node::binary& node)
               {
                  head( types::binary);
                  flat( node);
                  tail( types::binary);
               }

               void operator() ( const node::local_date data)
               {
                  date( data);
               }

               void operator() ( const node::local_time data)
               {
                  date( data);
               }

               void operator() ( const node::local_datetime data)
               {
                  date( data);
               }
            
               void operator() ( const node::zoned_datetime data)
               {
                  head( types::instant);
                  emit( std::format( "{:%FT%TZ}", std::chrono::floor< std::chrono::seconds>( data.get_sys_time())));
                  tail( types::instant);
               }

            private:

               void date( const auto& data)
               {
                  head( types::string);
                  time( data);
                  tail( types::string);
               }
            };


            template< std::size_t spaces, bool complete>
            void write( const node& root, auto&& target)
            {
               auto sink = core::save::make::target< writer, spaces>( target);

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
            template< std::size_t spaces, bool complete>
            auto write( const node& root)
            {
               std::string target;
               write< spaces, complete>( root, target);
               return target;
            }

         } // detail

         inline namespace elegant
         {
            template< std::size_t spaces = xml::detail::spaces>
            auto write( auto&&... parameters)
            {
               return detail::write< spaces, false>( std::forward< decltype( parameters)>( parameters)...);
            }

            template< std::size_t spaces = xml::detail::spaces>
            auto whole( auto&&... parameters)
            {
               return detail::write< spaces, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // elegant

         namespace compact
         {
            auto write( auto&&... parameters)
            {
               return detail::write< 0, false>( std::forward< decltype( parameters)>( parameters)...);
            }

            auto whole( auto&&... parameters)
            {
               return detail::write< 0, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // compact

         namespace detail
         {
            struct rule : core::lint::rule
            {
               template< typename type>
               static constexpr bool block = core::type::some< type, node::nothing, node::local_date, node::local_time, node::local_datetime>;
            };
         } // detail

         decltype( auto) check( auto&& data)
         {
            return core::lint::test< detail::rule>( std::forward< decltype( data)>( data));
         }
      } // plist

      namespace detail
      {
         inline constexpr auto name_start_character = [](const auto sign)
         {
            return 
               core::help::is::alpha( sign) or 
               core::help::is::some< ':', '_'>( sign);
         };

         inline constexpr auto name_character = [](const auto sign)
         {
            return name_start_character( sign) or
               core::help::is::digit( sign) or
               core::help::is::some< '-', '.'>( sign);
         };
         
         struct rule : core::lint::rule
         {
            template< typename type>
            static constexpr bool block = core::type::some< type, node::array>;
            static constexpr bool multi = true;
            static constexpr bool table = true;
            //! just a naive ascii check that doesn't care about unicode
            static constexpr auto valid = [](const std::string& name) 
            {  
               if( name.empty()) return false;
               if( ! name_start_character( name.front())) return false;
               return std::ranges::all_of( std::next( std::begin( name)), std::end( name), name_character);
            };
         };
      } // detail

      decltype( auto) check( auto&& data)
      {
         return core::lint::test< detail::rule>( std::forward< decltype( data)>( data));
      }
   } // xml
} // poly_version
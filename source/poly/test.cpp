#include "poly/json.hpp"
#include "poly/toml.hpp"
#include "poly/tool.hpp"

#include <stdexcept>
#include <cassert>
#include <fstream>
#include <print>

namespace poly
{
   inline namespace version
   {
      namespace base::test
      {
         namespace cases
         {
            void access()
            {
               node source;

               source[ "aaa"] = nullptr;
               source[ "bbb"] = true;
               source[ "ccc"] = -42;
               source[ "ddd"] = 3.14;
               source[ "eee"] = "qwerty";
               source[ "fff"][ 0] = 123456;
               source[ "ggg"][ "xxx"] = 123;
               source[ "ggg"][ "yyy"] = 456;
               source[ "ggg"][ "zzz"] = 789;

               assert( source[ "aaa"].as_nothing() == nullptr);
               assert( source[ "bbb"].as_boolean() == true);
               assert( source[ "ccc"].as_integer() == -42);
               assert( source[ "ddd"].as_decimal() == 3.14);
               assert( source[ "eee"].as_string() == "qwerty");
               assert( ! source[ "fff"].as_array().empty());
               assert( ! source[ "ggg"].as_table().empty());

               assert( source[ "ggg"][ "xxx"].is_numeric());
               assert( source[ "ggg"][ "yyy"].is_numeric());
               assert( source[ "ggg"][ "zzz"].is_numeric());

               assert( source[ "aaa"].is_null());
               assert( source[ "bbb"].is_true());
               assert( ! source[ "bbb"].is_false());
               assert( source[ "ccc"].is_numeric());
               assert( source[ "ddd"].is_numeric());
               assert( source[ "eee"].is_scalar());

               assert( source.at( "ggg").at( "yyy").as_integer() == 456);
               assert( source.at( "fff").at( 0).as_integer() == 123456);

               assert( source( "fff")( 0)->to_integer() != nullptr);
               assert( source( "ggg")( "yyy")->to_integer() != nullptr);
            }
         } // cases

         void all()
         {
            cases::access();
         }
      } // base::test

      namespace tool::test
      {
         namespace cases
         {
            void bom()
            {
               assert( json::parse( tool::bom::ignore( "\xEF\xBB\xBF{}")).is_object());
               assert( json::parse( tool::bom::ignore( "{}")).is_object());
            }
         } // cases

         void all()
         {
            cases::bom();
         }
      } // tool::test

      namespace json::test
      {
         namespace cases
         {
            namespace detail
            {
               auto roundtrip( const node& node)
               {
                  const auto source = write( node);
                  const auto target = write( parse( source));
                  assert( source == target);
               }
            } // detail

            void roundtrip()
            {
               detail::roundtrip( nullptr);
               detail::roundtrip( true);
               detail::roundtrip( false);
               detail::roundtrip( 42);
               detail::roundtrip( 3.14);
               detail::roundtrip( " \\ hello \n\u0007\t world \\ ");

               node source;
               source[ "aaa"] = nullptr;
               source[ "bbb"] = true;
               source[ "ccc"] = -42;
               source[ "ddd"] = 3.14;
               source[ "eee"] = "qwerty";
               source[ "fff"][ 0] = 123456;
               source[ "ggg"][ "xxx"] = 123;
               source[ "ggg"][ "yyy"] = 456;
               source[ "ggg"][ "zzz"] = 789;
               detail::roundtrip( source);
            }

            void direct()
            {
               assert( json::parse( "null").as_nothing() == nullptr);
               assert( json::parse( "true").as_boolean() == true);
               assert( json::parse( "false").as_boolean() == false);
               assert( json::parse( "-42").as_integer() == -42);
               assert( json::parse( "3.14").as_decimal() == 3.14);
            }

            void string()
            {
               assert( json::parse( R"("\b")").as_string() == "\b");
               assert( json::parse( R"("\\")").as_string() == "\\");
               assert( json::parse( R"("\n")").as_string() == "\n");


               assert( json::parse( R"("\u0041")").as_string() == "\x41");
               assert( json::parse( R"("\u00A3")").as_string() == "\xC2\xA3");
               assert( json::parse( R"("\u20AC")").as_string() == "\xE2\x82\xAC");
               assert( json::parse( R"("\uD83D\uDE00")").as_string() == "\xF0\x9F\x98\x80");
            }

         } // cases

         void all()
         {
            cases::roundtrip();
            cases::direct();
            cases::string();
         }
      } // json::test

      namespace toml::test
      {
         namespace cases
         {
            void parse()
            {
               const auto source = R"(
# this is a TOML document
key = "value"
bare."quoted" = 'bare with quoted'
bare-key = 'bare key 1'
bare_key = 'bare key 2'
"quoted key".bare = "quoted with bare"
"" = "blank"
[a.b.c] # just a comment 
# just another comment
boolean = true
integer = 42
decimal = 3.14
42s = [ -42, +0x2A, -0o52, +0b101010]
not_a_number = NaN
infinite = Inf
'big integer' = 123_456_789
"multiline basic" = """
this is a
   " multi line"" \

   basic 
   """""""
multiline_literal = '''
   multi ' line
   literal
'''
[[ numbers ]]
small.decimal = -3.14
small.integer = -42
[[ numbers ]]
small.decimal = 3.14
small.integer = 42
[inline]
table = { a = 1, b = 2, c = 3, d.f.g = true }
[unicode]
arrow = "\u2192"
smile = "\uD83D\uDE00"
earth = "\U0001F30D"

)";

               const poly::node table = toml::parse( source);

               assert( table.at( "key").as_string() == "value");
               assert( table.at( "bare").at( "quoted").as_string() == "bare with quoted");
               assert( table.at( "bare-key").as_string() == "bare key 1");
               assert( table.at( "bare_key").as_string() == "bare key 2");
               assert( table.at( "quoted key").at( "bare").as_string() == "quoted with bare");
               assert( table.at( "").as_string() == "blank");
               const auto c = table( "a")( "b")( "c");
               assert( c( "boolean")->as_boolean() == true);
               assert( c( "integer")->as_integer() == 42);
               assert( c( "decimal")->as_decimal() == 3.14);
               assert( c( "42s")->as_array().back().as_integer() == 42);
               assert( std::isnan( c( "not_a_number")->as_decimal()));
               assert( std::isinf( c( "infinite")->as_decimal()));
               assert( c( "multiline basic")->as_string() == "this is a\n   \" multi line\"\" basic \n   \"\"\"\"");
               assert( c( "multiline_literal")->as_string() == "multi ' line\n   literal\n");
               assert( table( "numbers")( 0)( "small")( "decimal")->as_decimal() == -3.14);
               assert( table( "numbers")( 0)( "small")( "integer")->as_integer() == -42);
               assert( table( "numbers")( 1)( "small")( "decimal")->as_decimal() == 3.14);
               assert( table( "numbers")( 1)( "small")( "integer")->as_integer() == 42);
               assert( table( "inline")( "table")( "d")( "f")( "g")->as_boolean() == true);
               assert( table( "unicode")( "arrow")->as_string().size() == 3);
               assert( table( "unicode")( "smile")->as_string().size() == 4);
               assert( table( "unicode")( "earth")->as_string().size() == 4);
            }
         } // cases

         void all()
         {
            cases::parse();
         }
      } // toml::test

   } // version
} // poly


int main( const int argc, const char* const argv[])
{
   try
   {
      if( argc > 1)
      {
         for( int idx = 1; idx < argc; ++idx)
         {
            if( std::ifstream file{ argv[ idx]})
               poly::json::parse( file);
            else
               throw std::runtime_error{ std::format( "failed to open file [{}]", argv[ idx])};
         }
      }
      else
      {
         poly::base::test::all();
         poly::tool::test::all();
         poly::json::test::all();
         poly::toml::test::all();
      }

      return 0;
   }
   catch( const std::exception& e)
   {
      std::println( stderr, "{}", e.what());
      return 1;
   }
}

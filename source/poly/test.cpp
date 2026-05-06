#include "poly/help.hpp"
#include "poly/json.hpp"
#include "poly/toml.hpp"
#include "poly/tool.hpp"
#include "poly/yaml.hpp"

#include <stdexcept>
#include <cassert>
#include <fstream>
#include <print>

namespace poly
{
   inline namespace version
   {
      namespace help::test
      {
         namespace cases
         {
            void space()
            {
               // all std::isspace characters
               assert( help::is::space( ' '));
               assert( help::is::space( '\t'));
               assert( help::is::space( '\n'));
               assert( help::is::space( '\v'));
               assert( help::is::space( '\f'));
               assert( help::is::space( '\r'));

               // non-space characters
               assert( ! help::is::space( 'a'));
               assert( ! help::is::space( '0'));
               assert( ! help::is::space( '\0'));
               assert( ! help::is::space( '!'));
            }

            void classify()
            {
               assert( help::is::digit( '0'));
               assert( help::is::digit( '9'));
               assert( ! help::is::digit( 'a'));

               assert( help::is::lower( 'a'));
               assert( help::is::lower( 'z'));
               assert( ! help::is::lower( 'A'));

               assert( help::is::upper( 'A'));
               assert( help::is::upper( 'Z'));
               assert( ! help::is::upper( 'a'));

               assert( help::is::alpha( 'a'));
               assert( help::is::alpha( 'Z'));
               assert( ! help::is::alpha( '0'));

               assert( help::is::alnum( 'a'));
               assert( help::is::alnum( '9'));
               assert( ! help::is::alnum( '_'));

               assert( help::is::xdigit( '0'));
               assert( help::is::xdigit( 'f'));
               assert( help::is::xdigit( 'F'));
               assert( ! help::is::xdigit( 'g'));

               assert( help::is::cntrl( '\0'));
               assert( help::is::cntrl( '\n'));
               assert( help::is::cntrl( 0x7F));
               assert( ! help::is::cntrl( ' '));
               assert( ! help::is::cntrl( 'a'));
            }
         } // cases

         void all()
         {
            cases::space();
            cases::classify();
         }
      } // help::test

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
               assert( ! source[ "ggg"].as_object().empty());

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
            void binary()
            {
               // basic decode
               const node::binary hello{ std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F}};
               const node::binary man{ std::byte{0x4D}, std::byte{0x61}, std::byte{0x6E}};

               assert( help::transform::binary( "SGVsbG8=") == hello);
               assert( help::transform::binary( "TWFu") == man);
               assert( help::transform::binary( "") == node::binary{});

               // whitespace tolerance
               assert( help::transform::binary( "SGVs\nbG8=") == hello);
               assert( help::transform::binary( "SGVs bG8=") == hello);

               // invalid character
               assert( ! help::transform::binary( "SGVs!G8="));
            }
         } // cases

         void all()
         {
            cases::access();
            cases::binary();
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

               assert( json::parse( R"("\f")").as_string() == "\f");
               assert( json::parse( R"("\r")").as_string() == "\r");
               assert( json::parse( R"("\t")").as_string() == "\t");
               assert( json::parse( R"("\/")").as_string() == "/");
            }

            void strict_gentle()
            {
               node source;
               source[ "t"] = help::transform::instant( "2025-01-01").value();

               // strict: throws on instant
               try { json::write( source); assert( false); }
               catch( const std::invalid_argument&) {}

               // gentle: writes instant as quoted string
               const auto text = json::elegant::gentle::write( source);
               auto parsed = json::parse( text);
               assert( parsed.at( "t").is_string());
            }

            void binary()
            {
               const node::binary hello{ std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F}};

               // strict: throws on binary
               { node source; source[ "b"] = hello;
                 auto ok = false; try { json::write( source); } catch( const std::invalid_argument&) { ok = true; } assert( ok); }

               // gentle write contains base64
               { node source; source[ "b"] = hello;
                 const auto text = json::elegant::gentle::write( source);
                 assert( text.contains( "SGVsbG8="));

                 // round-trip via manual decode
                 auto parsed = json::parse( text);
                 const auto decoded = help::transform::binary( parsed.at( "b").as_string());
                 assert( decoded && *decoded == hello); }
            }

         } // cases

         void all()
         {
            cases::roundtrip();
            cases::direct();
            cases::string();
            cases::strict_gentle();
            cases::binary();
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

            void elegant()
            {
               // underscore grouping for large integers
               {
                  node source;
                  source[ "n"] = 1234567890L;
                  assert( toml::write( source) == "n = 1_234_567_890\n");
               }

               // single-quote strings when no single-quote or control chars
               {
                  node source;
                  source[ "a"] = "hello world";
                  source[ "b"] = "say \"hi\"";
                  source[ "c"] = "line\none";
                  const auto text = toml::write( source);
                  assert( text.contains( "a = 'hello world'"));
                  assert( text.contains( "b = 'say \"hi\"'"));
                  assert( text.contains( "c = \"line\\none\""));
               }

               // inline tables for shallow array of objects — compact only
               {
                  node source;
                  source[ "points"][ 0][ "x"] = 1L;
                  source[ "points"][ 0][ "y"] = 2L;
                  source[ "points"][ 1][ "x"] = 3L;
                  source[ "points"][ 1][ "y"] = 4L;

                  const auto compact = toml::compact::write( source);
                  assert( compact.contains( "points = [{ x = 1, y = 2 }, { x = 3, y = 4 }]"));

                  const auto elegant = toml::write( source);
                  assert( ! elegant.contains( "[{"));
                  assert( elegant.contains( "[[points]]"));

                  const auto target = toml::parse( compact);
                  assert( target.at( "points").as_array().size() == 2);
                  assert( target.at( "points").at( 0).at( "x").as_integer() == 1);
                  assert( target.at( "points").at( 1).at( "y").as_integer() == 4);
               }

               // compact writer does NOT produce single quotes
               {
                  node source;
                  source[ "s"] = "hello";
                  source[ "points"][ 0][ "x"] = 1L;
                  source[ "points"][ 1][ "x"] = 2L;

                  const auto text = toml::compact::write( source);
                  assert( ! text.contains( '\''));
               }
            }

            void write()
            {
               // flat table roundtrip
               {
                  node source;
                  source[ "a"] = true;
                  source[ "b"] = -42;
                  source[ "c"] = 3.14;
                  source[ "d"] = "hello";

                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "a").as_boolean() == true);
                  assert( target.at( "b").as_integer() == -42);
                  assert( target.at( "c").as_decimal() == 3.14);
                  assert( target.at( "d").as_string() == "hello");
               }

               // nested table roundtrip
               {
                  node source;
                  source[ "outer"][ "inner"] = 42;
                  source[ "outer"][ "flag"] = false;

                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "outer").at( "inner").as_integer() == 42);
                  assert( target.at( "outer").at( "flag").as_boolean() == false);
               }

               // array of tables roundtrip
               {
                  node source;
                  source[ "items"][ 0][ "x"] = 1;
                  source[ "items"][ 1][ "x"] = 2;

                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "items").as_array().size() == 2);
                  assert( target.at( "items").at( 0).at( "x").as_integer() == 1);
                  assert( target.at( "items").at( 1).at( "x").as_integer() == 2);
               }

               // quoted key roundtrip
               {
                  node source;
                  source[ "my key"] = 99;

                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "my key").as_integer() == 99);
               }
            }

            void timestamp()
            {
               using namespace std::chrono_literals;

               // local date
               {
                  const auto table = toml::parse( "d = 2024-03-15\n");
                  assert( table.at( "d").is_local_date());
                  assert( table.at( "d").as_local_date() == std::chrono::local_days{ std::chrono::year{2024}/std::chrono::March/15});
               }

               // local datetime
               {
                  const auto table = toml::parse( "dt = 2024-03-15T12:30:00\n");
                  assert( table.at( "dt").is_local_datetime());
               }

               // world datetime (with offset)
               {
                  const auto table = toml::parse( "dt = 2024-03-15T12:30:00+02:00\n");
                  assert( table.at( "dt").is_zoned_datetime());
               }

               // local time
               {
                  const auto table = toml::parse( "t = 12:30:00\n");
                  assert( table.at( "t").is_local_time());
               }

               // roundtrip local date
               {
                  const auto source = toml::parse( "d = 2024-03-15\n");
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "d").is_local_date());
                  assert( source.at( "d").as_local_date() == target.at( "d").as_local_date());
               }

               // roundtrip world datetime
               {
                  const auto source = toml::parse( "dt = 2024-03-15T12:30:00+00:00\n");
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "dt").is_zoned_datetime());
                  assert( source.at( "dt").as_zoned_datetime() == target.at( "dt").as_zoned_datetime());
               }

               // local time with fractional seconds
               {
                  const auto table = toml::parse( "t = 12:30:00.123456789\n");
                  assert( table.at( "t").is_local_time());
               }

               // local datetime with fractional seconds
               {
                  const auto table = toml::parse( "dt = 2024-03-15T12:30:00.123456789\n");
                  assert( table.at( "dt").is_local_datetime());
               }

               // zoned datetime with fractional seconds
               {
                  const auto table = toml::parse( "dt = 2024-03-15T12:30:00.123456789+00:00\n");
                  assert( table.at( "dt").is_zoned_datetime());
               }

               // roundtrip local time with millisecond granularity
               {
                  node source;
                  source[ "t"] = node::local_time( std::chrono::milliseconds( 45296123));
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "t").is_local_time());
                  assert( source.at( "t").as_local_time().to_duration() == target.at( "t").as_local_time().to_duration());
               }

               // roundtrip local datetime with millisecond granularity
               {
                  using namespace std::chrono_literals;
                  node source;
                  source[ "dt"] = node::local_datetime{ std::chrono::local_days{ 2024y/std::chrono::March/15} + 12h + 30min + std::chrono::milliseconds( 123)};
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "dt").is_local_datetime());
                  assert( source.at( "dt").as_local_datetime() == target.at( "dt").as_local_datetime());
               }

               // roundtrip world datetime with millisecond granularity
               {
                  using namespace std::chrono_literals;
                  node source;
                  source[ "dt"] = node::zoned_datetime{ std::chrono::sys_days{ 2024y/std::chrono::March/15} + 12h + 30min + std::chrono::milliseconds( 123)};
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "dt").is_zoned_datetime());
                  assert( source.at( "dt").as_zoned_datetime() == target.at( "dt").as_zoned_datetime());
               }

               // roundtrip world datetime from system_clock::now()
               {
                  node source;
                  source[ "now"] = node::instant( std::chrono::system_clock::now());
                  const auto target = toml::parse( toml::write( source));
                  assert( target.at( "now").is_zoned_datetime());
                  assert( source.at( "now").as_zoned_datetime() == target.at( "now").as_zoned_datetime());
               }
            }

            void strict_gentle()
            {
               node source;
               source[ "n"] = nullptr;

               // strict: throws on nothing
               try { toml::write( source); assert( false); }
               catch( const std::invalid_argument&) {}

               // gentle: writes nothing as "null" string
               const auto text = toml::elegant::gentle::write( source);
               auto parsed = toml::parse( text);
               assert( parsed.at( "n").is_string());
            }

            void binary()
            {
               const node::binary hello{ std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F}};

               // strict: throws on binary
               { node source; source[ "b"] = hello;
                 auto ok = false; try { toml::write( source); } catch( const std::invalid_argument&) { ok = true; } assert( ok); }

               // gentle elegant write contains base64
               { node source; source[ "b"] = hello;
                 const auto text = toml::elegant::gentle::write( source);
                 assert( text.contains( "SGVsbG8="));

                 // round-trip via manual decode
                 auto parsed = toml::parse( text);
                 const auto decoded = help::transform::binary( parsed.at( "b").as_string());
                 assert( decoded && *decoded == hello); }

               // gentle compact write contains base64
               { node source; source[ "b"] = hello;
                 const auto text = toml::compact::gentle::write( source);
                 assert( text.contains( "SGVsbG8="));

                 auto parsed = toml::parse( text);
                 const auto decoded = help::transform::binary( parsed.at( "b").as_string());
                 assert( decoded && *decoded == hello); }
            }

         } // cases

         void all()
         {
            cases::parse();
            cases::elegant();
            cases::timestamp();
            cases::write();
            cases::strict_gentle();
            cases::binary();
         }
      } // toml::test


      namespace yaml::test 
      {
         namespace cases
         {
            void parse()
            {
               // nested map with directives, comments, blank lines, null, sequences, quoted strings
               {
                  const auto source = R"(#this is a YAML document
%YAML 1.2
a: 123
# this is a comment
 
b:
  ba:  22
  bb:  
    ca: 333
  bc: 3.14
c: 24
d: null
e:
  - 1
  - 2
  - 3
f: 'hello world'
g: "hello\nworld")";

                  const auto document = yaml::parse( source);

                  assert( document.is_object());
                  assert( document.at( "a").as_integer() == 123);
                  assert( document.at( "b").at( "ba").as_integer() == 22);
                  assert( document.at( "b").at( "bb").at( "ca").as_integer() == 333);
                  assert( document.at( "b").at( "bc").as_decimal() == 3.14);
                  assert( document.at( "c").as_integer() == 24);
                  assert( document.at( "d").is_null());
                  assert( document.at( "e").is_array());
                  assert( document.at( "e").as_array().size() == 3);
                  assert( document.at( "e").at( 1).as_integer() == 2);
                  assert( document.at( "f").as_string() == "hello world");
                  assert( document.at( "g").as_string() == "hello\nworld");
               }

               // document boundary: stop at ... and ---
               {
                  const auto d1 = yaml::parse( "a: 1\n...\nb: 2\n");
                  assert( d1.at( "a").as_integer() == 1);
                  assert( ! d1.as_object().contains( "b"));

                  const auto d2 = yaml::parse( "a: 1\n---\nb: 2\n");
                  assert( d2.at( "a").as_integer() == 1);
                  assert( ! d2.as_object().contains( "b"));
               }

               // null documents
               {
                  assert( yaml::parse( "...\n").is_null());
                  assert( yaml::parse( "---\n...\n").is_null());
                  assert( yaml::parse( "---\nnull\n").is_null());
               }

               // empty stream yields 0 documents
               {
                  assert( yaml::all::parse( "").size() == 0);
                  assert( yaml::all::parse( "# just a comment\n").size() == 0);
               }

               // all::parse multi-document stream
               {
                  const auto docs = yaml::all::parse( "a: 1\n---\nb: 2\n");
                  assert( docs.size() == 2);
                  assert( docs.at( 0).at( "a").as_integer() == 1);
                  assert( docs.at( 1).at( "b").as_integer() == 2);
               }

               // ~ as null
               { const auto t = yaml::parse( "a: ~\n"); assert( t.at( "a").is_null()); }

               // boolean variants
               { const auto t = yaml::parse( "a: True\n");  assert( t.at( "a").as_boolean() == true); }
               { const auto t = yaml::parse( "a: TRUE\n");  assert( t.at( "a").as_boolean() == true); }
               { const auto t = yaml::parse( "a: False\n"); assert( t.at( "a").as_boolean() == false); }
               { const auto t = yaml::parse( "a: FALSE\n"); assert( t.at( "a").as_boolean() == false); }

               // hex and octal integers
               { const auto t = yaml::parse( "a: 0x1F\n"); assert( t.at( "a").as_integer() == 31); }
               { const auto t = yaml::parse( "a: 0o17\n"); assert( t.at( "a").as_integer() == 15); }

               // special float values
               { const auto t = yaml::parse( "a: .inf\n");  assert( std::isinf( t.at( "a").as_decimal()) && t.at( "a").as_decimal() > 0); }
               { const auto t = yaml::parse( "a: +.inf\n"); assert( std::isinf( t.at( "a").as_decimal()) && t.at( "a").as_decimal() > 0); }
               { const auto t = yaml::parse( "a: -.inf\n"); assert( std::isinf( t.at( "a").as_decimal()) && t.at( "a").as_decimal() < 0); }
               { const auto t = yaml::parse( "a: .nan\n");  assert( std::isnan( t.at( "a").as_decimal())); }

            }

            void flow()
            {
               // inline mapping
               {
                  const auto target = yaml::parse( "point: {\"x\": 1, \"y\": 2}\n");
                  assert( target.at( "point").at( "x").as_integer() == 1);
                  assert( target.at( "point").at( "y").as_integer() == 2);
               }

               // inline sequence
               {
                  const auto target = yaml::parse( "tags: [\"web\", \"api\", \"v2\"]\n");
                  assert( target.at( "tags").as_array().size() == 3);
                  assert( target.at( "tags").at( 0).as_string() == "web");
                  assert( target.at( "tags").at( 2).as_string() == "v2");
               }

               // nested flow
               {
                  const auto target = yaml::parse( "server: {\"host\": \"localhost\", \"port\": 8080, \"tags\": [\"web\", \"api\"]}\n");
                  assert( target.at( "server").at( "host").as_string() == "localhost");
                  assert( target.at( "server").at( "port").as_integer() == 8080);
                  assert( target.at( "server").at( "tags").at( 1).as_string() == "api");
               }

               // mixed block and flow
               {
                  const auto source = R"(
name: example
config: {"debug": true, "timeout": 30}
items: [1, 2, 3]
)";
                  const auto target = yaml::parse( source);
                  assert( target.at( "name").as_string() == "example");
                  assert( target.at( "config").at( "debug").as_boolean() == true);
                  assert( target.at( "config").at( "timeout").as_integer() == 30);
                  assert( target.at( "items").as_array().size() == 3);
                  assert( target.at( "items").at( 1).as_integer() == 2);
               }

               // flow::write object roundtrip
               {
                  node source;
                  source[ "x"] = 1;
                  source[ "y"] = 2;

                  const auto text = yaml::compact::write( source);
                  const auto target = json::parse( text);
                  assert( target.at( "x").as_integer() == 1);
                  assert( target.at( "y").as_integer() == 2);
               }

               // flow::write array roundtrip
               {
                  node source;
                  source[ 0] = "web";
                  source[ 1] = "api";
                  source[ 2] = "v2";

                  const auto text = yaml::compact::write( source);
                  const auto target = json::parse( text);
                  assert( target.as_array().size() == 3);
                  assert( target.at( 0).as_string() == "web");
                  assert( target.at( 2).as_string() == "v2");
               }

               // flow::write nested roundtrip
               {
                  node source;
                  source[ "host"] = "localhost";
                  source[ "port"] = 8080;
                  source[ "tags"][ 0] = "web";
                  source[ "tags"][ 1] = "api";

                  const auto text = yaml::compact::write( source);
                  const auto target = json::parse( text);
                  assert( target.at( "host").as_string() == "localhost");
                  assert( target.at( "port").as_integer() == 8080);
                  assert( target.at( "tags").at( 1).as_string() == "api");
               }
            }

            void write()
            {
               // flat map
               {
                  node source;
                  source[ "a"] = nullptr;
                  source[ "b"] = true;
                  source[ "c"] = -42;
                  source[ "d"] = 3.14;
                  source[ "e"] = "hello";

                  const auto target = yaml::parse( yaml::write( source));
                  assert( target.at( "a").is_null());
                  assert( target.at( "b").as_boolean() == true);
                  assert( target.at( "c").as_integer() == -42);
                  assert( target.at( "d").as_decimal() == 3.14);
                  assert( target.at( "e").as_string() == "hello");
               }

               // nested map
               {
                  node source;
                  source[ "outer"][ "inner"] = 42;
                  source[ "outer"][ "flag"] = false;

                  const auto target = yaml::parse( yaml::write( source));
                  assert( target.at( "outer").at( "inner").as_integer() == 42);
                  assert( target.at( "outer").at( "flag").as_boolean() == false);
               }

               // array of scalars
               {
                  node source;
                  source[ "items"][ 0] = 1;
                  source[ "items"][ 1] = 2;
                  source[ "items"][ 2] = 3;

                  const auto target = yaml::parse( yaml::write( source));
                  assert( target.at( "items").as_array().size() == 3);
                  assert( target.at( "items").at( 0).as_integer() == 1);
                  assert( target.at( "items").at( 2).as_integer() == 3);
               }

               // array of maps
               {
                  node source;
                  source[ "servers"][ 0][ "host"] = "db1";
                  source[ "servers"][ 0][ "port"] = 5432;
                  source[ "servers"][ 1][ "host"] = "db2";
                  source[ "servers"][ 1][ "port"] = 5433;

                  const auto target = yaml::parse( yaml::write( source));
                  assert( target.at( "servers").as_array().size() == 2);
                  assert( target.at( "servers").at( 0).at( "host").as_string() == "db1");
                  assert( target.at( "servers").at( 1).at( "port").as_integer() == 5433);
               }

               // inf/nan roundtrip
               {
                  node source;
                  source[ "a"] = std::numeric_limits< node::decimal>::infinity();
                  source[ "b"] = -std::numeric_limits< node::decimal>::infinity();
                  source[ "c"] = std::numeric_limits< node::decimal>::quiet_NaN();

                  const auto target = yaml::parse( yaml::write( source));
                  assert( std::isinf( target.at( "a").as_decimal()) && target.at( "a").as_decimal() > 0);
                  assert( std::isinf( target.at( "b").as_decimal()) && target.at( "b").as_decimal() < 0);
                  assert( std::isnan( target.at( "c").as_decimal()));
               }

               // key that needs quoting
               {
                  node source;
                  source[ "my key"] = 42;

                  const auto target = yaml::parse( yaml::write( source));
                  assert( target.at( "my key").as_integer() == 42);
               }
            }

            void block()
            {
               // literal: preserves newlines, clip (default)
               {
                  const auto target = yaml::parse( "key: |\n  hello\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello\nworld\n");
                  assert( target.at( "next").as_integer() == 1);
               }

               // literal: strip chomping
               {
                  const auto target = yaml::parse( "key: |-\n  hello\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello\nworld");
               }

               // literal: keep chomping
               {
                  const auto target = yaml::parse( "key: |+\n  hello\n  world\n\n\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello\nworld\n\n\n");
               }

               // folded: newlines become spaces, clip
               {
                  const auto target = yaml::parse( "key: >\n  hello\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello world\n");
               }

               // folded: blank line becomes newline
               {
                  const auto target = yaml::parse( "key: >\n  hello\n\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello\nworld\n");
               }

               // literal with leading blank lines
               {
                  const auto target = yaml::parse( "key: |\n\n  hello\nnext: 1\n");
                  assert( target.at( "key").as_string() == "\nhello\n");
               }

               // literal with # inside (not a comment)
               {
                  const auto target = yaml::parse( "key: |\n  hello\n  # not a comment\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello\n# not a comment\nworld\n");
               }

               // nested block scalar
               {
                  const auto target = yaml::parse( "a:\n  b: |\n    hello\n    world\n  c: 42\n");
                  assert( target.at( "a").at( "b").as_string() == "hello\nworld\n");
                  assert( target.at( "a").at( "c").as_integer() == 42);
               }

               // folded: strip chomping
               {
                  const auto target = yaml::parse( "key: >-\n  hello\n  world\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello world");
               }

               // folded: keep chomping
               {
                  const auto target = yaml::parse( "key: >+\n  hello\n  world\n\n\nnext: 1\n");
                  assert( target.at( "key").as_string() == "hello world\n\n\n");
               }
            }

            void tagged()
            {
               // !!str plain — coerces non-string types to string
               { const auto t = yaml::parse( "key: !!str 123\n");   assert( t.at( "key").as_string() == "123"); }
               { const auto t = yaml::parse( "key: !!str true\n");  assert( t.at( "key").as_string() == "true"); }
               { const auto t = yaml::parse( "key: !!str hello\n"); assert( t.at( "key").as_string() == "hello"); }

               // !!str quoted
               { const auto t = yaml::parse( "key: !!str \"hello\"\n"); assert( t.at( "key").as_string() == "hello"); }
               { const auto t = yaml::parse( "key: !!str 'hello'\n");   assert( t.at( "key").as_string() == "hello"); }

               // !!null — all valid representations
               { const auto t = yaml::parse( "key: !!null\n");       assert( t.at( "key").is_null()); }
               { const auto t = yaml::parse( "key: !!null ~\n");     assert( t.at( "key").is_null()); }
               { const auto t = yaml::parse( "key: !!null null\n");  assert( t.at( "key").is_null()); }
               { const auto t = yaml::parse( "key: !!null Null\n");  assert( t.at( "key").is_null()); }
               { const auto t = yaml::parse( "key: !!null NULL\n");  assert( t.at( "key").is_null()); }

               // !!bool — all core schema variants
               { const auto t = yaml::parse( "key: !!bool true\n");  assert( t.at( "key").as_boolean() == true); }
               { const auto t = yaml::parse( "key: !!bool True\n");  assert( t.at( "key").as_boolean() == true); }
               { const auto t = yaml::parse( "key: !!bool TRUE\n");  assert( t.at( "key").as_boolean() == true); }
               { const auto t = yaml::parse( "key: !!bool false\n"); assert( t.at( "key").as_boolean() == false); }
               { const auto t = yaml::parse( "key: !!bool False\n"); assert( t.at( "key").as_boolean() == false); }
               { const auto t = yaml::parse( "key: !!bool FALSE\n"); assert( t.at( "key").as_boolean() == false); }

               // !!int — decimal, negative, hex, octal
               { const auto t = yaml::parse( "key: !!int 42\n");    assert( t.at( "key").as_integer() == 42); }
               { const auto t = yaml::parse( "key: !!int -7\n");    assert( t.at( "key").as_integer() == -7); }
               { const auto t = yaml::parse( "key: !!int 0x1F\n");  assert( t.at( "key").as_integer() == 31); }
               { const auto t = yaml::parse( "key: !!int 0o17\n");  assert( t.at( "key").as_integer() == 15); }

               // !!float — decimal, integer coerced, special values
               { const auto t = yaml::parse( "key: !!float 3.14\n");  assert( t.at( "key").as_decimal() == 3.14); }
               { const auto t = yaml::parse( "key: !!float 123\n");   assert( t.at( "key").as_decimal() == 123.0); }
               { const auto t = yaml::parse( "key: !!float .inf\n");  assert( std::isinf( t.at( "key").as_decimal()) && t.at( "key").as_decimal() > 0); }
               { const auto t = yaml::parse( "key: !!float -.inf\n"); assert( std::isinf( t.at( "key").as_decimal()) && t.at( "key").as_decimal() < 0); }
               { const auto t = yaml::parse( "key: !!float .nan\n");  assert( std::isnan( t.at( "key").as_decimal())); }

               // error cases
               { auto ok = false; try { yaml::parse( "key: !!bool 1\n"); }       catch( const std::exception&) { ok = true; } assert( ok); }
               { auto ok = false; try { yaml::parse( "key: !!int 3.14\n"); }     catch( const std::exception&) { ok = true; } assert( ok); }
               { auto ok = false; try { yaml::parse( "key: !!null banana\n"); }  catch( const std::exception&) { ok = true; } assert( ok); }
               { auto ok = false; try { yaml::parse( "key: !!foo bar\n"); }      catch( const std::exception&) { ok = true; } assert( ok); }
            }

            void anchor()
            {
               // scalar anchor and alias
               {
                  const auto t = yaml::parse( "a: &x 42\nb: *x\n");
                  assert( t.at( "a").as_integer() == 42);
                  assert( t.at( "b").as_integer() == 42);
               }

               // string anchor
               {
                  const auto t = yaml::parse( "a: &x hello\nb: *x\n");
                  assert( t.at( "a").as_string() == "hello");
                  assert( t.at( "b").as_string() == "hello");
               }

               // alias is independent copy
               {
                  const auto t = yaml::parse( "a: &x 1\nb: *x\nc: *x\n");
                  assert( t.at( "a").as_integer() == 1);
                  assert( t.at( "b").as_integer() == 1);
                  assert( t.at( "c").as_integer() == 1);
               }

               // anchor on flow sequence
               {
                  const auto t = yaml::parse( "a: &x [1, 2, 3]\nb: *x\n");
                  assert( t.at( "a").as_array().size() == 3);
                  assert( t.at( "b").as_array().size() == 3);
                  assert( t.at( "b").at( 1).as_integer() == 2);
               }

               // anchor on flow mapping
               {
                  const auto t = yaml::parse( "a: &x {\"p\": 1, \"q\": 2}\nb: *x\n");
                  assert( t.at( "a").at( "p").as_integer() == 1);
                  assert( t.at( "b").at( "q").as_integer() == 2);
               }

               // anchor overwrite: last definition wins
               {
                  const auto t = yaml::parse( "a: &x 1\nb: &x 2\nc: *x\n");
                  assert( t.at( "c").as_integer() == 2);
               }

               // alias before anchor is undefined
               { auto ok = false; try { yaml::parse( "a: *x\nb: &x 1\n"); } catch( const std::exception&) { ok = true; } assert( ok); }

               // anchors cleared between documents
               { auto ok = false; try { yaml::all::parse( "a: &x 1\n---\nb: *x\n"); } catch( const std::exception&) { ok = true; } assert( ok); }

               // block mapping anchor not supported
               { auto ok = false; try { yaml::parse( "a: &x\n  p: 1\nb: *x\n"); } catch( const std::exception&) { ok = true; } assert( ok); }

               // unknown alias error
               { auto ok = false; try { yaml::parse( "a: *unknown\n"); } catch( const std::exception&) { ok = true; } assert( ok); }
            }

            void timestamp()
            {
               using namespace std::chrono_literals;

               // write local date via elegant yaml
               {
                  const auto text = yaml::write( node::instant( std::chrono::local_days{ 1970y/std::chrono::May/2}));
                  assert( text.contains( "!!timestamp"));
                  assert( text.contains( "1970-05-02"));
               }

               // write world datetime via elegant yaml
               {
                  const auto text = yaml::write( node::instant( node::zoned_datetime{ std::chrono::sys_days{ 1970y/std::chrono::May/2} + 18h + 30min}));
                  assert( text.contains( "!!timestamp"));
                  assert( text.contains( "1970-05-02T18:30:00"));
               }

            }

            void strict_gentle()
            {
               const node source = help::transform::instant( "12:30:00").value();

               // strict: throws on local_time
               try { yaml::write( source); assert( false); }
               catch( const std::invalid_argument&) {}

               // gentle: writes local_time as bare value (parses back as string)
               const auto text = yaml::elegant::gentle::write( source);
               assert( yaml::parse( text).is_string());
            }

            void binary()
            {
               const node::binary hello{ std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F}};

               // elegant write emits !!binary literal block
               { 
                  node source = hello;
                  const auto text = yaml::write( source);
                  assert( text.contains( "!!binary"));
                  assert( text.contains( "SGVsbG8=")); 
               }

               // compact write emits !!binary when node is binary directly (object delegates to json writer)
               { 
                  node source = hello;
                  const auto text = yaml::compact::gentle::write( source);
                  assert( text.contains( "!!binary"));
                  assert( text.contains( "SGVsbG8=")); 
               }
            }

         } // cases

         void all()
         {
            cases::parse();
            cases::flow();
            cases::write();
            cases::block();
            cases::tagged();
            cases::anchor();
            cases::timestamp();
            cases::strict_gentle();
            cases::binary();
         }

      } // yaml::test
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
         poly::help::test::all();
         poly::base::test::all();
         poly::tool::test::all();
         poly::json::test::all();
         poly::toml::test::all();
         poly::yaml::test::all();
      }

      return 0;
   }
   catch( const std::exception& e)
   {
      std::println( stderr, "{}", e.what());
      return 1;
   }
}

//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <chrono>
#include <format>
#include <string>
#include <spanstream>


namespace poly
{
   inline namespace version
   {
      namespace time
      {
         namespace local
         {
            namespace current
            {
               auto date_time()
               {
                  return std::chrono::current_zone()->to_local( std::chrono::system_clock::now());
               }

               auto date()
               {
                  const auto now = date_time();
                  return std::chrono::floor< std::chrono::days>( now);
               }

               auto time()
               {
                  const auto now = date_time();
                  return std::chrono::hh_mm_ss{ now - std::chrono::floor< std::chrono::days>( now)};
               }

            } // current

            auto date_time( std::string_view value)
            {
               std::chrono::local_time<std::chrono::system_clock::duration> result;
               if( ( std::ispanstream{ value} >> std::chrono::parse( "%F %T", result)).fail())
                  std::ispanstream{ value} >> std::chrono::parse( "%FT%T", result);
               return result;
            }

            auto date( std::string_view value)
            {
               std::chrono::local_time<std::chrono::system_clock::duration> result;
               std::ispanstream{ value} >> std::chrono::parse( "%F", result);
               return std::chrono::floor< std::chrono::days>( result);
            }

            auto time( std::string_view value)
            {
               std::chrono::system_clock::duration result;
               std::ispanstream{ value} >> std::chrono::parse( "%T", result);
               return std::chrono::hh_mm_ss{ result};
            }

            auto date_time( std::chrono::local_time< std::chrono::system_clock::duration> value)
            {
               return std::format( "{:%F %T}", value);
            }

            auto date( std::chrono::local_time< std::chrono::days> value)
            {
               return std::format( "{:%F}", value);
            }

            auto time( std::chrono::hh_mm_ss< std::chrono::system_clock::duration> value)
            {
               return std::format( "{:%T}", value);
            }

         } // local

         namespace world
         {
            namespace current
            {
               auto date_time()
               {
                  return std::chrono::system_clock::now();
               }
            } // current

            auto date_time( std::chrono::sys_time< std::chrono::system_clock::duration> value)
            {
               return std::format( "{:%F %T%z}", std::chrono::zoned_time{ std::chrono::current_zone(), value});
            }

            auto date_time( std::string_view value)
            {
               std::chrono::sys_time< std::chrono::system_clock::duration> result;
               if( ( std::ispanstream{ value} >> std::chrono::parse( "%F %T%z", result)).fail())
                  std::ispanstream{ value} >> std::chrono::parse( "%FT%T%z", result);
               return result;
            }
         } // world

      } // time
   } // version
} // poly

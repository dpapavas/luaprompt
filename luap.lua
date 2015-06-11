#!/usr/bin/env lua

-- Copyright (C) 2015 Boris Nagaev, Dimitris Papavasiliou
--
-- Permission is hereby granted, free of charge, to any person
-- obtaining a copy of this software and associated documentation
-- files (the "Software"), to deal in the Software without
-- restriction, including without limitation the rights to use, copy,
-- modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
-- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
-- BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
-- ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
-- CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

local version = "0.6"
local copyright = "luap " .. version ..
   " Copyright (C) 2012-2015 Dimitris Papavasiliou, Boris Nagaev"

local prompt = require "prompt"
local argparse = require "argparse"

-- Define the command-line argument parser.

local parser = argparse()
   :name "luap"
   :description "A Lua command prompt with pretty-printing and auto-completion."
   :add_help "-h"

-- Options

parser:option "-e"
   :argname "STMT"
   :description "Execute string 'STMT'."
   :count "*"

parser:option "-l"
   :argname "NAME"
   :description "Require library 'NAME'."
   :count "*"

if jit then
   parser:option "-j"
      :argname "CMD"
      :description "Perform LuaJIT control command."
      :count "*"
   parser:option "-O"
      :argname "OPT"
      :description "Control LuaJIT optimizations."
      :count "*"
end

-- Flags

parser:flag "-p"
   :description "Force plain, uncolored output."

parser:flag "-v"
   :description "Print version information."

parser:flag "-i"
   :description "Enter interactive mode."

-- Arguments

parser:argument "SCRIPT"
   :description [[A Lua script to be executed.  Any arguments
specfied after the script name, are passed to
the script.]]
   :args '*'

local args = parser:parse(arg)

-- Print version information and exit.

if args.v then
   print(table.concat ({copyright,
                        prompt.copyrights[1],
                        prompt.copyrights[2]}, "\n"))
   os.exit(0)
end

-- Pass optimization options to LuaJIT.

if args.O then
   for _, O in ipairs(args.O) do
      jit.opt.start(O)
   end
end

local interactive = (args.i or (prompt.interactive and
                                   #args.SCRIPT == 0 and #args.e == 0))

if interactive then
   print(table.concat ({prompt.copyrights[2], copyright}, "\n"))
end

-- Parse control commands and pass them to LuaJIT.

if args.j then
   for _, j in ipairs(args.j) do
      local unpack = unpack or table.unpack

      -- Parse the command name.

      local name = j:match('^[^=]+')

      if not name then
         print(parser:get_help())
         os.exit(0)
      end

      -- Parse the arguments, if any.

      local args = {}

      for arg in (j:match('=(.*)$') or ""):gmatch('[^,]+') do
         table.insert(args, arg)
      end

      -- Look for a builtin command.

      if jit[name] then
         jit[name](unpack(args))
      else
         local ok, m = pcall(require, 'jit.' .. name)

         if not ok or not m then
            parser:error('unknown luaJIT command or ' ..
                     'jit.* modules not installed')
         end

         m.start(unpack(args))
      end
   end
end

-- Require modules specified on the command line.

if #args.l > 0 then
   for _, l in ipairs(args.l) do
      if _VERSION == "Lua 5.1" then
         require(l)
      else
         _G[l] = require(l)
      end
   end
end

-- Load and execute chunks passed on the command line.

if #args.e > 0 then
   local loadstring = loadstring or load
   for _, e in ipairs(args.e) do
      loadstring(e)()
   end
end

-- Run the script given on the command line, passing any arguments as
-- required.

if #args.SCRIPT > 0 or (not interactive and #args.e == 0) then
   local chunk
   local loadstring = loadstring or load
   local unpack = unpack or table.unpack
   local name

   if #args.SCRIPT > 0 then
     name = table.remove(args.SCRIPT, 1)
   else
      name = "-"
   end

   if name == "-" then
      chunk, message = loadstring(io.stdin:read("*a"))
   else
      chunk, message = loadfile(name)
   end

   if chunk then
      -- This duplicates the behavior of the standard Lua interpreter
      -- to some extent.  Arguments prior to the script name are not
      -- passed.

      arg = {[0] = name, unpack(args.SCRIPT)}
      prompt.call(chunk, unpack(args.SCRIPT))
   else
      print(message)
      os.exit(0)
   end
end

-- Enter interactive mode, if appropriate.

if interactive then
   prompt.name = 'lua'
   prompt.prompts = {'>  ', '>> '}
   prompt.colorize = not args.p
   prompt.history = os.getenv('HOME') .. '/.lua_history'

   prompt.enter()
end

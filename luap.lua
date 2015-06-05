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

local version = "0.5"
local prompt = require "prompt"
local argparse = require "argparse"

local function greet()
   print(string.format([[
luap %s Copyright (C) 2015 Dimitris Papavasiliou, Boris Nagaev
luaprompt %s Copyright (C) 2012-2015 Dimitris Papavasiliou]],
            version, prompt.version))
end

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
      :argname "cmd"
      :description "Perform LuaJIT control command."
      :count "*"
   parser:option "-O"
      :argname "opt"
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
specfied after the script name are passed to
the script.]]
   :args '*'

local args = parser:parse(arg)

-- Print version information and exit.

if args.v then
   greet()
   os.exit(0)
end

-- Pass optimization options to LuaJIT.

if args.O then
   for _, O in ipairs(args.O) do
      jit.opt.start(O)
   end
end

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

-- Load and execute chunks passed on the command line.

if args.e and #args.e > 0 then
   local loadstring = loadstring or load
   for _, e in ipairs(args.e) do
      loadstring(e)()
   end
end

-- Require modules specified on the command line.

if args.l and #args.l > 0 then
   for _, l in ipairs(args.l) do
      require(l)
   end
end

-- Run the script given on the command line, passing any arguments as
-- required.

if args.script and #args.script > 0 then
   local name = table.remove(args.script, 1)
   loadfile(name)(unpack(args.script))
end

-- Enter interactive mode, if appropriate.

if args.i or (#args.SCRIPT == 0 and #args.e == 0)  then
   greet()

   prompt.name = 'lua'
   prompt.prompts = {'>  ', '>> '}
   prompt.colorize = not args.p
   prompt.history = os.getenv('HOME') .. '/.lua_history'

   prompt.enter()
end

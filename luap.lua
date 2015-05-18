#!/usr/bin/env lua

local argparse = require "argparse"

local parser = argparse()
    :name "luap"
    :description "Lua command prompt"
parser:option "-e"
    :argname "STMT"
    :description "Execute string 'STMT'"
    :count "*"
parser:option "-l"
    :argname "NAME"
    :description "Require library 'NAME'"
    :count "*"
parser:flag "-p"
    :description "Force plain, uncolored output"
parser:flag "-v"
    :description "Print version information"
parser:flag "-i"
    :description "Enter interactive mode"
parser:argument "script"
    :description "Lua script and its arguments"
    :args '*'

if jit then
    -- TODO
end

local args = parser:parse(arg)

if args.v then
    -- TODO
    os.exit(0)
end

if args.script and #args.script == 0 then
    args.script = nil
end

if args.l and #args.l == 0 then
    args.l = nil
end

if args.e and #args.e == 0 then
    args.e = nil
end

if args.e then
    local loadstring = loadstring or load
    for _, e in ipairs(args.e) do
        loadstring(e)()
    end
end

if args.l then
    for _, l in ipairs(args.l) do
        require(l)
    end
end

if args.script then
    local script_name = table.remove(args.script, 1)
    arg = args.script
    dofile(script_name)
end

if (not args.script and not args.e) or args.i then
    local prompt = require 'prompt'
    prompt.setname('lua')
    prompt.setprompts('> ', '> ')
    if args.p then
        prompt.setcolor(false)
    end
    prompt.sethistory(os.getenv('HOME') .. '/.lua_history')
    prompt.enter()
end

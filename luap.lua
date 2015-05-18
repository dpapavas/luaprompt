#!/usr/bin/env lua

local function greet()
    local prompt = require 'prompt'
    local text = [[
luap %s
Copyright (C) 2012-2015 Dimitris Papavasiliou, Boris Nagaev]]
    print(text:format(prompt.version()))
end

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
    parser:option "-j"
        :argname "cmd"
        :description "Perform LuaJIT control command"
        :count "*"
    parser:option "-O"
        :argname "cmd"
        :description "Control LuaJIT optimizations"
        :count "*"
end

local args = parser:parse(arg)

if args.v then
    greet()
    os.exit(0)
end

if args.O then
    for _, O in ipairs(args.O) do
        jit.opt.start(O)
    end
end

if args.j then
    for _, j in ipairs(args.j) do
        local cmd = j:match('^[^=]+')
        if not cmd then
            print(parser:get_help())
            os.exit(0)
        end
        local cmd_args = {}
        local cmd_args_str = j:match('=(.*)$')
        if cmd_args_str then
            for cmd_arg in cmd_args_str:gmatch('[^,]+') do
                table.insert(cmd_args, cmd_arg)
            end
        end
        local jit_module = jit[cmd]
        if not jit_module then
            local ok, m = pcall(require, 'jit.' .. cmd)
            if ok then
                jit_module = m
            end
        end
        if not jit_module then
            print('luap: unknown luaJIT command or ' ..
                'jit.* modules not installed')
            os.exit(0)
        end
        local unpack = unpack or table.unpack
        jit_module.start(unpack(cmd_args))
    end
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
    greet()
    local prompt = require 'prompt'
    prompt.setname('lua')
    prompt.setprompts('> ', '> ')
    if args.p then
        prompt.setcolor(false)
    end
    prompt.sethistory(os.getenv('HOME') .. '/.lua_history')
    prompt.enter()
end

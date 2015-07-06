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

package = "luaprompt"
version = "dev-1"
supported_platforms = {"!windows", "!win32"}

source = {
    url = "git://github.com/dpapavas/luaprompt.git",
}

description = {
    summary = "A Lua command prompt with pretty-printing and auto-completion",
    homepage = "https://github.com/dpapavas/luaprompt",
    license = "MIT/X11",

    detailed = [[
luaprompt is both an interactive Lua prompt that can be used instead
of the official interpreter, as well as module that provides a Lua
command prompt that can be embedded in a host application.  As a
standalone interpreter it provides many conveniences that are missing
from the official Lua interpreter.  As an embedded prompt, it's meant
for applications that use Lua as a configuration or interface language
and can therefore benefit from an interactive prompt for debugging or
regular use.

luaprompt features:

* Readline-based input with history and completion: In particular all
  keywords, global variables and table accesses (with string or
  integer keys) can be completed in addition to readline's standard
  file completion.  Module names are also completed, for modules
  installed in the standard directories, and completed modules can
  optionally be loaded.

* Persistent command history (retained across sessions).

* Proper value pretty-printing for interactive use: When an expression
  is entered at the prompt, all returned values are printed
  (prepending with an equal sign is not required).  Values are printed
  in a descriptive way that tries to be as readable as possible.  The
  formatting tries to mimic Lua code (this is done to minimize
  ambiguities and no guarantees are made that it is valid code).
  Additionally, each value is stored in a table for future reference.

* Color highlighting of error messages and variable printouts.
]],
}

dependencies = {
    "lua >= 5.1",
    "argparse",
}

external_dependencies = {
    READLINE = {
        header = "readline/readline.h",
        library = "readline",
    },

    HISTORY = {
        header = "readline/history.h",
        library = "history",
    },
}

build = {
    type = "builtin",

    modules = {
        ['prompt'] = {
            sources = {
                "prompt.c",
                "module.c"
            },

            incdirs = {
                "$(READLINE_INCDIR)",
                "$(HISTORY_INCDIR)",
            },

            libdirs = {
                "$(READLINE_LIBDIR)",
                "$(HISTORY_LIBDIR)",
            },

            defines = {
                "_GNU_SOURCE",
                "HAVE_LIBREADLINE",
                "HAVE_READLINE_HISTORY",
                "HAVE_IOCTL",
                "COMPLETE_KEYWORDS",
                "COMPLETE_TABLE_KEYS",
                "COMPLETE_METATABLE_KEYS",
                "COMPLETE_FILE_NAMES",
                "COMPLETE_MODULES",
                "SAVE_RESULTS",
                'RESULTS_TABLE_NAME=\"_\"',
            },

            libraries = {
                "readline",
                "history",
            },
        },
    },

    install = {
        bin = {
            luap = 'luap.lua'
        },
    },
}

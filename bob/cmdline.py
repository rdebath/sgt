# Python function to parse a command line for options and arguments
# in a basically Unixy way.

import string

def parse_cmdline(args, short_opts_taking_values):
    doing_opts = 1
    ret = []
    while len(args) > 0:
        arg = args[0]
        args = args[1:]

        if doing_opts and arg[:1] == "-":
            # Process command-line options, in general.
            if arg == "--":
                doing_opts = 0
            elif arg[:2] == "--":
                # GNUish long option, with an optional value separated
                # from the option name by "=".
                eqidx = string.find(arg, "=")
                if eqidx >= 0:
                    opt = arg[:eqidx]
                    val = arg[eqidx+1:]
                else:
                    opt = arg
                    val = None
                ret.append((opt, val))
            else:
                # Single-letter options. If one takes a value, then the
                # value is everything after that option letter in this
                # argument word, or the whole of the next word if
                # there's nothing left in this one; if it doesn't, the
                # rest of the argument word is treated as further
                # option letters.
                arg = arg[1:]
                while len(arg) > 0:
                    if arg[0] in short_opts_taking_values:
                        if len(arg) > 1:
                            ret.append(("-"+arg[0], arg[1:]))
                        elif len(args) > 0:
                            ret.append(("-"+arg[0], args[0]))
                            args = args[1:]
                        else:
                            ret.append(("-"+arg[0], None)) # no value available
                        # Whatever we did here, we're finished with
                        # this argument value.
                        break
                    else:
                        ret.append(("-"+arg[0], None))
                        arg = arg[1:]
        else:
            ret.append((None, arg))

    return ret

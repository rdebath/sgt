import c_lex
import c_parse
import c_semantics
import tgt_sample
import sys

action = "sem"

args = sys.argv[1:]
while 1:
    if args[0] == "-s":
        action = "sem"
        args = args[1:]
    if args[0] == "-p":
        action = "parse"
        args = args[1:]
    elif args[0] == "-l":
        action = "lex"
        args = args[1:]
    if args == [] or args[0][:1] != "-":
        break

if args == []:
    file = sys.stdin
else:
    file = open(args[0])

l = c_lex.lexer()
l.feed(file.read())
if action == "lex":
    while 1:
        token = l.get()
        if token == None:
            break
        print token.type, "`" + token.text + "'"
    sys.exit(0)
else:
    tree = c_parse.parse(l)

if action == "parse":
    tree.display(0)
else:
    sem = c_semantics.semantics(tree, tgt_sample.target())
    sem.display()

#!/bin/sh
find . \( -name '*.h' -o -name '*.cpp' -o -name '*.c' \) -not -path "./gtk3/*" -not -path "./build/*"  | xargs clang-format -i

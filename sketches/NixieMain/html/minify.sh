#!/usr/bin/env bash

# https://github.com/kangax/html-minifier
# npm install html-minifier -g

OUT="$(html-minifier \
    --collapse-boolean-attributes \
    --collapse-whitespace \
    --decode-entities \
    --quote-character \' \
    --remove-attribute-quotes \
    --remove-comments \
    --remove-empty-attributes \
    --remove-optional-tags \
    --remove-script-type-attributes \
    --use-short-doctype \
    --minify-css true \
    --minify-js true \
    $1)"

# Escapes %, but leaves pre-escaped flags unchanged
# https://www.tutorialspoint.com/c_standard_library/c_function_printf.htm
# Regarding the capital %S:
# https://forum.arduino.cc/index.php?topic=293408.msg2049610#msg2049610
OUT="$(echo "$OUT" | perl -ne 's/%(?![cdieEfgGosuxXpnS%])/%%/g; print;')"

# Replace all " with \" (but we use ' for attributes)
OUT="$(echo "$OUT" | perl -ne 's/"/\\"/g; print;')"

echo "$OUT"

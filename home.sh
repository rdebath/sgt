#!/bin/sh

exec > home.pac

cat ick-config.js
cat << EOF

function FindProxyForURL(url, host)
{
    if (RewriteURL(url) != url)
        return "PROXY localhost:$ICK_PROXY";

    return "DIRECT";
}
EOF

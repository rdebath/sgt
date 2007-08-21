#!/bin/sh

exec > work.pac

cat ick-config.js
cat << EOF

function FindProxyForURL(url, host)
{
    if (RewriteURL(url) != url)
        return "PROXY localhost:$ICK_PROXY";

    if (shExpMatch(host, "*.arm.com") ||
        shExpMatch(host, "*.local") ||
        isPlainHostName(host) ||
        isInNet(host, "127.0.0.0", "255.0.0.0"))
        return "DIRECT";

    return "SOCKS localhost:11111";
}
EOF

// -*- java -*-

function strprefix(str, pfx)
{
    return (str.length >= pfx.length &&
	    str.substr(0, pfx.length) == pfx);
}

function RewriteURL(url)
{
    if (strprefix(url, "http://www.livejournal.com/users/") ||
        strprefix(url, "http://www.livejournal.com/community/")) {
	if (!strprefix(url, "http://www.livejournal.com/users/simont/") &&
	    url.indexOf("?style=") < 0 && url.indexOf("&style=") < 0) {
	    var hashpos;
	    var extra;
	    hashpos = url.indexOf("#");
	    if (hashpos < 0)
		hashpos = url.length;
	    if (url.indexOf("?") < 0)
		extra = "?style=mine";
	    else
		extra = "&style=mine";
	    url = url.substr(0, hashpos) + extra +
		url.substring(hashpos, url.length);
	}
    }

    return url;
}

function FindProxyForURL(url, host)
{
    if (RewriteURL(url) != url)
        return "PROXY localhost:18768";

    return "DIRECT";
}

// -*- java -*-

function strprefix(str, pfx)
{
    return (str.length >= pfx.length &&
	    str.substr(0, pfx.length) == pfx);
}

function RewriteURL(url)
{
    var style;

    style = false;

    if (strprefix(url, "http://www.livejournal.com/users/") ||
        strprefix(url, "http://www.livejournal.com/~") ||
        strprefix(url, "http://www.livejournal.com/community/")) {
	if (!strprefix(url, "http://www.livejournal.com/users/simont/") &&
            !strprefix(url, "http://www.livejournal.com/community/join.bml") &&
            !strprefix(url, "http://www.livejournal.com/community/leave.bml")){
	    style = true;
        }
    }

    if (strprefix(url, "http://") && url.indexOf(".livejournal.com/") > 0) {
	var middle, mpos, list;
	mpos = url.indexOf(".livejournal.com/");
	middle = url.substr(7, mpos-7);
        /* list of livejournal.com hostnames that _don't_ need
	 * the style=mine suffix. Most of these are special things that
	 * aren't usernames; `simont' is also included here. */
        list = "/pics/userpic/lists/userpic-origin/files/stat/" +
            "bigip/stat-origin/img/status/mail/post/www/ljdev/" +
            "userpic-voxel/wwwstat/files-origin/simont/";
	if (middle.indexOf(".") < 0 && list.indexOf("/"+middle+"/") < 0)
	    style = true;
    }

    if (style && url.indexOf("?style=") < 0 && url.indexOf("&style=") < 0) {
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

    return url;
}

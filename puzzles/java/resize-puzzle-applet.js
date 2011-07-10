function initResizablePuzzleApplet() {
    var applet = document.getElementById("applet");
    var e = document.getElementById("eresize");
    var s = document.getElementById("sresize");
    var se = document.getElementById("seresize");

    state = { resizing: 0, xbase: 0, ybase: 0 };

    e.onmousedown = function(event) {
        state.xbase = 2*event.pageX - applet.width;
        state.resizing = 1;
    }
    s.onmousedown = function(event) {
        state.ybase = event.pageY - applet.height;
        state.resizing = 2;
    }
    se.onmousedown = function(event) {
        state.xbase = 2*event.pageX - applet.width;
        state.ybase = event.pageY - applet.height;
        state.resizing = 3;
    }

    document.onmousemove = function(event) {
        if (state.resizing & 1) {
            var newwidth = 2*event.pageX - state.xbase;
            applet.width = newwidth;
            s.style.width = newwidth + "px";
        }
        if (state.resizing & 2) {
            var newheight = event.pageY - state.ybase;
            applet.height = newheight;
            e.style.height = newheight + "px";
        }
    }

    document.onmouseup = function(event) {
        state.resizing = 0;
    }
}

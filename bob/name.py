# The name of my build system, and things associated with it. (I'm
# prepared to need to rename it at short notice, so I'm making it
# easy to do so...)

name = "bob"

server = name + "-delegate-server"
server_banner = "---" + server + "---"

conffile = "." + name + "/config"

# Class describing a bookmarks collection.

class Bookmarks:
    def __init__(self):
	self.things = []

    def heading(self, level, title, localdata):
	self.things.append((level, "heading", title, localdata))

    def bookmark(self, level, name, url, localdata):
	self.things.append((level, "bookmark", name, url, localdata))

    def separator(self, level, localdata):
	self.things.append((level, "separator", localdata))

    def output_hierarchy(self):
	return self.things

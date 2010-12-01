#!/usr/bin/env python

import pygtk
pygtk.require('2.0')
import gtk

from qmf.console import Session

gridclasses = [ 'master', 'collector', 'negotiator', 'slot', 'scheduler', 'jobserver', 'submitter', 'submission' ]

class QmfDashboard:

	ui = '''<ui>
		<menubar name="MenuBar">
			<menu action="File">
			<menuitem action="Add New Broker"/>
			<menuitem action="Add Recent Broker"/>
			<menuitem action="Quit"/>
			</menu>
		</menubar>
	</ui>'''

	def __init__(self):
		self.session = Session()
		self.brokers = {}
		window = gtk.Window(gtk.WINDOW_TOPLEVEL)
		window.set_title(self.__class__.__name__)
		window.set_size_request(500, 500)
		window.connect("destroy", lambda w: self.menu_quit())
		self.ui_manager = gtk.UIManager()

		vbox = gtk.VBox(False, 0)
		window.add(vbox)

		filemenu = self.build_file_menu()
		vbox.pack_start(filemenu, False)

		self.build_broker_treeview()
		vbox.pack_start(self.treeview, False)

		self.treeview.show()
		filemenu.show()
		vbox.show()
		window.show()
		
	def menu_add_broker(self, widget=None, event=None, data=None):
		dialog = gtk.MessageDialog(None, gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
			gtk.MESSAGE_INFO, gtk.BUTTONS_OK_CANCEL, None)
		entry = gtk.Entry()
		entry.set_text('amqp://localhost:5672')

		hbox = gtk.HBox()
		hbox.pack_start(gtk.Label("URL:"), False, 5, 5)
		hbox.pack_end(entry)

		dialog.format_secondary_markup("Please enter the URL of a Qpid broker")

		dialog.vbox.pack_end(hbox, True, True, 0)
		dialog.show_all()

		dialog.run()

		text = entry.get_text()
		dialog.destroy()

		broker = None
		try:
			broker = self.session.addBroker(text)
		except:
			print "Connection to ", text, " failed"
		self.brokers[text] = broker
		parent = self.add_broker_in_tree(broker)
		self.update_objects_in_tree(broker, parent)

	def update_objects_in_tree(self, broker, broker_parent):
		for gridclass in gridclasses:
			qmfobjects = self.session.getObjects(_class=gridclass, _package='com.redhat.grid')
			class_parent = self.treestore.append(broker_parent, [gridclass])
			for qmfobject in qmfobjects:
				self.treestore.append(class_parent, [qmfobject.Name])

	def add_broker_in_tree(self, broker):
		return self.treestore.append(None, [broker.getUrl()])

	def menu_quit(self, widget=None, event=None, data=None):
		try:
			for (k,b) in self.brokers.iteritems():
				self.session.delBroker(b)
		except:
			print "Error deleting brokers"
		gtk.main_quit()

	def build_file_menu(self):
		action_group = gtk.ActionGroup('File ActionGroup')

		action_group.add_actions(
		[
		("File",None,"File","F",None,None),
		("Add New Broker",None,"Add New Broker","A",None, self.menu_add_broker),
		("Add Recent Broker",None,"Add Recent Broker","R",None, self.menu_add_broker),
		("Quit",None,"Quit","Q",None,self.menu_quit)
		]
		)

		self.ui_manager.insert_action_group(action_group, 0)

		self.ui_manager.add_ui_from_string(self.ui)
		menubar = self.ui_manager.get_widget('/MenuBar')
		return menubar

	def build_broker_treeview(self):
		self.treestore = gtk.TreeStore(str)
		self.treeview = gtk.TreeView(self.treestore)
		tvcolumn = gtk.TreeViewColumn('Brokers')
		
		self.treeview.append_column(tvcolumn)
		cell = gtk.CellRendererText()
		tvcolumn.pack_start(cell, True)
		tvcolumn.add_attribute(cell, 'text', 0)
		self.treeview.set_search_column(0)
		tvcolumn.set_sort_column_id(0)
		self.treeview.set_reorderable(True)

	def main(self):
		gtk.main()

if __name__ == "__main__":
	dashboard = QmfDashboard()
	dashboard.main()
